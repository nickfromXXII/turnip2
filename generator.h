//
// Created by NEzyaka on 29.09.16.
//

#ifndef TURNIP2_GENERATOR_H
#define TURNIP2_GENERATOR_H

#include "node.h"
#include <map>
#include <stack>
#include <string>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DataLayout.h>

using namespace llvm;

class Generator {
    std::map<std::string, std::pair<StructType *, std::pair<std::vector<std::string>, std::vector<int>>>> user_types;
    std::map<std::string, Value *> table;
    std::map<std::string, std::pair<Function *, int>> functions;
    LLVMContext context;

    IRBuilder<> *builder;
    std::stack<Value*> stack;

    std::vector<Type*> printfArgs;
    FunctionType *printfType;
    Constant *printf{};

    std::vector<Type*> scanfArgs;
    FunctionType *scanfType;
    Constant *scanf{};

    bool io_using = false;
    void use_io();

public:
    Generator() {
        module = new Module("trnp2", context);
        builder = new IRBuilder<>(context);
    }

    void generate(std::shared_ptr<Node> n);
    Module *module;

};


#endif //TURNIP2_GENERATOR_H
