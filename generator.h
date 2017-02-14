//
// Created by NEzyaka on 29.09.16.
//

#ifndef TURNIP2_GENERATOR_H
#define TURNIP2_GENERATOR_H

#include "node.h"
#include <map>
#include <stack>
#include <memory>
#include <string>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IR/LegacyPassManager.h>

using namespace llvm;

class Generator {
    void error(unsigned line, const std::string &e);
    std::string file;

    std::map<std::string, std::pair<StructType *, std::pair<std::vector<std::string>, std::vector<int>>>> user_types;
    std::map<std::string, Value *> table;
    std::map<std::string, Value *> array_sizes;
    std::map<std::string, std::pair<Function *, int>> functions;
    LLVMContext context;
    std::vector<std::string> last_vars;

    std::unique_ptr<IRBuilder<>> builder;

    std::stack<Value*> stack;
    std::unique_ptr<legacy::FunctionPassManager> passmgr;
    bool optimize;

    std::vector<Type*> printfArgs;
    FunctionType *printfType;
    Constant *printf;

    std::vector<Type*> scanfArgs;
    FunctionType *scanfType;
    Constant *scanf;

    bool io_using = false;
    void use_io();

    bool generateDI;
    DICompileUnit *compileUnit;

    DIType *getDebugType(Type *ty);
    std::vector<DIScope *> lexical_blocks;
    std::map<std::shared_ptr<Node>, DIScope *> func_scopes;
    DIFile *unit;

    DISubroutineType *CreateFunctionType(std::vector<Type *> args, DIFile *unit);
    void emitLocation(std::shared_ptr<Node> n);

public:
    Generator(bool opt, bool genDI, const std::string &f);
    void generate(const std::shared_ptr<Node>& n);
    std::unique_ptr<Module> module;
    std::unique_ptr<DIBuilder> dbuilder;

};


#endif //TURNIP2_GENERATOR_H
