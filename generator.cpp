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

                    int64_t elements_count = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(elements_count_val)) {
                        if (CI->getBitWidth() <= 32)
                            elements_count = CI->getSExtValue();
                    }

                    switch (arr->value_type) {
                        case Node::integer:
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
                        case Node::floating:
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
            else { // new variable
                switch (n->value_type) {
                    case Node::integer:
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
                    case Node::floating:
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
                case Node::integer:
                    table.insert(
                            std::pair<std::string, Value *>(n->var_name,
                                                            builder->CreateAlloca(
                                                                    Type::getInt32Ty(context),
                                                                    nullptr,
                                                                    n->var_name + "_ptr")
                            )
                    );
                    break;
                case Node::floating:
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
            Value *val = stack.top();
            stack.pop();
            builder->CreateStore(val, table.at(n->var_name)); // set it

            break;
        }
        case Node::DELETE:
            table.erase(n->var_name);
            break;
        case Node::VAR: { // push the value of the variable to the top of the stack
            stack.push(builder->CreateLoad(Type::getInt32Ty(context), table.at(n->var_name), n->var_name));
            break;
        }
        case Node::ARRAY_ACCESS: {
            generate(n->o1); // get number of element to access
            Value *elements_count_val = stack.top();
            stack.pop();

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
        case Node::CONST: {
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
        case Node::ADD: {
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            Value *sum = builder->CreateAdd(left, right, "add");
            stack.push(sum);

            break;
        }
        case Node::SUB: {
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            Value *sum = builder->CreateSub(left, right, "sub");
            stack.push(sum);

            break;
        }
        case Node::MUL: {
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            Value *sum = builder->CreateMul(left, right, "mul");
            stack.push(sum);

            break;
        }
        case Node::DIV: {
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            Value *sum = builder->CreateSDiv(left, right, "div");
            stack.push(sum);

            break;
        }
        case Node::LESS_TEST: { // <
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpSLT(left, right, "eq_cmp"));
            break;
        }
        case Node::LESS_IS_TEST: { // <=
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpSLE(left, right, "eq_cmp"));
            break;
        }
        case Node::MORE_TEST: { // >
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpSGT(left, right, "sgt_cmp"));
            break;
        }
        case Node::MORE_IS_TEST: { // >=
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpSGE(left, right, "sge_cmp"));
            break;
        }
        case Node::IS_TEST: { // is
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpEQ(left, right, "eq_cmp"));
            break;
        }
        case Node::IS_NOT_TEST: { // is not
            generate(n->o1);
            Value *left = stack.top();
            stack.pop();

            generate(n->o2);
            Value *right = stack.top();
            stack.pop();

            stack.push(builder->CreateICmpNE(left, right, "ne_cmp"));
            break;
        }
        case Node::SET: {
            generate(n->o1); // generate new value
            Value *val = stack.top();
            stack.pop();

            if (n->o2 != NULL) { // change value of array's element
                generate(n->o2);
                Value *elements_count_val = stack.top();
                stack.pop();

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
                );
            } else builder->CreateStore(val, table.at(n->var_name)); // just variable

            break;
        }
        case Node::IF: {
            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent);
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock);

            builder->SetInsertPoint(thenBlock);
            generate(n->o2); // generate then body

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock);

            break;
        }
        case Node::ELSE: {
            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent);
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock);

            builder->SetInsertPoint(thenBlock);
            generate(n->o2); // generate then body

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);
            generate(n->o3); // generate else body

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock);

            break;
        }
        case Node::DO: {
            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);
            generate(n->o1); // generate body

            generate(n->o2); // generate condition
            Value *condition = stack.top();
            stack.pop();

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock);

            break;
        }
        case Node::WHILE: {
            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);
            generate(n->o2); // generate body

            generate(n->o1); // generate condition
            Value *condition = stack.top();
            stack.pop();

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock);

            break;
        }
        case Node::REPEAT: { // FIXME
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

            generate(n->o1); // generate condition of the loop
            Value *times = stack.top();
            stack.pop();

            // increment value of counter by new iteration of cycle
            Value *counter = table.at("index");
            Value *counter_val = builder->CreateLoad(Type::getInt32Ty(context), counter, "index");
            Value *incr = builder->CreateAdd(counter_val, ConstantInt::get(Type::getInt32Ty(context),
                                                                           APInt(32, 1)), "incr");
            builder->CreateStore(incr, table.at("index"));

            Value *condition = builder->CreateICmpSLT(incr, times, "condition"); // check condition

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock);

            builder->SetInsertPoint(afterBlock);
            builder->CreateStore(ConstantInt::get(Type::getInt32Ty(context), APInt(32, 0)),
                                 table.at("index")); // zeroize the counter after all iterations of cycle

            break;
        }
        case Node::FUNCTION_DEFINE: {
            FunctionType *type;

            // generate arguments
            std::vector<Type *> args_types;
            std::vector<std::string> args_names;
            for (auto &iterator : n->o1->args) {
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

            for (auto &Arg : func->args()) // erase arguments' allocators from the table
                table.erase(Arg.getName().str());

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
            generate(n->o1);

            builder->CreateRet(stack.top());
            stack.pop();
            stack.pop();

            break;
        case Node::PRINT: {
            if (!io_using)
                use_io();

            generate(n->o1);
            std::vector<Value*> args;
            Value *format;

            if (stack.top()->getType()->isIntegerTy(32))
                format = builder->CreateGlobalStringPtr("%i\n");
            else if (stack.top()->getType()->isFloatTy())
                format = builder->CreateGlobalStringPtr("%f\n");
            else
                format = builder->CreateGlobalStringPtr("%s\n");

            args.push_back(format);
            args.push_back(stack.top());
            stack.pop();
            builder->CreateCall(printf, args);

            break;
        }
        case Node::INPUT: {
            if (!io_using)
                use_io();

            std::vector<Value*> args;
            Value *format = builder->CreateGlobalStringPtr("%d");

            args.push_back(format);
            args.push_back(table.at(n->var_name));
            builder->CreateCall(scanf, args);

            break;
        }
        case Node::PROG:
            generate(n->o1);
            break;
    }
}
