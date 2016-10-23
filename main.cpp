#include "lexer.h"
#include "parser.h"
#include "generator.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/LegacyPassManager.h"

#include <iostream>
#include <stdlib.h>

void generateObject(Module *m, std::string &out) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetAsmPrinter();

    auto targetTriple = sys::getDefaultTargetTriple();
    m->setTargetTriple(targetTriple);

    std::string error;
    auto target = TargetRegistry::lookupTarget(targetTriple, error);

    if (!target) {
        errs() << error;
        return;
    }

    auto CPU = "generic";
    auto features = "";

    TargetOptions opt;
    auto RM = Optional<Reloc::Model>();
    auto theTargetMachine = target->createTargetMachine(targetTriple, CPU, features, opt, RM);

    m->setDataLayout(theTargetMachine->createDataLayout());

    std::error_code EC;
    raw_fd_ostream dest(out + ".o", EC, sys::fs::F_None);

    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return;
    }

    legacy::PassManager pass;
    auto fileType = TargetMachine::CGFT_ObjectFile;

    if (theTargetMachine->addPassesToEmitFile(pass, dest, fileType)) {
        errs() << "Couldn't emit a file of this type";
        return;
    }

    pass.run(*m);
    dest.flush();

    system(std::string("gcc " + out + ".o " + " -o " + out).c_str());
}

int main(int argc, char **argv) {
    std::ifstream in(argv[1]);

    char c;
    std::vector<char> code;

    while (!in.eof()) {
        in.get(c);
        code.push_back(c);
    }
    code.push_back(EOF);

    try {
        Lexer *lexer = new Lexer;
        lexer->load(code);

        Parser *parser = new Parser(lexer);
        Node *ast = parser->parse();

        Generator *generator = new Generator;
        generator->generate(ast);

        generator->module->dump();


        std::string output = "a.out";
        if (argc >= 3)
            output = std::string(argv[2]);

        generateObject(generator->module, output);
    }
    catch (const std::string &err) {
        std::cerr << err << std::endl;
        return 1;
    }

    return 0;
}
