//
// Created by NEzyaka on 29.09.16.
//

#include <iostream>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/InstrTypes.h>
#include "generator.h"

void Generator::use_io() {
    io_using = true;

    table.insert(std::make_pair("int_out_format", builder->CreateGlobalStringPtr("%i\n", "int_out_format")));
    table.insert(std::make_pair("float_out_format", builder->CreateGlobalStringPtr("%f\n", "float_out_format")));
    table.insert(std::make_pair("str_out_format", builder->CreateGlobalStringPtr("%s\n", "str_out_format")));

    printfArgs.push_back(Type::getInt8PtrTy(context)); // create the prototype of printf function
    printfType = FunctionType::get(Type::getInt32Ty(context), printfArgs, true);
    printf = module->getOrInsertFunction("printf", printfType);

    table.insert(std::make_pair("float_in_format", builder->CreateGlobalStringPtr("%d", "float_in_format")));

    scanfArgs.push_back(Type::getInt8PtrTy(context)); // create the prototype of scanf function
    scanfType = FunctionType::get(Type::getInt32Ty(context), scanfArgs, true);
    scanf = module->getOrInsertFunction("scanf", scanfType);
}

void Generator::generate(std::shared_ptr<Node> n) {
    switch(n->kind) {
        case Node::NEW: {
            if (n->o1 != NULL) { // new array
                if (n->o1->kind == Node::ARRAY) {
                    std::shared_ptr<Node> arr = n->o1;
                    generate(arr->o1);
                    Value *elements_count_val = stack.top();
                    stack.pop();

                    // get number of elements in array
                    int64_t elements_count = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(elements_count_val)) {
                        if (CI->getBitWidth() <= 32) {
                            elements_count = CI->getSExtValue();
                        }
                    }

                    switch (arr->value_type) {
                        case Node::INTEGER: // integer array
                            table.insert(
                                    std::make_pair(
                                            n->var_name,
                                            builder->CreateAlloca(
                                                    ArrayType::get(
                                                            Type::getInt32Ty(context),
                                                            static_cast<uint64_t>(elements_count)
                                                    ),
                                                    nullptr,
                                                    n->var_name + "_ptr")
                                    )
                            );
                            break;
                        case Node::FLOATING: // float array
                            table.insert(
                                    std::make_pair(
                                            n->var_name,
                                            builder->CreateAlloca(
                                                    ArrayType::get(
                                                            Type::getDoubleTy(context),
                                                            static_cast<uint64_t>(elements_count)
                                                    ),
                                                    nullptr,
                                                    n->var_name + "_ptr"
                                            )
                                    )
                            );
                            break;
                        case Node::USER: // float array
                            table.insert(
                                    std::make_pair(
                                            n->var_name,
                                            builder->CreateAlloca(
                                                    ArrayType::get(
                                                            PointerType::get(user_types.at(n->user_type).first, 0),
                                                            static_cast<uint64_t>(elements_count)
                                                    ),
                                                    nullptr,
                                                    n->var_name + "_ptr"
                                            )
                                    )
                            );
                            break;
                    }
                }
            }
            else { // just variable
                switch (n->value_type) {
                    case Node::INTEGER: // int variable
                        table.insert(
                                std::make_pair(
                                        n->var_name,
                                        builder->CreateAlloca(
                                                Type::getInt32Ty(context),
                                                nullptr,
                                                n->var_name + "_ptr")
                                )
                        );
                        break;
                    case Node::FLOATING: // float variable
                        table.insert(
                                std::make_pair(
                                        n->var_name,
                                        builder->CreateAlloca(
                                                Type::getDoubleTy(context),
                                                nullptr,
                                                n->var_name + "_ptr")
                                )
                        );
                        break;
                    case Node::USER: // user-type variable
                        table.insert(
                                std::make_pair(
                                        n->var_name,
                                        builder->CreateAlloca(
                                                user_types.at(n->user_type).first,
                                                nullptr,
                                                n->var_name + "_ptr")
                                )
                        );
                        break;
                }
            }
            break;
        }
        case Node::INIT: { // create and initialize the variable
            switch (n->value_type) {
                case Node::INTEGER: // integer variable
                    table.insert(
                            std::make_pair(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            Type::getInt32Ty(context),
                                            nullptr,
                                            n->var_name + "_ptr")
                            )
                    );
                    break;
                case Node::FLOATING: // float variable
                    table.insert(std::make_pair(
                            n->var_name,
                            builder->CreateAlloca(
                                    Type::getDoubleTy(context),
                                    nullptr,
                                    n->var_name + "_ptr")
                                 )
                    );
                    break;
                case Node::USER: // user-type variable
                    table.insert(
                            std::make_pair(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            user_types.at(n->user_type).first,
                                            nullptr,
                                            n->var_name + "_ptr")
                            )
                    );
                    break;
            }

            if (n->o1->value_type == Node::USER && n->o1->kind == Node::VAR) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type)
                    throw std::string("types of objects '"
                                      + n->var_name
                                      + "' ("
                                      + std::string(
                            n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int"
                                                                                                           : "float"))
                                      + ") and '"
                                      + n->o1->var_name
                                      + "' ("
                                      + std::string(
                            n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER
                                                                                    ? "int" : "float"))
                                      + ") does not match!"
                    );

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        table.at(n->o1->var_name),
                        module->getDataLayout().getTypeAllocSize(table.at(n->o1->var_name)->getType()),
                        table.at(n->o1->var_name)->getPointerAlignment(module->getDataLayout())
                );
            }
            else {
                if (n->o1->kind == Node::OBJECT_CONSTRUCT)
                    stack.push(table.at(n->var_name));

                generate(n->o1); // generate initial value

                if (n->o1->kind == Node::OBJECT_CONSTRUCT) // object constructors are void functions
                    break;

                Value *val = stack.top(); // take it from the stack
                stack.pop(); // erase it from the stack

                if (table.at(n->var_name)->getType() != val->getType()) {
                    if (table.at(n->var_name)->getType()->isIntegerTy(32) && val->getType()->isDoubleTy()) {
                        Value *buf = val;
                        val = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                    } else if (table.at(n->var_name)->getType()->isDoubleTy() && val->getType()->isIntegerTy(32)) {
                        Value *buf = val;
                        val = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                    }
                }

                builder->CreateStore(val, table.at(n->var_name));
            }
            break;
        }
        case Node::DELETE:
            table.erase(n->var_name); // erase pointer from the table
            break;
        case Node::VAR: { // push value of the variable to the stack
            Type *type;

            auto ptr = table.at(n->var_name);
            for (auto &&user_type : user_types) {
                auto iter = std::find(user_type.second.second.begin(), user_type.second.second.end(), n->var_name);
                if (iter != user_type.second.second.end()) {
                    ptr = builder->CreateGEP(
                            user_type.second.first,
                            builder->CreateLoad(table.at("this")),
                            {
                                    ConstantInt::get(Type::getInt32Ty(context), 0),
                                    ConstantInt::get(
                                            Type::getInt32Ty(context),
                                            static_cast<uint64_t>(std::distance(user_type.second.second.begin(), iter))
                                    )
                            },
                            n->var_name
                    );
                }
            }

            if (table.at(n->var_name)->getType() == Type::getInt32PtrTy(context)) {
                type = Type::getInt32Ty(context);
            } else if (table.at(n->var_name)->getType() == Type::getDoublePtrTy(context)) {
                type = Type::getDoubleTy(context);
            } else {
                stack.push(table.at(n->var_name));
                break;
            }

            stack.push(builder->CreateLoad(type, ptr, n->var_name));
            break;
        }
        case Node::PROPERTY_ACCESS: {
            Value *ptr;
            auto user_type = user_types.at(n->user_type);
            auto iter = std::find(std::begin(user_type.second), std::end(user_type.second), n->property_name);

            Value* src = table.at(n->var_name);
            while (cast<PointerType>(src->getType())->getElementType()->isPointerTy()) {
                Value *temp = builder->CreateLoad(src);
                src = temp;
            }

            if (iter != std::end(user_type.second)) {
                ptr = builder->CreateGEP(
                        user_type.first,
                        src,
                        {
                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                ConstantInt::get(
                                        Type::getInt32Ty(context),
                                        static_cast<uint64_t>(std::distance(begin(user_type.second), iter))
                                )
                        },
                        n->var_name
                );
            }
            stack.push(builder->CreateLoad(ptr, n->property_name));
            break;
        }
        case Node::FUNCTION_CALL: { // generate function's call
            Function *callee = functions.at(n->var_name); // get the function's prototype

            if (callee->arg_size() != n->func_call_args.size()) { // check number of arguments in prototype and in calling
                throw std::string(
                        "Invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        ") , expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.push_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"_call");
            }

            stack.push(call); // push call to the stack
            break;
        }
        case Node::METHOD_CALL: { // generate function's call
            Function *callee = functions.at(n->property_name); // get the function's prototype

            if (callee->arg_size() != n->func_call_args.size() && callee->arg_size()-n->func_call_args.size()>1) { // check number of arguments in prototype and in calling
                throw std::string(
                        " Invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        "), expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            if (cast<PointerType>(table.at(n->var_name)->getType())->getElementType()->isPointerTy())
                args.push_back(builder->CreateLoad(table.at(n->var_name)));
            else args.push_back(table.at(n->var_name));

            stack.pop();
            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.push_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"_call");
            }

            call->dump();
            stack.push(call); // push call to the stack
            break;
        }
        case Node::OBJECT_CONSTRUCT: { // generate function's call
            Function *callee = functions.at(n->var_name); // get the function's prototype

            if (callee->arg_size() != n->func_call_args.size() && callee->arg_size()-n->func_call_args.size()>1) { // check number of arguments in prototype and in calling
                throw std::string(
                        " Invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        "), expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            args.push_back(stack.top());
            stack.pop();
            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.push_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"_call");
            }

            stack.push(call); // push call to the stack
            break;
        }
        case Node::ARRAY_ACCESS: { // generate access to array's element
            generate(n->o1); // get number of element to access
            Value *elements_count_val = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            Value *arr_ptr = table.at(n->var_name); // get array's pointer
            stack.push(
                    builder->CreateLoad(
                            builder->CreateGEP( // get element's pointer
                                    arr_ptr,
                                    {
                                            ConstantInt::get(Type::getInt32Ty(context), 0),
                                            elements_count_val
                                    },
                                    n->var_name
                            ),
                            n->var_name
                    )
            );
            break;
        }
        case Node::CONST: { // generate constant value
            switch (n->value_type) {
                case Node::STRING: // str constant
                    stack.push(builder->CreateGlobalStringPtr(n->str_val));
                    break;
                case Node::INTEGER: { // integer constant
                    stack.push(ConstantInt::get(context, APInt(32, static_cast<uint64_t>(n->int_val))));
                    break;
                }
                case Node::FLOATING: // float constant
                    stack.push(ConstantFP::get(context, APFloat(n->float_val)));
                    break;
            }
            break;
        }
        case Node::ADD: { // generate addition
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToUI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateAdd(left, right)); // push generated addition to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.push(builder->CreateFAdd(left, right)); // push generated addition to the stack
            }
            break;
        }
        case Node::SUB: { // generate subtraction
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToUI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateSub(left, right)); // push generated subtraction to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.push(builder->CreateFSub(left, right)); // push generated subtraction to the stack
            }
            break;
        }
        case Node::MUL: { // generate multiplication
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToUI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateMul(left, right)); // push generated multiplication to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.push(builder->CreateFMul(left, right)); // push generated multiplication to the stack
            }
            break;
        }
        case Node::DIV: { // generate division
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateSDiv(left, right)); // push generated division to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                stack.push(builder->CreateFDiv(left, right)); // push generated division to the stack
            }
            break;
        }
        case Node::LESS_TEST: { // < FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpSLT(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpULT(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::LESS_IS_TEST: { // <= FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpSLE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpULE(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::MORE_TEST: { // > FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpSGT(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpUGT(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::MORE_IS_TEST: { // >= FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpSGE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpUGE(left, right)); // push generated test to the stack
            }
            break;        }
        case Node::IS_TEST: { // is FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpEQ(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpUEQ(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::IS_NOT_TEST: { // is not FIXME floating point test
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.push(builder->CreateICmpNE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.push(builder->CreateFCmpUNE(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::SET: { // generate an update variable's or array element's value
            if (n->o1->value_type == Node::USER && n->o1->kind == Node::VAR) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type)
                    throw std::string("types of objects '"
                                      + n->var_name
                                      + "' ("
                                      + std::string(
                            n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int"
                                                                                                           : "float"))
                                      + ") and '"
                                      + n->o1->var_name
                                      + "' ("
                                      + std::string(
                            n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER
                                                                                    ? "int" : "float"))
                                      + ") does not match!"
                    );

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        table.at(n->o1->var_name),
                        module->getDataLayout().getTypeAllocSize(table.at(n->o1->var_name)->getType()),
                        table.at(n->o1->var_name)->getPointerAlignment(module->getDataLayout())
                );
            }
            else {
                if (n->o1->kind == Node::OBJECT_CONSTRUCT)
                    stack.push(table.at(n->var_name));

                generate(n->o1); // generate new value

                if (n->o1->kind == Node::OBJECT_CONSTRUCT) // object constructors are void functions
                    break;

                Value *val = stack.top(); // take it from the stack
                stack.pop(); // erase it from the stack

                if (n->o2 != NULL) { // change value of array's element
                    generate(n->o2); // generate the number of element
                    Value *elements_count_val = stack.top(); // take it from the stack
                    stack.pop(); // erase it from the stack

                    Value *arr_ptr = table.at(n->var_name); // get array's pointer
                    Value *el_ptr = builder->CreateGEP( // get element's pointer
                            arr_ptr,
                            {
                                    ConstantInt::get(Type::getInt32Ty(context), 0),
                                    elements_count_val
                            },
                            n->var_name
                    );

                    if (el_ptr->getType() != val->getType()) {
                        if (el_ptr->getType() == Type::getInt32PtrTy(context) && val->getType()->isDoubleTy()) {
                            Value *buf = val;
                            val = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                        } else if (el_ptr->getType() == Type::getDoublePtrTy(context) &&
                                   val->getType()->isIntegerTy(32)) {
                            Value *buf = val;
                            val = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                        }
                    }
                    builder->CreateStore(val, el_ptr); // update value of variable

                } else {
                    if (table.at(n->var_name)->getType() != val->getType()) {
                        if (table.at(n->var_name)->getType() == Type::getInt32PtrTy(context) &&
                            val->getType()->isDoubleTy()) {
                            Value *buf = val;
                            val = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                        } else if (table.at(n->var_name)->getType() == Type::getDoublePtrTy(context) &&
                                   val->getType()->isIntegerTy(32)) {
                            Value *buf = val;
                            val = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                        }
                    }

                    auto ptr = table.at(n->var_name);

                    for (auto &&user_type : user_types) {
                        auto iter = std::find(user_type.second.second.begin(), user_type.second.second.end(),
                                              n->var_name);
                        if (iter != user_type.second.second.end()) {
                            ptr = builder->CreateGEP(
                                    user_type.second.first,
                                    builder->CreateLoad(table.at("this")),
                                    {
                                            ConstantInt::get(Type::getInt32Ty(context), 0),
                                            ConstantInt::get(
                                                    Type::getInt32Ty(context),
                                                    static_cast<uint64_t>(std::distance(user_type.second.second.begin(),
                                                                                        iter))
                                            )
                                    },
                                    n->var_name
                            );
                        }
                    }

                    builder->CreateStore(val, ptr); // update value of variable
                }
            }

            break;
        }
        case Node::CLASS_DEFINE: {
            std::vector<Type *> properties_types;
            std::vector<std::string> properties_names;
            for (auto &&defProperty : n->class_def_properties) { // generate properties first
                if (defProperty.second.second->kind == Node::NEW) {
                    properties_names.push_back(defProperty.first);
                    switch (defProperty.second.second->value_type) {
                        case Node::INTEGER:
                            properties_types.push_back(Type::getInt32Ty(context));
                            break;
                        case Node::FLOATING:
                            properties_types.push_back(Type::getDoubleTy(context));
                            break;
                    }
                    table.insert(std::make_pair(defProperty.first, builder->CreateAlloca(properties_types.back(), nullptr)));
                }
            }

            StructType* class_type = StructType::create(context, n->var_name);
            class_type->setBody(properties_types);
            user_types.insert(std::make_pair(n->var_name, std::make_pair(class_type, properties_names)));

            for (auto &&defProperty : n->class_def_properties) { // then, generate methods
                if (defProperty.second.second->kind == Node::FUNCTION_DEFINE) {
                    FunctionType *type;

                    // generate arguments
                    std::vector<Type *> args_types;
                    std::vector<std::string> args_names;

                    args_types.push_back(PointerType::get(class_type, 0));
                    args_names.push_back("this");

                    for (auto &iterator : defProperty.second.second->o1->func_def_args) {
                        switch (iterator.second->value_type) {
                            case Node::INTEGER:
                                args_types.push_back(Type::getInt32Ty(context));
                                break;
                            case Node::FLOATING:
                                args_types.push_back(Type::getDoubleTy(context));
                                break;
                            case Node::USER:
                                args_types.push_back(PointerType::get(user_types.at(iterator.second->user_type_name).first, 0));
                                break;
                        }
                        args_names.push_back(iterator.first);
                    }

                    switch (defProperty.second.second->value_type) { // set type of the function
                        case Node::INTEGER:
                            type = FunctionType::get(Type::getInt32Ty(context), args_types, false);
                            break;
                        case Node::FLOATING:
                            type = FunctionType::get(Type::getDoubleTy(context), args_types, false);
                            break;
                        case Node::USER:
                            type = FunctionType::get(PointerType::get(class_type, 0), args_types, false);
                            break;
                        default:
                            type = FunctionType::get(Type::getVoidTy(context), args_types, false);
                    }
                    Function *func = Function::Create(type, Function::ExternalLinkage, defProperty.second.second->var_name, module);
                    functions.insert(std::make_pair(defProperty.second.second->var_name, func));

                    BasicBlock *entry = BasicBlock::Create(context, "entry", func);
                    builder->SetInsertPoint(entry); // set new insert block

                    unsigned long idx = 0;
                    for (auto &Arg : func->args()) { // create pointers to arguments of the function
                        std::string name = args_names.at(idx++);
                        Arg.setName(name);

                        // insert argument's allocator to the table
                        table.insert(std::make_pair(name, builder->CreateAlloca(
                                Arg.getType(),
                                nullptr,
                                name+"_ptr"))
                        );
                        builder->CreateStore(&Arg, table.at(name)); // store the value of argument to allocator
                    }

                    generate(defProperty.second.second->o2); // generate body of the function

                    if (!func->getAttributes().hasAttribute(0, "ret")) {
                        switch (defProperty.second.second->value_type) { // create default return value
                            case Node::INTEGER:
                                builder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
                                break;
                            case Node::FLOATING:
                                builder->CreateRet(ConstantFP::get(Type::getDoubleTy(context), 0.0));
                                break;
                            default:
                                builder->CreateRetVoid();
                        }
                    }

                    for (auto &Arg : func->args()) { // erase arguments' allocators from the table
                        table.erase(Arg.getName());
                    }
                }
            }

            for (auto &&item : properties_names) {
                table.erase(item);
            }

            break;
        }
        case Node::IF: { // generate 'if' condition without 'else' branch
            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock); // create conditional goto

            builder->SetInsertPoint(thenBlock);
            generate(n->o2); // generate the body of 'then' branch

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition

            break;
        }
        case Node::ELSE: { // generate 'if' condition with 'else' branch
            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock); // create conditional goto

            builder->SetInsertPoint(thenBlock);
            generate(n->o2); // generate the body of 'then' branch

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);
            generate(n->o3); // generate the body of 'else' branch

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition

            break;
        }
        case Node::DO: { // do-while cycle
            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);
            generate(n->o1); // generate the body

            generate(n->o2); // generate the condition
            Value *condition = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock); // set insert point to block after the condition

            break;
        }
        case Node::WHILE: { // 'while' cycle
            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);
            generate(n->o2); // generate the body

            generate(n->o1); // generate the condition
            Value *condition = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock); // set insert point to block after the condition

            break;
        }
        case Node::REPEAT: { // 'repeat' cycle FIXME
            if (table.count("index") == 0)
            {
                table.insert(std::make_pair("index", builder->CreateAlloca(Type::getInt32Ty(context), nullptr, "index_ptr")));
            }

            builder->CreateStore(ConstantInt::get(Type::getInt32Ty(context), APInt(32, 0)),
                                 table.at("index")); // zeroize the counter

            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock); // go to begin of the loop
            builder->SetInsertPoint(loopBlock);
            generate(n->o2); // generate body of the loop

            generate(n->o1); // generate the condition
            Value *times = stack.top(); // take it form the stack
            stack.pop(); // erase it from the stack

            // increment value of counter by new iteration of cycle
            Value *counter = table.at("index");
            Value *counter_val = builder->CreateLoad(Type::getInt32Ty(context), counter, "index");
            Value *incr = builder->CreateAdd(counter_val, ConstantInt::get(Type::getInt32Ty(context),
                                                                           APInt(32, 1)), "incr");
            builder->CreateStore(incr, table.at("index"));

            Value *condition = builder->CreateICmpSLT(incr, times, "condition"); // check condition

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock);

            builder->SetInsertPoint(afterBlock); // set insert point to block after the condition
            builder->CreateStore(ConstantInt::get(Type::getInt32Ty(context), APInt(32, 0)),
                                 table.at("index")); // zeroize the counter after all iterations of cycle

            break;
        }
        case Node::FUNCTION_DEFINE: { // generate function's definition
            FunctionType *type;

            // generate arguments
            std::vector<Type *> args_types;
            std::vector<std::string> args_names;
            for (auto &iterator : n->o1->func_def_args) {
                switch (iterator.second->value_type) {
                    case Node::INTEGER:
                        args_types.push_back(Type::getInt32Ty(context));
                        break;
                    case Node::FLOATING:
                        args_types.push_back(Type::getDoubleTy(context));
                        break;
                    case Node::USER:
                        args_types.push_back(PointerType::get(
                                user_types.at(iterator.second->user_type_name).first, 0)
                        );
                        break;
                }
                args_names.push_back(iterator.first);
            }

            switch (n->value_type) { // set type of the function
                case Node::INTEGER:
                    type = FunctionType::get(Type::getInt32Ty(context), args_types, false);
                    break;
                case Node::FLOATING:
                    type = FunctionType::get(Type::getDoubleTy(context), args_types, false);
                    break;
                case Node::USER:
                    type = FunctionType::get(PointerType::get(user_types.at(n->user_type).first, 0), args_types, false);
                default:
                    type = FunctionType::get(Type::getVoidTy(context), args_types, false);
            }
            Function *func = Function::Create(type, Function::ExternalLinkage, n->var_name, module);
            functions.insert(std::make_pair(n->var_name, func));

            BasicBlock *entry = BasicBlock::Create(context, "entry", func);
            builder->SetInsertPoint(entry); // set new insert block

            unsigned long idx = 0;
            for (auto &Arg : func->args()) { // create pointers to arguments of the function
                std::string name = args_names.at(idx++);
                Arg.setName(name);

                // insert argument's allocator to the table
                table.insert(
                        std::make_pair(
                                name,
                                builder->CreateAlloca(
                                        Arg.getType(),
                                        nullptr,
                                        name+"_ptr")
                        )
                );
                builder->CreateStore(&Arg, table.at(name)); // store the value of argument to allocator
            }

            generate(n->o2); // generate body of the function

            if (!func->getAttributes().hasAttribute(0, "ret")) {
                switch (n->value_type) { // create default return value
                    case Node::INTEGER:
                        builder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
                        break;
                    case Node::FLOATING:
                        builder->CreateRet(ConstantFP::get(Type::getDoubleTy(context), 0.0));
                        break;
                    default:
                        builder->CreateRetVoid();
                }
            }

            for (auto &Arg : func->args()) { // erase arguments' allocators from the table
                table.erase(Arg.getName());
            }

            break;
        }
        case Node::SEQ:
            generate(n->o1);
            generate(n->o2);
            break;
        case Node::EXPR:
            generate(n->o1);
            break;
        case Node::RETURN:
            generate(n->o1); // generate return value
            builder->CreateRet(stack.top()); // take it from the stack
            stack.pop(); // erase it from the stack

            // set the 'ret' attribute
            builder->GetInsertBlock()->getParent()->addAttribute(0, Attribute::get(context, "ret"));

            break;
        case Node::PRINT: { // print something TODO: migrate to call of prinf function
            if (!io_using) {
                use_io();
            }

            generate(n->o1); // generate the value to print
            std::vector<Value*> args;

            Value *format;
            if (stack.top()->getType()->isIntegerTy(32)) { // print integer
                format = table.at("int_out_format");
            } else if (stack.top()->getType()->isDoubleTy()) { // print float
                format = table.at("float_out_format");
            } else { // print string
                format = table.at("str_out_format");
            }

            args.push_back(format);
            args.push_back(stack.top()); // get the value from the stack
            stack.pop(); // erase it from the stack
            builder->CreateCall(printf, args); // call prinf

            break;
        }
        case Node::INPUT: {
            if (!io_using) {
                use_io();
            }

            std::vector<Value*> args;
            Value *format = table.at("float_in_format");

            args.push_back(format);
            args.push_back(table.at(n->var_name));
            builder->CreateCall(scanf, args); // call scanf

            break;
        }
        case Node::PROG:
            generate(n->o1);
            break;
    }
}
