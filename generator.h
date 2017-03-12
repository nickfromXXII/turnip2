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

    class Method {
    public:
        Method(Function *p = nullptr, unsigned short a = Node::PRIVATE) : prototype(p), access_type(a) {}
        Function *prototype;
        unsigned short access_type;
    };

    class ClassDefinition {
    public:
        ClassDefinition(const std::string &n = "", StructType *ty = nullptr) : name(n), llvm_type(ty) {}
        std::string name;
        StructType *llvm_type;

        std::map<std::string, unsigned short> properties;
        std::map<std::string, std::shared_ptr<Method>> methods;
    };

    std::map<std::string, std::shared_ptr<ClassDefinition>> user_types;
    std::map<std::string, Value *> table;
    std::map<std::string, Value *> array_sizes;
    std::map<std::string, Function *> functions;
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
