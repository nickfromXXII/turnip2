//
// Created by NEzyaka on 29.09.16.
//

#include <iostream>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/InstrTypes.h>
#include "generator.h"

void Generator::error(unsigned line, const std::string &e) {
    throw std::string(std::to_string(line) + " -> " + e);
}

Generator::Generator(bool opt, bool genDI, const std::string &f) : optimize(opt), generateDI(genDI), file (f) {
    module = std::make_unique<Module>(file, context);
    builder = std::make_unique<IRBuilder<>>(context);

    if (generateDI) {
        dbuilder = std::make_unique<DIBuilder>(*module.get());
        compileUnit = dbuilder->createCompileUnit(
                dwarf::DW_LANG_C,
                file,
                ".",
                "turnip2",
                0,
                "",
                0
        );
    }

    if (optimize) {
        passmgr = std::make_unique<legacy::FunctionPassManager>(module.get());
        passmgr->add(createInstructionSimplifierPass());
        passmgr->add(createInstructionCombiningPass());
        passmgr->add(createReassociatePass());
        passmgr->add(createDeadCodeEliminationPass());
        passmgr->add(createDeadInstEliminationPass());
        passmgr->add(createDeadStoreEliminationPass());
        passmgr->add(createConstantHoistingPass());
        passmgr->add(createLoadCombinePass());
        passmgr->add(createAggressiveDCEPass());
        passmgr->add(createLoopLoadEliminationPass());
        passmgr->add(createConstantPropagationPass());
        passmgr->add(createCFGSimplificationPass());
        passmgr->add(createMemCpyOptPass());
        passmgr->doInitialization();
    }
}

void Generator::use_io() {
    io_using = true;

    table.emplace("int_out_format", builder->CreateGlobalStringPtr("%i\n", "int_out_format"));
    table.emplace("float_out_format", builder->CreateGlobalStringPtr("%f\n", "float_out_format"));
    table.emplace("str_out_format", builder->CreateGlobalStringPtr("%s\n", "str_out_format"));

    printfArgs.emplace_back(Type::getInt8PtrTy(context)); // create the prototype of printf function
    printfType = FunctionType::get(Type::getInt32Ty(context), printfArgs, true);
    printf = module->getOrInsertFunction("printf", printfType);

    table.emplace("float_in_format", builder->CreateGlobalStringPtr("%d", "float_in_format"));
    table.emplace("str_in_format", builder->CreateGlobalStringPtr("%255s", "str_in_format"));

    scanfArgs.emplace_back(Type::getInt8PtrTy(context)); // create the prototype of scanf function
    scanfType = FunctionType::get(Type::getInt32Ty(context), scanfArgs, true);
    scanf = module->getOrInsertFunction("scanf", scanfType);
}

