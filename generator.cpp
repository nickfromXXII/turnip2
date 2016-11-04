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

    printfArgs.push_back(Type::getInt8PtrTy(context)); // create the prototype of printf function
    printfType = FunctionType::get(Type::getInt32Ty(context), printfArgs, true);
    printf = module->getOrInsertFunction("printf", printfType);

    scanfArgs.push_back(Type::getInt8PtrTy(context)); // create the prototype of scanf function
    scanfType = FunctionType::get(Type::getInt32Ty(context), scanfArgs, true);
    scanf = module->getOrInsertFunction("scanf", scanfType);
}

void Generator::generate(Node *n) {
    switch(n->kind) {
        case Node::NEW: {
            if (n->o1 != NULL) { // new array
                if (n->o1->kind == Node::ARRAY) {
                    Node *arr = n->o1;
                    generate(arr->o1);
                    Value *elements_count_val = stack.top();
                    stack.pop();

                    // get number of elements in array
                    int64_t elements_count = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(elements_count_val)) {
                        if (CI->getBitWidth() <= 32)
                            elements_count = CI->getSExtValue();
                    }

                    switch (arr->value_type) {
                        case Node::integer: // integer array
                            table.insert(
                                    std::pair<std::string, Value *>(
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
                        case Node::floating: // float array
                            table.insert(
                                    std::pair<std::string, Value *>(
                                            n->var_name,
                                            builder->CreateAlloca(
                                                    ArrayType::get(
                                                            Type::getFloatTy(context),
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
                    case Node::integer: // int variable
                        table.insert(
                                std::pair<std::string, Value *>(
                                        n->var_name,
                                        builder->CreateAlloca(
                                                Type::getInt32Ty(context),
                                                nullptr,
                                                n->var_name + "_ptr")
                                )
                        );
                        break;
                    case Node::floating: // float variable
                        table.insert(
                                std::pair<std::string, Value *>(
                                        n->var_name,
                                        builder->CreateAlloca(
                                                Type::getFloatTy(context),
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
                case Node::integer: // integer variable
                    table.insert(
                            std::pair<std::string, Value *>(n->var_name,
                                                            builder->CreateAlloca(
                                                                    Type::getInt32Ty(context),
                                                                    nullptr,
                                                                    n->var_name + "_ptr")
                            )
                    );
                    break;
                case Node::floating: // float variable
                    table.insert(
                            std::pair<std::string, Value *>(n->var_name,
                                                            builder->CreateAlloca(
                                                                    Type::getFloatTy(context),
                                                                    nullptr,
                                                                    n->var_name + "_ptr")
                            )
                    );
                    break;
            }

            generate(n->o1); // generate initial value
            Value *val = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack
            builder->CreateStore(val, table.at(n->var_name)); // set it

            break;
        }
        case Node::DELETE:
            table.erase(n->var_name); // erase pointer from the table
            break;
        case Node::VAR: { // push value of the variable to the stack
            stack.push(builder->CreateLoad(Type::getInt32Ty(context), table.at(n->var_name), n->var_name));
            break;
        }
        case Node::FUNCTION_CALL: { // generate function's call
            Function *callee = functions.at(n->var_name); // get the function's prototype

            if (callee->arg_size() != n->call_args.size()) // check number of arguments in prototype and in calling
                throw std::string(
                        "Invalid number arguments (" +
                                std::to_string(n->call_args.size()) +
                                ") , expected " +
                                std::to_string(callee->arg_size())
                );

            std::vector<Value *> args; // generate values of arguments
            for (auto &&arg : n->call_args) {
                generate(arg); // generate value
                args.push_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context))
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            else
                call = builder->CreateCall(callee, args, n->var_name+"_call");

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
                case Node::string: // str constant
                    stack.push(builder->CreateGlobalStringPtr(n->str_val));
                    break;
                case Node::integer: // integer constant
                    stack.push(ConstantInt::get(context, APInt(32, static_cast<uint64_t>(n->int_val))));
                    break;
                case Node::floating: // float constant
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

            stack.push(builder->CreateAdd(left, right)); // push generated addition to the stack
            break;
        }
        case Node::SUB: { // generate subtraction
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateSub(left, right)); // push generated subtraction to the stack
            break;
        }
        case Node::MUL: { // generate multiplication
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateMul(left, right)); // push generated multiplication to the stack
            break;
        }
        case Node::DIV: { // generate division
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateSDiv(left, right)); // push generated division to the stack
            break;
        }
        case Node::LESS_TEST: { // <
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpSLT(left, right)); // push generated test to the stack
            break;
        }
        case Node::LESS_IS_TEST: { // <=
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpSLE(left, right)); // push generated test to the stack
            break;
        }
        case Node::MORE_TEST: { // >
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpSGT(left, right)); // push generated test to the stack
            break;
        }
        case Node::MORE_IS_TEST: { // >=
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpSGE(left, right)); // push generated test to the stack
            break;        }
        case Node::IS_TEST: { // is
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpEQ(left, right)); // push generated test to the stack
            break;
        }
        case Node::IS_NOT_TEST: { // is not
            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.push(builder->CreateICmpNE(left, right)); // push generated test to the stack
            break;
        }
        case Node::SET: { // generate an update variable's or array element's value
            generate(n->o1); // generate new value
            Value *val = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            if (n->o2 != NULL) { // change value of array's element
                generate(n->o2); // generate the number of element
                Value *elements_count_val = stack.top(); // take it from the stack
                stack.pop(); // erase it from the stack

                Value *arr_ptr = table.at(n->var_name); // get array's pointer
                builder->CreateStore(
                        val,
                        builder->CreateGEP( // get element's pointer
                                arr_ptr,
                                {
                                        ConstantInt::get(Type::getInt32Ty(context), 0),
                                        elements_count_val
                                },
                                n->var_name
                        )
                ); // update the value
            } else builder->CreateStore(val, table.at(n->var_name)); // update value of variable

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
                table.insert(std::pair<std::string, Value *>("index", builder->CreateAlloca(Type::getInt32Ty(context),
                                                                                            nullptr,
                                                                                            "index_ptr")));
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
            for (auto &iterator : n->o1->def_args) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    
                switch (iterator.second) {
                    case Node::integer:
                        args_types.push_back(Type::getInt32Ty(context));
                        break;
                    case Node::floating:
                        args_types.push_back(Type::getFloatTy(context));
                        break;
                }
                args_names.push_back(iterator.first);
            }

            switch (n->value_type) { // set type of the function
                case Node::integer:
                    type = FunctionType::get(Type::getInt32Ty(context), args_types, false);
                    break;
                case Node::floating:
                    type = FunctionType::get(Type::getFloatTy(context), args_types, false);
                    break;
                default:
                    type = FunctionType::get(Type::getVoidTy(context), args_types, false);
            }
            Function *func = Function::Create(type, Function::ExternalLinkage, n->var_name, module);
            functions.insert(std::pair<std::string, Function*>(n->var_name, func));

            BasicBlock *entry = BasicBlock::Create(context, "entry", func);
            builder->SetInsertPoint(entry); // set new insert block
            currentBlock = entry;

            unsigned long idx = 0;
            for (auto &Arg : func->args()) { // create pointers to arguments of the function
                std::string name = args_names.at(idx++);
                Arg.setName(name);

                // insert argument's allocator to the table
                table.insert(std::pair<std::string, Value *>(name, builder->CreateAlloca(
                        Arg.getType(),
                        nullptr,
                        name+"_ptr"))
                );
                builder->CreateStore(&Arg, table.at(name)); // store the value of argument to allocator
            }

            generate(n->o2); // generate body of the function

            if (!func->getAttributes().hasAttribute(0, "ret")) {
                switch (n->value_type) { // create default return value
                    case Node::integer:
                        builder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
                        break;
                    case Node::floating:
                        builder->CreateRet(ConstantFP::get(Type::getFloatTy(context), 0.0));
                        break;
                    default:
                        builder->CreateRetVoid();
                }
            }

            for (auto &Arg : func->args()) // erase arguments' allocators from the table
                table.erase(Arg.getName());

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
            if (!io_using)
                use_io();

            generate(n->o1); // generate the value to print
            std::vector<Value*> args;

            Value *format;
            if (stack.top()->getType()->isIntegerTy(32)) // print integer
                format = builder->CreateGlobalStringPtr("%i\n");
            else if (stack.top()->getType()->isFloatTy()) // print float
                format = builder->CreateGlobalStringPtr("%f\n");
            else // print string
                format = builder->CreateGlobalStringPtr("%s\n");

            args.push_back(format);
            args.push_back(stack.top()); // get the value from the stack
            stack.pop(); // erase it from the stack
            builder->CreateCall(printf, args); // call prinf

            break;
        }
        case Node::INPUT: {
            if (!io_using)
                use_io();

            std::vector<Value*> args;
            Value *format = builder->CreateGlobalStringPtr("%d");

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
