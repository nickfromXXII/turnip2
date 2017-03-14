#include "lexer.h"
#include "parser.h"
#include "generator.h"

#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

#include <iostream>
#include <unistd.h>

class InputParser {
public:
    InputParser (int &argc, char **argv) {
        if (argc == 1) {
            show_usage();
            exit(0);
        }

        for (int i = 1; i < argc; ++i) {
            if (std::find(std::begin(supported_options), std::end(supported_options), std::string(argv[i]))
                != std::end(supported_options) || std::string(argv[i-1]) == "-o" || i == 1) {
                tokens.emplace_back(std::string(argv[i]));
            } else {
                std::cerr << "error: option '" << argv[i] << "' is not supported!" << std::endl;
            }
        }
    }

    void show_usage() {
        std::cout << "USAGE: turnip2 <input> [options]" << std::endl
                << "OPTIONS:" << std::endl
                << "\t -g          generate source-level debug information" << std::endl
                << "\t -emit-llvm  emit LLVM IR for source inputs" << std::endl
                << "\t -o <file>   write output to <file>" << std::endl
                << "\t -O          optimize code to reduce size and time of execution" << std::endl
                << "\t -S          only run compilation steps" << std::endl;
    }

    const std::string& get_option(const std::string &option) const {
        std::vector<std::string>::const_iterator itr;
        itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
        if (itr != this->tokens.end() && ++itr != this->tokens.end()){
            return *itr;
        }

        for (auto &&token : tokens) {
            if (token.find(option) == 0) {
                return *new std::string(token.substr(token.find_first_of(option.back())+1));
            }
        }

        return *new std::string("");
    }

    bool option_exists(const std::string &option) const {
        return std::find(std::cbegin(tokens), std::cend(tokens), option) != std::cend(tokens);
    }

private:
    std::vector<std::string> tokens;
    const std::vector<std::string> supported_options = {
            "-g",
            "-emit-llvm",
            "-o",
            "-O",
            "-S"
    };
};

void generateObject(Module *m, std::string &out) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetAsmPrinter();

    auto targetTriple = sys::getDefaultTargetTriple();
    m->setTargetTriple(targetTriple);

    std::string error;
    auto target = TargetRegistry::lookupTarget(targetTriple, error);

    if (target == nullptr) {
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
    raw_fd_ostream dest((out.find('.') != std::string::npos) ? (out.substr(0, std::string(out).find_last_of('.')) + ".o") : (out + ".o"), EC, sys::fs::F_None);

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

#if defined(__linux__)
    execl("/usr/bin/gcc", "/usr/bin/gcc", std::string((out.find('.') != std::string::npos) ? (out.substr(0, std::string(out).find_last_of('.')) + ".o") : (out + ".o")).c_str(), std::string("-o" + out).c_str(), NULL);
#endif

}

int main(int argc, char **argv) {
    InputParser params(argc, argv);
    std::ifstream in(argv[1]);

    char c;
    std::vector<char> code;

    while (!in.eof()) {
        in.get(c);
        code.emplace_back(c);
    }
    code.emplace_back(EOF);

    try {
        Lexer *lexer = new Lexer;
        lexer->load(code);

        Parser *parser = new Parser(lexer);
        std::shared_ptr<Node> ast = parser->parse();

        bool optimize = params.option_exists("-O");
        bool generateDI = params.option_exists("-g");
        if (optimize && generateDI)
            generateDI = false;

        Generator *generator = new Generator(optimize, generateDI, argv[1]);
        generator->generate(ast);

        std::string output = "a.out";
        if (!params.get_option("-o").empty()) {
            output = params.get_option("-o");
        }

        if (params.option_exists("-emit-llvm")) {
            std::string file = std::string(argv[1]).substr(0, std::string(argv[1]).find_last_of('.')) + ".s";
            std::error_code EC;
            raw_fd_ostream dest(file, EC, sys::fs::F_None);
            generator->module.get()->print(dest, nullptr);
            dest.close();
        }

        if (!params.option_exists("-S")) {
            generateObject(generator->module.get(), output);

            if (generateDI) {
                generator->dbuilder->finalize();
            }
        }
    }
    catch (const std::string &err) {
        std::cerr << "In file " << argv[1] << ":" << err << std::endl;
        return 1;
    }

    return 0;
}
