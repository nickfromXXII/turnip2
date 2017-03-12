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

    class Class_definition {
    public:
        std::string name;
        StructType *llvm_type;

        std::vector<std::string> private_properties;
        std::vector<std::string> protected_properties;
        std::vector<std::string> public_properties;
        
        inline std::vector<std::string>::iterator private_property(const std::string &name) {
            return std::find(std::begin(private_properties), std::end(private_properties), name);
        }

        inline std::vector<std::string>::iterator protected_property(const std::string &name) {
            return std::find(std::begin(protected_properties), std::end(protected_properties), name);
        }

        inline std::vector<std::string>::iterator public_property(const std::string &name) {
            return std::find(std::begin(public_properties), std::end(public_properties), name);
        }
        
        
        std::map<std::string, Function *> private_methods;
        std::map<std::string, Function *> protected_methods;
        std::map<std::string, Function *> public_methods;

        inline std::map<std::string, Function *>::iterator private_method(const std::string &name) {
            return std::find(std::begin(private_methods), std::end(private_methods), name);
        }

        inline std::map<std::string, Function *>::iterator protected_method(const std::string &name) {
            return std::find(std::begin(protected_methods), std::end(protected_methods), name);
        }

        inline std::map<std::string, Function *>::iterator public_method(const std::string &name) {
            return std::find(std::begin(public_methods), std::end(public_methods), name);
        }
    };

    std::map<std::string, std::shared_ptr<Class_definition>> user_types;
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