void Generator::generate(const std::shared_ptr<Node>& n) {
    switch(n->kind) {
        case Node::VAR_DEF: {
            if (n->o1 != nullptr) { // new array
                if (n->o1->kind == Node::ARRAY) {
                    std::shared_ptr<Node> arr = n->o1;
                    generate(arr->o1);
                    Value *elements_count_val = stack.top();
                    stack.pop();
                    array_sizes.emplace(n->var_name, elements_count_val);

                    // get number of elements in array
                    int64_t elements_count = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(elements_count_val)) {
                        if (CI->getBitWidth() <= 32) {
                            elements_count = CI->getSExtValue();
                            if (elements_count <= 0) {
                                error(n->location.line, "cannot create array of negative or null elements");
                            }
                        }
                    }

                    switch (arr->value_type) {
                        last_vars.emplace_back(n->var_name);
                        case Node::INTEGER: { // integer array
                            table.emplace(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            ArrayType::get(
                                                    Type::getInt32Ty(context),
                                                    static_cast<uint64_t>(elements_count)
                                            ),
                                            nullptr,
                                            n->var_name + "_ptr"
                                    )
                            );
                            if (generateDI) {
                                DILocalVariable *var = dbuilder->createAutoVariable(
                                        lexical_blocks.back(),
                                        n->var_name,
                                        unit,
                                        n->location.line,
                                        dbuilder->createArrayType(
                                                static_cast<unsigned>(elements_count),
                                                module->getDataLayout().getABITypeAlignment(Type::getInt32Ty(context)),
                                                getDebugType(Type::getInt32Ty(context)),
                                                nullptr
                                        )
                                );
                                dbuilder->insertDeclare(
                                        table.at(n->var_name),
                                        var,
                                        dbuilder->createExpression(),
                                        DebugLoc::get(
                                                (n->location.line),
                                                0,
                                                lexical_blocks.back()
                                        ),
                                        builder->GetInsertBlock()
                                );
                            }
                            break;
                        }
                        case Node::FLOATING: { // float array
                            table.emplace(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            ArrayType::get(
                                                    Type::getDoubleTy(context),
                                                    static_cast<uint64_t>(elements_count)
                                            ),
                                            nullptr,
                                            n->var_name + "_ptr"
                                    )
                            );
                            if (generateDI) {
                                DILocalVariable *var = dbuilder->createAutoVariable(
                                        lexical_blocks.back(),
                                        n->var_name,
                                        unit,
                                        n->location.line,
                                        dbuilder->createArrayType(
                                                static_cast<unsigned>(elements_count),
                                                module->getDataLayout().getABITypeAlignment(Type::getDoubleTy(context)),
                                                getDebugType(Type::getDoubleTy(context)),
                                                nullptr
                                        )
                                );
                                dbuilder->insertDeclare(
                                        table.at(n->var_name),
                                        var,
                                        dbuilder->createExpression(),
                                        DebugLoc::get(
                                                (n->location.line),
                                                0,
                                                lexical_blocks.back()
                                        ),
                                        builder->GetInsertBlock()
                                );
                            }
                            break;
                        }
                        case Node::STRING: { // string array
                            table.emplace(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            ArrayType::get(
                                                    ArrayType::get(Type::getInt8Ty(context), 256),
                                                    static_cast<uint64_t>(elements_count)
                                            ),
                                            nullptr,
                                            n->var_name + "_ptr"
                                    )
                            );
                            if (generateDI) {
                                DILocalVariable *var = dbuilder->createAutoVariable(
                                        lexical_blocks.back(),
                                        n->var_name,
                                        unit,
                                        n->location.line,
                                        dbuilder->createArrayType(
                                                static_cast<unsigned>(elements_count),
                                                module->getDataLayout().getABITypeAlignment(ArrayType::get(Type::getInt8Ty(context), 256)),
                                                getDebugType(ArrayType::get(Type::getInt8Ty(context), 256)),
                                                nullptr
                                        )
                                );
                                dbuilder->insertDeclare(
                                        table.at(n->var_name),
                                        var,
                                        dbuilder->createExpression(),
                                        DebugLoc::get(
                                                (n->location.line),
                                                0,
                                                lexical_blocks.back()
                                        ),
                                        builder->GetInsertBlock()
                                );
                            }
                            break;
                        }
                        case Node::BOOL: { // bool array
                            table.emplace(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            ArrayType::get(
                                                    Type::getInt1Ty(context),
                                                    static_cast<uint64_t>(elements_count)
                                            ),
                                            nullptr,
                                            n->var_name + "_ptr"
                                    )
                            );
                            if (generateDI) {
                                DILocalVariable *var = dbuilder->createAutoVariable(
                                        lexical_blocks.back(),
                                        n->var_name,
                                        unit,
                                        n->location.line,
                                        dbuilder->createArrayType(
                                                static_cast<unsigned>(elements_count),
                                                module->getDataLayout().getABITypeAlignment(Type::getInt1Ty(context)),
                                                getDebugType(Type::getInt1Ty(context)),
                                                nullptr
                                        )
                                );
                                dbuilder->insertDeclare(
                                        table.at(n->var_name),
                                        var,
                                        dbuilder->createExpression(),
                                        DebugLoc::get(
                                                (n->location.line),
                                                0,
                                                lexical_blocks.back()
                                        ),
                                        builder->GetInsertBlock()
                                );
                            }
                            break;
                        }
                        case Node::USER: // user type array TODO debug info
                            table.emplace(
                                    n->var_name,
                                    builder->CreateAlloca(
                                            ArrayType::get(
                                                    PointerType::get(user_types.at(n->user_type).first, 0),
                                                    static_cast<uint64_t>(elements_count)
                                            ),
                                            nullptr,
                                            n->var_name + "_ptr"
                                    )
                            );
                            break;
                    }
                }
            }
            else { // just variable
                last_vars.emplace_back(n->var_name);
                switch (n->value_type) {
                    case Node::INTEGER: { // int variable
                        table.emplace(
                                n->var_name,
                                builder->CreateAlloca(
                                        Type::getInt32Ty(context),
                                        nullptr,
                                        n->var_name + "_ptr"
                                )
                        );
                        if (generateDI) {
                            DILocalVariable *var = dbuilder->createAutoVariable(
                                    lexical_blocks.back(),
                                    n->var_name,
                                    unit,
                                    n->location.line,
                                    getDebugType(Type::getInt32Ty(context))
                            );
                            dbuilder->insertDeclare(
                                    table.at(n->var_name),
                                    var,
                                    dbuilder->createExpression(),
                                    DebugLoc::get(
                                            n->location.line,
                                            0,
                                            lexical_blocks.back()
                                    ),
                                    builder->GetInsertBlock()
                            );
                        }
                        break;
                    }
                    case Node::FLOATING: { // float variable
                        table.emplace(
                                n->var_name,
                                builder->CreateAlloca(
                                        Type::getDoubleTy(context),
                                        nullptr,
                                        n->var_name + "_ptr"
                                )
                        );
                        if (generateDI) {
                            DILocalVariable *var = dbuilder->createAutoVariable(
                                    lexical_blocks.back(),
                                    n->var_name,
                                    unit,
                                    n->location.line,
                                    getDebugType(Type::getDoubleTy(context))
                            );
                            dbuilder->insertDeclare(
                                    table.at(n->var_name),
                                    var,
                                    dbuilder->createExpression(),
                                    DebugLoc::get(
                                            n->location.line,
                                            0,
                                            lexical_blocks.back()
                                    ),
                                    builder->GetInsertBlock()
                            );
                        }
                        break;
                    }
                    case Node::STRING: { // string variable
                        table.emplace(
                                n->var_name,
                                builder->CreateAlloca(
                                        ArrayType::get(Type::getInt8Ty(context), 256),
                                        nullptr,
                                        n->var_name + "_ptr")

                        );
                        if (generateDI) {
                            DILocalVariable *var = dbuilder->createAutoVariable(
                                    lexical_blocks.back(),
                                    n->var_name,
                                    unit,
                                    n->location.line,
                                    getDebugType(ArrayType::get(Type::getInt8Ty(context), 256))
                            );
                            dbuilder->insertDeclare(
                                    table.at(n->var_name),
                                    var,
                                    dbuilder->createExpression(),
                                    DebugLoc::get(
                                            n->location.line,
                                            0,
                                            lexical_blocks.back()
                                    ),
                                    builder->GetInsertBlock()
                            );
                        }
                        break;
                    }
                    case Node::BOOL: { // bool variable
                        table.emplace(
                                n->var_name,
                                builder->CreateAlloca(
                                        Type::getInt1Ty(context),
                                        nullptr,
                                        n->var_name + "_ptr"
                                )
                        );
                        if (generateDI) {
                            DILocalVariable *var = dbuilder->createAutoVariable(
                                    lexical_blocks.back(),
                                    n->var_name,
                                    unit,
                                    n->location.line,
                                    getDebugType(Type::getInt1Ty(context))
                            );
                            dbuilder->insertDeclare(
                                    table.at(n->var_name),
                                    var,
                                    dbuilder->createExpression(),
                                    DebugLoc::get(
                                            n->location.line,
                                            0,
                                            lexical_blocks.back()
                                    ),
                                    builder->GetInsertBlock()
                            );
                        }
                        break;
                    }
                    case Node::USER: // user-type variable
                        table.emplace(
                                n->var_name,
                                builder->CreateAlloca(
                                        user_types.at(n->user_type).first,
                                        nullptr,
                                        n->var_name + "_ptr"
                                )
                        );
                        break;
                }
            }
            break;
        }
        case Node::INIT: { // create and initialize the variable
            last_vars.emplace_back(n->var_name);
            switch (n->value_type) {
                case Node::INTEGER: { // integer variable
                    table.emplace(
                            n->var_name,
                            builder->CreateAlloca(
                                    Type::getInt32Ty(context),
                                    nullptr,
                                    n->var_name + "_ptr"
                            )
                    );
                    if (generateDI) {
                        DILocalVariable *var = dbuilder->createAutoVariable(
                                lexical_blocks.back(),
                                n->var_name,
                                unit,
                                n->location.line,
                                getDebugType(Type::getInt32Ty(context))
                        );
                        dbuilder->insertDeclare(
                                table.at(n->var_name),
                                var,
                                dbuilder->createExpression(),
                                DebugLoc::get(
                                        n->location.line,
                                        0,
                                        lexical_blocks.back()
                                ),
                                builder->GetInsertBlock()
                        );
                    }
                    break;
                }
                case Node::FLOATING: { // float variable
                    table.emplace(
                            n->var_name,
                            builder->CreateAlloca(
                                    Type::getDoubleTy(context),
                                    nullptr,
                                    n->var_name + "_ptr"
                            )
                    );
                    if (generateDI) {
                        DILocalVariable *var = dbuilder->createAutoVariable(
                                lexical_blocks.back(),
                                n->var_name,
                                unit,
                                n->location.line,
                                getDebugType(Type::getDoubleTy(context))
                        );
                        dbuilder->insertDeclare(
                                table.at(n->var_name),
                                var,
                                dbuilder->createExpression(),
                                DebugLoc::get(
                                        n->location.line,
                                        0,
                                        lexical_blocks.back()
                                ),
                                builder->GetInsertBlock()
                        );
                    }
                    break;
                }
                case Node::STRING: { // string variable
                    table.emplace(
                            n->var_name,
                            builder->CreateAlloca(
                                    ArrayType::get(Type::getInt8Ty(context), 256),
                                    nullptr,
                                    n->var_name + "_ptr")

                    );
                    if (generateDI) {
                        DILocalVariable *var = dbuilder->createAutoVariable(
                                lexical_blocks.back(),
                                n->var_name,
                                unit,
                                n->location.line,
                                getDebugType(ArrayType::get(Type::getInt8Ty(context), 256))
                        );
                        dbuilder->insertDeclare(
                                table.at(n->var_name),
                                var,
                                dbuilder->createExpression(),
                                DebugLoc::get(
                                        n->location.line,
                                        0,
                                        lexical_blocks.back()
                                ),
                                builder->GetInsertBlock()
                        );
                    }
                    break;
                }
                case Node::BOOL: { // bool variable
                    table.emplace(
                            n->var_name,
                            builder->CreateAlloca(
                                    Type::getInt1Ty(context),
                                    nullptr,
                                    n->var_name + "_ptr"
                            )
                    );
                    if (generateDI) {
                        DILocalVariable *var = dbuilder->createAutoVariable(
                                lexical_blocks.back(),
                                n->var_name,
                                unit,
                                n->location.line,
                                getDebugType(Type::getInt1Ty(context))
                        );
                        dbuilder->insertDeclare(
                                table.at(n->var_name),
                                var,
                                dbuilder->createExpression(),
                                DebugLoc::get(
                                        n->location.line,
                                        0,
                                        lexical_blocks.back()
                                ),
                                builder->GetInsertBlock()
                        );
                    }
                    break;
                }
                case Node::USER: // user-type variable
                    table.emplace(
                            n->var_name,
                            builder->CreateAlloca(
                                    user_types.at(n->user_type).first,
                                    nullptr,
                                    n->var_name + "_ptr"
                            )
                    );
                    break;
            }

            if (n->o1->value_type == Node::USER && n->o1->kind == Node::VAR_ACCESS) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type) {
                    error(
                            n->location.line,
                            "types of objects '"
                            + n->var_name
                            + "' ("
                            + std::string(
                                    n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int" : "float"))
                            + ") and '"
                            + n->o1->var_name
                            + "' ("
                            + std::string(
                                    n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER ? "int" : "float"))
                            + ") does not match!"
                    );
                }

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        table.at(n->o1->var_name),
                        module->getDataLayout().getTypeAllocSize(table.at(n->o1->var_name)->getType()),
                        table.at(n->o1->var_name)->getPointerAlignment(module->getDataLayout())
                );
            } else if (n->o1->value_type == Node::USER && (n->o1->kind == Node::FUNCTION_CALL || n->o1->kind == Node::METHOD_CALL)) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type) {
                    error(
                            n->location.line,
                            "types of objects '"
                            + n->var_name
                            + "' ("
                            + std::string(
                                    n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int" : "float"))
                            + ") and '"
                            + n->o1->var_name
                            + "' ("
                            + std::string(
                                    n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER ? "int" : "float"))
                            + ") does not match!"
                    );
                }

                generate(n->o1);
                Value *call = stack.top();
                stack.pop();

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        call,
                        module->getDataLayout().getTypeAllocSize(call->getType()),
                        call->getPointerAlignment(module->getDataLayout())
                );
            } else {
                if (n->o1->kind == Node::OBJECT_CONSTRUCT) {
                    stack.emplace(table.at(n->var_name));
                }

                generate(n->o1); // generate initial value

                if (n->o1->kind == Node::OBJECT_CONSTRUCT) { // object constructors are void functions
                    break;
                }

                Value *val = stack.top(); // take it from the stack
                stack.pop(); // erase it from the stack

                if (table.at(n->var_name)->getType() != val->getType()) {
                    if (table.at(n->var_name)->getType()->isIntegerTy(32) && val->getType()->isDoubleTy()) {
                        Value *buf = val;
                        val = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                    } else if (table.at(n->var_name)->getType()->isDoubleTy() && val->getType()->isIntegerTy(32)) {
                        Value *buf = val;
                        val = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                    } else if (table.at(n->var_name)->getType()->isIntegerTy(1) && val->getType()->isDoubleTy()) {
                        Value *buf = val;
                        val = builder->CreateFPToUI(buf, Type::getInt1Ty(context));
                    } else if (table.at(n->var_name)->getType()->isDoubleTy() && val->getType()->isIntegerTy(1)) {
                        Value *buf = val;
                        val = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                    }
                }

                if (table.at(n->var_name)->getType() == PointerType::get(ArrayType::get(Type::getInt8Ty(context), 256), 0)) {
                    Value *arr = builder->CreateBitCast(
                            table.at(n->var_name),
                            Type::getInt8PtrTy(context)
                    );
                    builder->CreateMemCpy(
                            arr,
                            val,
                            255,
                            module->getDataLayout().getABITypeAlignment(Type::getInt8Ty(context))
                    );
                } else {
                    builder->CreateStore(val, table.at(n->var_name));
                }
            }
            break;
        }
        case Node::DELETE: {
            if (generateDI) {
                emitLocation(n);
            }

            table.erase(n->var_name); // erase pointer from the table
            last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), n->var_name));
            break;
        }
        case Node::VAR_ACCESS: { // push value of the variable to the stack
            if (generateDI) {
                emitLocation(n);
            }

            Type *type;

            auto ptr = table.at(n->var_name);
            for (auto &&user_type : user_types) {
                auto iter = std::find(
                        std::begin(user_type.second.second.first),
                        std::end(user_type.second.second.first),
                        n->var_name
                );
                if (iter != std::end(user_type.second.second.first)) {
                    ptr = builder->CreateGEP(
                            user_type.second.first,
                            builder->CreateLoad(table.at("this")),
                            {
                                    ConstantInt::get(Type::getInt32Ty(context), 0),
                                    ConstantInt::get(
                                            Type::getInt32Ty(context),
                                            static_cast<uint64_t>(std::distance(std::begin(user_type.second.second.first), iter))
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
            } else if (table.at(n->var_name)->getType() == Type::getInt1PtrTy(context)) {
                type = Type::getInt1Ty(context);
            } else if (table.at(n->var_name)->getType() == PointerType::get(ArrayType::get(Type::getInt8Ty(context), 256), 0)) {
                stack.emplace(
                        builder->CreateGEP(
                                table.at(n->var_name), {
                                        ConstantInt::get(Type::getInt32Ty(context), 0),
                                        ConstantInt::get(Type::getInt32Ty(context), 0)
                                },
                                n->var_name
                        )
                );
                break;
            } else if (table.at(n->var_name)->getType() == PointerType::get(PointerType::get(Type::getInt8Ty(context), 0), 0)) {
                type = PointerType::get(Type::getInt8Ty(context), 0);
            }
            else {
                stack.emplace(table.at(n->var_name));
                break;
            }

            stack.emplace(builder->CreateLoad(type, ptr, n->var_name));
            break;
        }
        case Node::FUNC_OBJ_PROPERTY_ACCESS: {
            if (generateDI) {
                emitLocation(n);
            }

            Value *ptr;

            auto user_type = user_types.at(n->user_type);
            auto iter = std::find(
                    std::begin(user_type.second.first),
                    std::end(user_type.second.first),
                    n->property_name
            );
            int property_access = user_type.second.second.at(static_cast<unsigned>(std::distance(std::begin(user_type.second.first), iter)));

            if (property_access == Node::PRIVATE) {
                error(n->location.line, "property '" + n->property_name + "' of object '" + n->var_name + "' is private");
            }

            generate(n->o1);
            Value* src = stack.top();
            stack.pop();

            while (cast<PointerType>(src->getType())->getElementType()->isPointerTy()) {
                Value *temp = builder->CreateLoad(src);
                src = temp;
            }

            if (iter != std::end(user_type.second.first)) {
                ptr = builder->CreateGEP(
                        src,
                        {
                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                ConstantInt::get(
                                        Type::getInt32Ty(context),
                                        static_cast<uint64_t>(std::distance(std::begin(user_type.second.first), iter))
                                )
                        },
                        n->var_name + "::" + n->property_name
                );
            }
            stack.emplace(builder->CreateLoad(ptr, n->property_name));
            break;
        }
        case Node::PROPERTY_ACCESS: {
            if (generateDI) {
                emitLocation(n);
            }

            Value *ptr;
            auto user_type = user_types.at(n->user_type);
            auto iter = std::find(
                    std::begin(user_type.second.first),
                    std::end(user_type.second.first),
                    n->property_name
            );
            int property_access = user_type.second.second.at(static_cast<unsigned>(std::distance(std::begin(user_type.second.first), iter)));

            if (n->var_name != "this" && property_access == Node::PRIVATE) {
                error(n->location.line, "property '" + n->property_name + "' of object '" + n->var_name + "' is private");
            }

            Value* src = table.at(n->var_name);
            while (cast<PointerType>(src->getType())->getElementType()->isPointerTy()) {
                Value *temp = builder->CreateLoad(src);
                src = temp;
            }

            if (iter != std::end(user_type.second.first)) {
                ptr = builder->CreateGEP(
                        src,
                        {
                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                ConstantInt::get(
                                        Type::getInt32Ty(context),
                                        static_cast<uint64_t>(std::distance(std::begin(user_type.second.first), iter))
                                )
                        },
                        n->var_name + "::" + n->property_name
                );
            }
            stack.emplace(builder->CreateLoad(ptr, n->property_name));
            break;
        }
        case Node::FUNCTION_CALL: { // generate function's call
            if (generateDI) {
                emitLocation(n);
            }

            Function *callee = functions.at(n->var_name).first; // get the function's prototype

            if (callee->arg_size() != n->func_call_args.size()) { // check number of arguments in prototype and in calling
                error(
                        n->location.line,
                        "invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        ") , expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.emplace_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"_call");
            }

            stack.emplace(call); // push call to the stack
            break;
        }
        case Node::METHOD_CALL: { // generate function's call
            if (generateDI) {
                emitLocation(n);
            }

            Function *callee = functions.at(n->property_name).first; // get the function's prototype

            if (n->var_name != "this" && functions.at(n->property_name).second == Node::PRIVATE) {
                error(n->location.line, "method '" + n->property_name + "' of object '" + n->var_name + "' is private");
            }

            if (callee->arg_size() != n->func_call_args.size() && callee->arg_size()-n->func_call_args.size()>1) { // check number of arguments in prototype and in calling
                error(
                        n->location.line,
                        "invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        "), expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            Value *self = table.at(n->var_name);
            while (cast<PointerType>(self->getType())->getElementType()->isPointerTy()) {
                Value *temp = builder->CreateLoad(self);
                self = temp;
            }
            args.emplace_back(self);

            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.emplace_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"::"+n->property_name+"_call");
            }

            stack.emplace(call); // push call to the stack
            break;
        }
        case Node::FUNC_OBJ_METHOD_CALL: { // generate function's call
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1);
            Value *obj = stack.top();
            stack.pop();

            Function *callee = functions.at(n->property_name).first; // get the function's prototype

            if (n->var_name != "this" && functions.at(n->property_name).second == Node::PRIVATE) {
                error(n->location.line, "method '" + n->property_name + "' of object returned by function '" + n->var_name + "' is private");
            }

            if (callee->arg_size() != n->func_call_args.size() && callee->arg_size()-n->func_call_args.size()>1) { // check number of arguments in prototype and in calling
                error(
                        n->location.line,
                        "invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        "), expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            while (cast<PointerType>(obj->getType())->getElementType()->isPointerTy()) {
                Value *temp = builder->CreateLoad(obj);
                obj = temp;
            }
            args.emplace_back(obj);

            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.emplace_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"::"+n->property_name+"_call");
            }

            stack.emplace(call); // push call to the stack
            break;
        }
        case Node::OBJECT_CONSTRUCT: { // generate function's call
            if (generateDI) {
                emitLocation(n);
            }

            Function *callee = functions.at(n->var_name).first; // get the function's prototype

            if (callee->arg_size() != n->func_call_args.size() && callee->arg_size()-n->func_call_args.size()>1) { // check number of arguments in prototype and in calling
                error(
                        n->location.line,
                        "invalid number arguments (" +
                        std::to_string(n->func_call_args.size()) +
                        "), expected " +
                        std::to_string(callee->arg_size())
                );
            }

            std::vector<Value *> args; // generate values of arguments
            args.emplace_back(stack.top());
            stack.pop();
            for (auto &&arg : n->func_call_args) {
                generate(arg); // generate value
                args.emplace_back(stack.top()); // take it from the stack
                stack.pop(); // erase it from the stack
            }

            Value* call; // generate call
            if (callee->getReturnType() == Type::getVoidTy(context)) {
                call = builder->CreateCall(callee, args); // LLVM forbids give name to call of void function
            } else {
                call = builder->CreateCall(callee, args, n->var_name+"_call");
            }

            stack.emplace(call); // push call to the stack
            break;
        }
        case Node::ARRAY_ACCESS: { // generate access to array's element
            generate(n->o1); // get number of element to access
            Value *element_val = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // get number of elements in array
            int64_t element_num = 0;
            if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(element_val)) {
                if (CI->getBitWidth() <= 32) {
                    element_num = CI->getSExtValue();
                }
            }

            int64_t array_size = 0;
            if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(array_sizes.at(n->var_name))) {
                if (CI->getBitWidth() <= 32) {
                    array_size = CI->getSExtValue();
                }
            }

            if (element_num >= array_size && array_size != 0) {
                error(n->location.line, "array '" + n->var_name + "' has only " + std::to_string(array_size) + " elements");
            }


            // runtime check of accessing element
            Value *cond = builder->CreateICmpSGE(
                    element_val,
                    array_sizes.at(n->var_name)
            );
            Function *parent = builder->GetInsertBlock()->getParent();
            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(cond, thenBlock, elseBlock); // create conditional goto
            builder->SetInsertPoint(thenBlock);

            if (!io_using) {
                use_io();
            }
            std::vector<Value*> args;
            args.emplace_back(table.at("str_out_format"));
            args.emplace_back(
                    builder->CreateGlobalStringPtr(
                            "Runtime error: accessing unallocated element of array '" + n->var_name + "'"
                    )
            );
            builder->CreateCall(printf, args); // call prinf
            std::vector<Type *> exitArgs = { Type::getInt32Ty(context) };
            FunctionType *exitType = FunctionType::get(Type::getVoidTy(context), exitArgs, false);
            Constant *exit = module->getOrInsertFunction("exit", exitType);
            builder->CreateCall(exit, ConstantInt::get(Type::getInt32Ty(context), 1));

            builder->CreateBr(mergeBlock);
            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);
            builder->CreateBr(mergeBlock);
            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition


            Value *arr_ptr = table.at(n->var_name); // get array's pointer

            ArrayType* arr_type = cast<ArrayType>(cast<PointerType>(arr_ptr->getType())->getElementType());
            if (arr_type->getElementType() == ArrayType::get(Type::getInt8Ty(context), 256)) { // array of strings
                stack.emplace(
                        builder->CreateGEP(
                                builder->CreateGEP(
                                        arr_ptr,
                                        {
                                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                                element_val
                                        },
                                        n->var_name
                                ),
                                {
                                        ConstantInt::get(Type::getInt32Ty(context), 0),
                                        ConstantInt::get(Type::getInt32Ty(context), 0)
                                },
                                n->var_name
                        )
                );
            } else {
                stack.emplace(
                        builder->CreateLoad(
                                builder->CreateGEP( // get element's pointer
                                        arr_ptr,
                                        {
                                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                                element_val
                                        },
                                        n->var_name
                                ),
                                n->var_name
                        )
                );
            }
            break;
        }
        case Node::CONST: { // generate constant value
            if (generateDI) {
                emitLocation(n);
            }

            switch (n->value_type) {
                case Node::STRING: // str constant
                    stack.emplace(builder->CreateGlobalStringPtr(n->str_val));
                    break;
                case Node::INTEGER: { // integer constant
                    stack.emplace(ConstantInt::get(Type::getInt32Ty(context), static_cast<uint64_t>(n->int_val), true));
                    break;
                }
                case Node::BOOL: { // bool constant
                    stack.emplace(ConstantInt::get(Type::getInt1Ty(context), static_cast<uint64_t>(n->int_val)));
                    break;
                }
                case Node::FLOATING: { // float constant
                    stack.emplace(ConstantFP::get(Type::getDoubleTy(context), n->float_val));
                    break;
                }
            }
            break;
        }
        case Node::ADD: { // generate addition
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // strings concatenation
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcat = module->getOrInsertFunction(
                        "strcat",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                Value *new_str = builder->CreateBitCast(
                        builder->CreateAlloca(
                                ArrayType::get(Type::getInt8Ty(context), 256),
                                nullptr,
                                ""
                        ),
                        Type::getInt8PtrTy(context)
                );
                builder->CreateMemCpy(new_str, left, 255, module->getDataLayout().getABITypeAlignment(Type::getInt8Ty(context)));
                builder->CreateCall(strcat, { new_str, right });
                stack.emplace(new_str);
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToUI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateAdd(left, right)); // push generated addition to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.emplace(builder->CreateFAdd(left, right)); // push generated addition to the stack
            }
            break;
        }
        case Node::SUB: { // generate subtraction
            if (generateDI) {
                emitLocation(n);
            }

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
                stack.emplace(builder->CreateSub(left, right)); // push generated subtraction to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.emplace(builder->CreateFSub(left, right)); // push generated subtraction to the stack
            }
            break;
        }
        case Node::MUL: { // generate multiplication
            if (generateDI) {
                emitLocation(n);
            }

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
                stack.emplace(builder->CreateMul(left, right)); // push generated multiplication to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateUIToFP(buf, Type::getDoubleTy(context));
                }
                stack.emplace(builder->CreateFMul(left, right)); // push generated multiplication to the stack
            }
            break;
        }
        case Node::DIV: { // generate division
            if (generateDI) {
                emitLocation(n);
            }

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
                stack.emplace(builder->CreateSDiv(left, right)); // push generated division to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                stack.emplace(builder->CreateFDiv(left, right)); // push generated division to the stack
            }
            break;
        }
        case Node::AND: {
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.emplace(builder->CreateAnd(left, right));
            break;
        }
        case Node::OR: {
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            stack.emplace(builder->CreateOr(left, right));
            break;
        }
        case Node::NOT: {
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1);
            Value *val = stack.top();
            stack.pop();

            stack.emplace(
                    builder->CreateXor(
                            val,
                            ConstantInt::get(Type::getInt1Ty(context), static_cast<uint64_t>(true))
                    )
            );
            break;
        }
        case Node::LESS: { // < FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpSLT(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpSLT(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpULT(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::LESS_EQUAL: { // <= FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpSLE(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpSLE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpULE(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::MORE: { // > FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpSGT(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpSGT(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpUGT(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::MORE_EQUAL: { // >= FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpSGE(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpSGE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpUGE(left, right)); // push generated test to the stack
            }
            break;        }
        case Node::EQUAL: { // is FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpEQ(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpEQ(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpUEQ(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::NOT_EQUAL: { // is not FIXME floating point test
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate first value
            Value *left = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            generate(n->o2); // generate second value
            Value *right = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            // compare strings
            if (left->getType() == Type::getInt8PtrTy(context) && right->getType() == Type::getInt8PtrTy(context)) {
                Constant *strcmp = module->getOrInsertFunction(
                        "strcmp",
                        FunctionType::get(
                                Type::getInt32Ty(context),
                                { Type::getInt8PtrTy(context), Type::getInt8PtrTy(context) },
                                false
                        )
                );

                stack.emplace(builder->CreateICmpNE(
                        builder->CreateCall(strcmp, { left, right }),
                        ConstantInt::get(Type::getInt32Ty(context), 0))
                );
            }

            if (left->getType()->isIntegerTy()) { // left operand is int
                if (right->getType()->isDoubleTy()) { // if right operand is double, we have to convert it to int
                    Value *buf = right;
                    right = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                }
                stack.emplace(builder->CreateICmpNE(left, right)); // push generated test to the stack
            }
            else if (left->getType()->isDoubleTy()) { // left operand is double
                if (right->getType()->isIntegerTy()) { // if right operand is int, we have to convert it to double
                    Value *buf = right;
                    right = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                }
                //stack.emplace(builder->CreateFCmpUNE(left, right)); // push generated test to the stack
            }
            break;
        }
        case Node::SET: { // generate an update variable's or array element's value
            if (generateDI) {
                emitLocation(n);
            }

            if (n->o1->value_type == Node::USER && n->o1->kind == Node::VAR_ACCESS) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type) {
                    error(n->location.line,
                          "types of objects '"
                          + n->var_name
                          + "' ("
                          + std::string(
                                  n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int" : "float"))
                          + ") and '"
                          + n->o1->var_name
                          + "' ("
                          + std::string(
                                  n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER ? "int" : "float"))
                          + ") does not match!"
                    );
                }

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        table.at(n->o1->var_name),
                        module->getDataLayout().getTypeAllocSize(table.at(n->o1->var_name)->getType()),
                        table.at(n->o1->var_name)->getPointerAlignment(module->getDataLayout())
                );
            } else if (n->o1->value_type == Node::USER && (n->o1->kind == Node::FUNCTION_CALL || n->o1->kind == Node::METHOD_CALL)) {
                if (n->value_type != n->o1->value_type || n->user_type != n->o1->user_type) {
                    error(n->location.line,
                          "types of objects '"
                          + n->var_name
                          + "' ("
                          + std::string(
                                  n->value_type == Node::USER ? (n->user_type) : (n->value_type == Node::INTEGER ? "int" : "float"))
                          + ") and '"
                          + n->o1->var_name
                          + "' ("
                          + std::string(
                                  n->o1->value_type == Node::USER ? (n->o1->user_type) : (n->o1->value_type == Node::INTEGER ? "int" : "float"))
                          + ") does not match!"
                    );
                }

                generate(n->o1);
                Value *call = stack.top();
                stack.pop();

                builder->CreateMemCpy(
                        table.at(n->var_name),
                        call,
                        module->getDataLayout().getTypeAllocSize(call->getType()),
                        call->getPointerAlignment(module->getDataLayout())
                );
            } else {
                if (n->o1->kind == Node::OBJECT_CONSTRUCT) {
                    stack.emplace(table.at(n->var_name));
                }

                generate(n->o1); // generate new value

                if (n->o1->kind == Node::OBJECT_CONSTRUCT) { // object constructors are void functions
                    break;
                }

                Value *val = stack.top(); // take it from the stack
                stack.pop(); // erase it from the stack

                if (n->o2 != nullptr) { // change value of array's element
                    generate(n->o2); // generate the number of element
                    Value *element_val = stack.top(); // take it from the stack
                    stack.pop(); // erase it from the stack


                    // get number of elements in array
                    bool constant = false;
                    int64_t element_num = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(element_val)) {
                        if (CI->getBitWidth() <= 32) {
                            element_num = CI->getSExtValue();
                        }
                    } else {
                        Value *cond = builder->CreateOr(
                                builder->CreateICmpSGE(
                                        element_val,
                                        array_sizes.at(n->var_name)
                                ),
                                builder->CreateICmpSLT(
                                        element_val,
                                        ConstantInt::get(Type::getInt32Ty(context), 0)
                                )
                        );
                        Function *parent = builder->GetInsertBlock()->getParent();
                        BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
                        BasicBlock *elseBlock = BasicBlock::Create(context, "else");
                        BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

                        builder->CreateCondBr(cond, thenBlock, elseBlock); // create conditional goto
                        builder->SetInsertPoint(thenBlock);

                        if (!io_using) {
                            use_io();
                        }
                        std::vector<Value *> args;
                        args.emplace_back(table.at("str_out_format"));
                        args.emplace_back(
                                builder->CreateGlobalStringPtr(
                                        "Runtime error: accessing unallocated element of array '" + n->var_name + "'"
                                )
                        );
                        builder->CreateCall(printf, args); // call prinf
                        std::vector<Type *> exitArgs = {Type::getInt32Ty(context)};
                        FunctionType *exitType = FunctionType::get(Type::getVoidTy(context), exitArgs, false);
                        Constant *exit = module->getOrInsertFunction("exit", exitType);
                        builder->CreateCall(exit, ConstantInt::get(Type::getInt32Ty(context), 1));

                        builder->CreateBr(mergeBlock);
                        parent->getBasicBlockList().push_back(elseBlock);
                        builder->SetInsertPoint(elseBlock);
                        builder->CreateBr(mergeBlock);
                        parent->getBasicBlockList().push_back(mergeBlock);
                        builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition
                    }

                    int64_t array_size = 0;
                    if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(array_sizes.at(n->var_name))) {
                        if (CI->getBitWidth() <= 32) {
                            array_size = CI->getSExtValue();
                            if (element_num >= array_size) { // number of array's elements and accessing elemnt are constant
                                error(n->location.line, "array '" + n->var_name + "' has only " + std::to_string(array_size) + " elements");
                            }
                        }
                    } else { // array of non-constant number of items
                        // runtime check of accessing element
                        Value *cond = builder->CreateICmpSGE(
                                element_val,
                                array_sizes.at(n->var_name)
                        );
                        Function *parent = builder->GetInsertBlock()->getParent();
                        BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
                        BasicBlock *elseBlock = BasicBlock::Create(context, "else");
                        BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

                        builder->CreateCondBr(cond, thenBlock, elseBlock); // create conditional goto
                        builder->SetInsertPoint(thenBlock);

                        if (!io_using) {
                            use_io();
                        }
                        std::vector<Value *> args;
                        args.emplace_back(table.at("str_out_format"));
                        args.emplace_back(
                                builder->CreateGlobalStringPtr(
                                        "Runtime error: accessing unallocated element of array '" + n->var_name + "'"
                                )
                        );
                        builder->CreateCall(printf, args); // call prinf
                        std::vector<Type *> exitArgs = {Type::getInt32Ty(context)};
                        FunctionType *exitType = FunctionType::get(Type::getVoidTy(context), exitArgs, false);
                        Constant *exit = module->getOrInsertFunction("exit", exitType);
                        builder->CreateCall(exit, ConstantInt::get(Type::getInt32Ty(context), 1));

                        builder->CreateBr(mergeBlock);
                        parent->getBasicBlockList().push_back(elseBlock);
                        builder->SetInsertPoint(elseBlock);
                        builder->CreateBr(mergeBlock);
                        parent->getBasicBlockList().push_back(mergeBlock);
                        builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition
                    }

                    // all is ok
                    Value *arr_ptr = table.at(n->var_name); // get array's pointer
                    Value *el_ptr = builder->CreateGEP( // get element's pointer
                            arr_ptr,
                            {
                                    ConstantInt::get(Type::getInt32Ty(context), 0),
                                    element_val
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

                    if (el_ptr->getType() == PointerType::get(ArrayType::get(Type::getInt8Ty(context), 256), 0)) {
                        Value *arr = builder->CreateBitCast(
                                el_ptr,
                                Type::getInt8PtrTy(context)
                        );
                        builder->CreateMemCpy(
                                arr,
                                val,
                                255,
                                module->getDataLayout().getABITypeAlignment(Type::getInt8Ty(context))
                        );
                    } else {
                        builder->CreateStore(val, el_ptr); // update value of variable
                    }

                } else if (!n->property_name.empty()) { // change value of object's property
                    Value *property_ptr;
                    Value *temp = table.at(n->var_name);
                    while (temp->getType()->isPointerTy()) {
                        Value *_temp = builder->CreateLoad(temp);
                        temp = _temp;
                    }

                    auto user_type = user_types.at(temp->getType()->getStructName().str());
                    auto iter = std::find(
                            std::begin(user_type.second.first),
                            std::end(user_type.second.first),
                            n->property_name
                    );
                    int property_access = user_type.second.second.at(static_cast<unsigned>(std::distance(std::begin(user_type.second.first),iter)));

                    if (n->var_name != "this" && property_access == Node::PRIVATE) {
                        error(n->location.line, "property '" + n->property_name + "' of object '" + n->var_name + "' is private");
                    }

                    Value* src = table.at(n->var_name);
                    while (cast<PointerType>(src->getType())->getElementType()->isPointerTy()) {
                        Value *_temp = builder->CreateLoad(src);
                        src = _temp;
                    }

                    if (iter != std::end(user_type.second.first)) {
                        property_ptr = builder->CreateGEP(
                                src,
                                {
                                        ConstantInt::get(Type::getInt32Ty(context), 0),
                                        ConstantInt::get(
                                                Type::getInt32Ty(context),
                                                static_cast<uint64_t>(std::distance(std::begin(user_type.second.first), iter))
                                        )
                                },
                                n->var_name + "::" + n->property_name
                        );
                    }

                    /*if (el_ptr->getType() != val->getType()) {
                        if (el_ptr->getType() == Type::getInt32PtrTy(context) && val->getType()->isDoubleTy()) {
                            Value *buf = val;
                            val = builder->CreateFPToSI(buf, Type::getInt32Ty(context));
                        } else if (el_ptr->getType() == Type::getDoublePtrTy(context) &&
                                   val->getType()->isIntegerTy(32)) {
                            Value *buf = val;
                            val = builder->CreateSIToFP(buf, Type::getDoubleTy(context));
                        }
                    }*/
                    builder->CreateStore(val, property_ptr); // update value of variable
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
                        auto iter = std::find(
                                std::begin(user_type.second.second.first),
                                std::end(user_type.second.second.first),
                                n->var_name
                        );
                        if (iter != std::end(user_type.second.second.first)) {
                            ptr = builder->CreateGEP(
                                    user_type.second.first,
                                    builder->CreateLoad(table.at("this")),
                                    {
                                            ConstantInt::get(Type::getInt32Ty(context), 0),
                                            ConstantInt::get(
                                                    Type::getInt32Ty(context),
                                                    static_cast<uint64_t>(std::distance(std::begin(user_type.second.second.first), iter))
                                            )
                                    },
                                    n->var_name
                            );
                        }
                    }

                    if (table.at(n->var_name)->getType() == PointerType::get(ArrayType::get(Type::getInt8Ty(context), 256), 0)) {
                        Value *arr = builder->CreateBitCast(
                                table.at(n->var_name),
                                Type::getInt8PtrTy(context)
                        );
                        builder->CreateMemCpy(
                                arr,
                                val,
                                255,
                                module->getDataLayout().getABITypeAlignment(Type::getInt8Ty(context))
                        );
                    } else {
                        builder->CreateStore(val, table.at(n->var_name)); // update value of variable
                    }
                }
            }

            break;
        }
        case Node::CLASS_DEFINE: {
            if (generateDI) {
                emitLocation(n);
            }

            std::vector<Type *> properties_types;
            std::vector<std::string> properties_names;
            std::vector<int> properties_access;
            for (auto &&defProperty : n->class_def_properties) { // generate properties first
                if (defProperty.second.second->kind == Node::VAR_DEF) {
                    properties_names.emplace_back(defProperty.first);
                    properties_access.emplace_back(defProperty.second.first);
                    switch (defProperty.second.second->value_type) {
                        case Node::INTEGER:
                            properties_types.emplace_back(Type::getInt32Ty(context));
                            break;
                        case Node::FLOATING:
                            properties_types.emplace_back(Type::getDoubleTy(context));
                            break;
                        case Node::STRING:
                            properties_types.emplace_back(Type::getInt8PtrTy(context));
                            break;
                        case Node::BOOL:
                            properties_types.emplace_back(Type::getInt1Ty(context));
                            break;
                    }
                    //table.emplace(properties_names.back(), builder->CreateAlloca(properties_types.back()));
                }
            }

            StructType* class_type = StructType::create(context, n->var_name);
            class_type->setBody(properties_types);
            user_types.emplace(n->var_name, std::make_pair(class_type, std::make_pair(properties_names, properties_access)));

            for (auto &&defProperty : n->class_def_properties) { // then, generate methods
                if (defProperty.second.second->kind == Node::FUNCTION_DEFINE) {
                    FunctionType *type;

                    // generate arguments
                    std::vector<Type *> args_types;
                    std::vector<std::string> args_names;

                    args_types.emplace_back(PointerType::get(class_type, 0));
                    args_names.emplace_back("this");

                    for (auto &iterator : defProperty.second.second->o1->func_def_args) {
                        switch (iterator.second->value_type) {
                            case Node::INTEGER:
                                args_types.emplace_back(Type::getInt32Ty(context));
                                break;
                            case Node::FLOATING:
                                args_types.emplace_back(Type::getDoubleTy(context));
                                break;
                            case Node::STRING:
                                args_types.emplace_back(Type::getInt8PtrTy(context));
                                break;
                            case Node::BOOL:
                                args_types.emplace_back(Type::getInt1Ty(context));
                                break;
                            case Node::USER:
                                args_types.emplace_back(PointerType::get(user_types.at(iterator.second->user_type_name).first, 0));
                                break;
                        }
                        args_names.emplace_back(iterator.first);
                    }

                    switch (defProperty.second.second->value_type) { // set type of the function
                        case Node::INTEGER:
                            type = FunctionType::get(Type::getInt32Ty(context), args_types, false);
                            break;
                        case Node::FLOATING:
                            type = FunctionType::get(Type::getDoubleTy(context), args_types, false);
                            break;
                        case Node::STRING:
                            type = FunctionType::get(Type::getInt8PtrTy(context), args_types, false);
                            break;
                        case Node::BOOL:
                            type = FunctionType::get(Type::getInt1Ty(context), args_types, false);
                            break;
                        case Node::USER:
                            type = FunctionType::get(PointerType::get(class_type, 0), args_types, false);
                            break;
                        default:
                            type = FunctionType::get(Type::getVoidTy(context), args_types, false);
                    }
                    Function *func = Function::Create(type, Function::ExternalLinkage, defProperty.second.second->var_name, module.get());
                    functions.emplace(defProperty.second.second->var_name, std::make_pair(func, defProperty.second.first));

                    BasicBlock *entry = BasicBlock::Create(context, "entry", func);
                    builder->SetInsertPoint(entry); // set new insert block

                    DISubprogram *SP;
                    if (generateDI) {
                        unit = dbuilder->createFile(
                                compileUnit->getFilename(),
                                compileUnit->getDirectory()
                        );

                        DIScope *fcontext = unit;
                        unsigned line = n->location.line;
                        unsigned line_scope = 0;
                        SP = dbuilder->createMethod(
                                fcontext,
                                func->getName(),
                                StringRef(),
                                unit,
                                line,
                                CreateFunctionType(args_types, unit),
                                false,
                                true,
                                line_scope,
                                DINode::FlagPrototyped,
                                false
                        );
                        func->setSubprogram(SP);
                        func_scopes[n] = SP;
                        lexical_blocks.emplace_back(func_scopes[n]);
                    }

                    unsigned long idx = 0;
                    for (auto &Arg : func->args()) { // create pointers to arguments of the function
                        std::string name = args_names.at(idx++);
                        Arg.setName(name);

                        // insert argument's allocator to the table
                        table.emplace(
                                name,
                                builder->CreateAlloca(
                                        Arg.getType(),
                                        nullptr,
                                        name+"_ptr"
                                )
                        );
                        builder->CreateStore(&Arg, table.at(name)); // store the value of argument to allocator
                        if (generateDI) {
                            DILocalVariable *var = dbuilder->createParameterVariable(
                                    SP,
                                    name,
                                    static_cast<unsigned int>(idx),
                                    unit,
                                    n->location.line,
                                    getDebugType(Arg.getType()),
                                    true
                            );
                            emitLocation(n->o2);
                        }
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
                            case Node::BOOL:
                                builder->CreateRet(ConstantInt::get(Type::getInt1Ty(context), 0));
                                break;
                            default:
                                builder->CreateRetVoid();
                        }
                    }

                    if (optimize) {
                        passmgr->run(*func); // run the optimizer
                    }

                    for (auto &&var : last_vars) { // erase vars declared in this function
                        table.erase(var);
                        last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
                    }

                    for (auto &Arg : func->args()) { // erase arguments' allocators from the table
                        table.erase(Arg.getName());
                    }
                }
            }

            //for (auto &&item : properties_names) {
            //    table.erase(item);
            //}

            break;
        }
        case Node::IF: { // generate 'if' condition without 'else' branch
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock); // create conditional goto

            builder->SetInsertPoint(thenBlock);

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            generate(n->o2); // generate the body of 'then' branch

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = _temp;

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition

            break;
        }
        case Node::ELSE: { // generate 'if' condition with 'else' branch
            if (generateDI) {
                emitLocation(n);
            }

            generate(n->o1); // generate condition

            Function *parent = builder->GetInsertBlock()->getParent();

            BasicBlock *thenBlock = BasicBlock::Create(context, "then", parent); // create blocks
            BasicBlock *elseBlock = BasicBlock::Create(context, "else");
            BasicBlock *mergeBlock = BasicBlock::Create(context, "ifcont");

            builder->CreateCondBr(stack.top(), thenBlock, elseBlock); // create conditional goto

            builder->SetInsertPoint(thenBlock);

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            generate(n->o2); // generate the body of 'then' branch

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = _temp;

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(elseBlock);
            builder->SetInsertPoint(elseBlock);

            std::vector<std::string> __temp(last_vars);
            last_vars.clear();

            generate(n->o3); // generate the body of 'else' branch

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = __temp;

            builder->CreateBr(mergeBlock);

            parent->getBasicBlockList().push_back(mergeBlock);
            builder->SetInsertPoint(mergeBlock); // set insert point to block after the condition

            break;
        }
        case Node::DO: { // do-while cycle
            Function *parent = builder->GetInsertBlock()->getParent();

            if (generateDI) {
                emitLocation(n);
            }

            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            generate(n->o1); // generate the body
            generate(n->o2); // generate the condition
            Value *condition = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = _temp;

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock); // set insert point to block after the condition

            break;
        }
        case Node::WHILE: { // 'while' cycle
            Function *parent = builder->GetInsertBlock()->getParent();

            if (generateDI) {
                emitLocation(n);
            }

            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock);

            builder->SetInsertPoint(loopBlock);

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            generate(n->o2); // generate the body

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = _temp;

            generate(n->o1); // generate the condition
            Value *condition = stack.top(); // take it from the stack
            stack.pop(); // erase it from the stack

            BasicBlock *afterBlock = BasicBlock::Create(context, "afterloop", parent);
            builder->CreateCondBr(condition, loopBlock, afterBlock); // create conditional goto
            builder->SetInsertPoint(afterBlock); // set insert point to block after the condition

            break;
        }
        case Node::REPEAT: { // 'repeat' cycle
            table.try_emplace("index", builder->CreateAlloca(Type::getInt32Ty(context), nullptr, "index_ptr"));

            builder->CreateStore(ConstantInt::get(Type::getInt32Ty(context), APInt(32, 0)),
                                 table.at("index")); // zeroize the counter

            Function *parent = builder->GetInsertBlock()->getParent();

            if (generateDI) {
                emitLocation(n);
            }

            BasicBlock *loopBlock = BasicBlock::Create(context, "loop", parent);
            builder->CreateBr(loopBlock); // go to begin of the loop
            builder->SetInsertPoint(loopBlock);

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            generate(n->o2); // generate the body

            for (auto &&var : last_vars) {
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }

            last_vars = _temp;

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
            if (n->var_name == "main") {
                n->value_type = Node::INTEGER;
            }

            FunctionType *type;

            // generate arguments
            std::vector<Type *> args_types;
            std::vector<std::string> args_names;
            for (auto &iterator : n->o1->func_def_args) {
                switch (iterator.second->value_type) {
                    case Node::INTEGER:
                        args_types.emplace_back(Type::getInt32Ty(context));
                        break;
                    case Node::FLOATING:
                        args_types.emplace_back(Type::getDoubleTy(context));
                        break;
                    case Node::STRING:
                        args_types.emplace_back(Type::getInt8PtrTy(context));
                        break;
                    case Node::BOOL:
                        args_types.emplace_back(Type::getInt1Ty(context));
                        break;
                    case Node::USER:
                        args_types.emplace_back(PointerType::get(
                                user_types.at(iterator.second->user_type_name).first, 0)
                        );
                        break;
                }
                args_names.emplace_back(iterator.first);
            }

            switch (n->value_type) { // set type of the function
                case Node::INTEGER:
                    type = FunctionType::get(Type::getInt32Ty(context), args_types, false);
                    break;
                case Node::FLOATING:
                    type = FunctionType::get(Type::getDoubleTy(context), args_types, false);
                    break;
                case Node::STRING:
                    type = FunctionType::get(Type::getInt8PtrTy(context), args_types, false);
                    break;
                case Node::BOOL:
                    type = FunctionType::get(Type::getInt1Ty(context), args_types, false);
                    break;
                case Node::USER:
                    type = FunctionType::get(PointerType::get(user_types.at(n->user_type).first, 0), args_types, false);
                    break;
                default:
                    type = FunctionType::get(Type::getVoidTy(context), args_types, false);
            }
            Function *func = Function::Create(type, Function::ExternalLinkage, n->var_name, module.get());
            functions.emplace(n->var_name, std::make_pair(func, Node::PUBLIC));

            BasicBlock *entry = BasicBlock::Create(context, "entry", func);
            builder->SetInsertPoint(entry); // set new insert block

            DISubprogram *SP;
            if (generateDI) {
                unit = dbuilder->createFile(
                        compileUnit->getFilename(),
                        compileUnit->getDirectory()
                );

                DIScope *fcontext = unit;
                unsigned line = n->location.line;
                unsigned line_scope = 0;
                SP = dbuilder->createFunction(
                        fcontext,
                        func->getName(),
                        StringRef(),
                        unit,
                        line,
                        CreateFunctionType(args_types, unit),
                        false,
                        true,
                        line_scope,
                        DINode::FlagPrototyped,
                        false
                );
                func->setSubprogram(SP);
                func_scopes[n] = SP;
                lexical_blocks.emplace_back(func_scopes[n]);
            }

            unsigned idx = 0;
            for (auto &Arg : func->args()) { // create pointers to arguments of the function
                std::string name = args_names.at(idx++);
                Arg.setName(name);

                // insert argument's allocator to the table
                table.emplace(
                        name,
                        builder->CreateAlloca(
                                Arg.getType(),
                                nullptr,
                                name + "_ptr"
                        )
                );

                builder->CreateStore(&Arg, table.at(name)); // store the value of argument to allocator

                if (generateDI) {
                    DILocalVariable *var = dbuilder->createParameterVariable(
                            SP,
                            name,
                            idx,
                            unit,
                            n->location.line,
                            getDebugType(Arg.getType()),
                            true
                    );
                    emitLocation(n->o2);
                }
            }
            generate(n->o2); // generate body of the function

            lexical_blocks.pop_back();

            if (!func->getAttributes().hasAttribute(0, "ret")) {
                switch (n->value_type) { // create default return value
                    case Node::INTEGER:
                        builder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
                        break;
                    case Node::FLOATING:
                        builder->CreateRet(ConstantFP::get(Type::getDoubleTy(context), 0.0));
                        break;
                    case Node::BOOL:
                        builder->CreateRet(ConstantInt::get(Type::getInt1Ty(context), 0));
                        break;
                    default:
                        builder->CreateRetVoid();
                }
            }

            if (optimize) {
                passmgr->run(*func); // run the optimizer
            }

            for (auto &&var : last_vars) { // erase vars declared in this function
                table.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
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
        case Node::PRINTLN: { // print something
            if (!io_using) {
                use_io();
            }

            generate(n->o1); // generate the value to print
            std::vector<Value*> args;

            Value *format;
            if (stack.top()->getType()->isIntegerTy()) { // print integer
                format = table.at("int_out_format");
            } else if (stack.top()->getType()->isDoubleTy()) { // print float
                format = table.at("float_out_format");
            } else { // print string
                format = table.at("str_out_format");
            }

            args.emplace_back(format);
            args.emplace_back(stack.top()); // get the value from the stack
            stack.pop(); // erase it from the stack
            builder->CreateCall(printf, args); // call prinf

            break;
        }
        case Node::INPUT: {
            if (!io_using) {
                use_io();
            }

            std::vector<Value *> args;
            Value *format;
            bool is_str_var = false;
            if (table.at(n->var_name)->getType() == Type::getInt32PtrTy(context) ||
                table.at(n->var_name)->getType() == Type::getDoublePtrTy(context)) {
                format = table.at("float_in_format");
            } else {
                format = table.at("str_in_format");
                is_str_var = true;
            }

            args.emplace_back(format);

            Value *var;
            if (is_str_var) {
                var = builder->CreateGEP( // get element's pointer
                        table.at(n->var_name),
                        {
                                ConstantInt::get(Type::getInt32Ty(context), 0),
                                ConstantInt::get(Type::getInt32Ty(context), 0)
                        },
                        n->var_name
                );
            } else {
                var = table.at(n->var_name);
            }
            args.emplace_back(var);
            builder->CreateCall(scanf, args); // call scanf

            break;
        }
    }
}

DIType *Generator::getDebugType(Type *ty) {
    unsigned align = module->getDataLayout().getABITypeAlignment(ty);
    if (ty->isIntegerTy(32)) {
        return dbuilder->createBasicType("int", 32, align, dwarf::DW_ATE_signed);
    } else if (ty->isDoubleTy()) {
        return dbuilder->createBasicType("float", 64, align, dwarf::DW_ATE_float);
    } else if (ty->isIntegerTy(8)) {
        return dbuilder->createBasicType("char", 8, align, dwarf::DW_ATE_unsigned_char);
    } else if (ty == ArrayType::get(Type::getInt8Ty(context), 256)) {
        return dbuilder->createArrayType(256, align, getDebugType(Type::getInt8Ty(context)), nullptr);
    } else if (ty->isIntegerTy(1)) {
        return dbuilder->createBasicType("bool", 1, align, dwarf::DW_ATE_boolean);
    } else {
        return nullptr;
    }
}

DISubroutineType *Generator::CreateFunctionType(std::vector<Type *> args, DIFile *Unit) {
    std::vector<Metadata *> types;
    for (auto &&type : args) {
        types.emplace_back(getDebugType(type));
    }

    return dbuilder->createSubroutineType(dbuilder->getOrCreateTypeArray(types));
}

void Generator::emitLocation(std::shared_ptr<Node> n) {
    DIScope *scope;

    if(lexical_blocks.empty()) {
        scope = compileUnit;
    } else {
        scope = lexical_blocks.back();
    }

    builder->SetCurrentDebugLocation(
            DebugLoc::get(
                    n->location.line,
                    n->location.column,
                    scope
            )
    );
}
