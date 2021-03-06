//
// Created by NEzyaka on 28.09.16.
//

#ifndef TURNIP2_PARSER_H
#define TURNIP2_PARSER_H

#include "lexer.h"
#include "utilities.h"

#include <vector>

using namespace turnip2;

class Parser {
    Lexer *lexer;
    std::vector<std::string> last_vars;

    void error(const std::string &e);
    std::shared_ptr<Node> term();
    std::shared_ptr<Node> sum();
    std::shared_ptr<Node> test();
    std::shared_ptr<Node> expr();
    std::shared_ptr<Node> paren_expr();
    std::shared_ptr<Node> var_def(bool isClassProperty = false);
    std::shared_ptr<Node> function_arg();
    std::shared_ptr<Node> function_args();
    std::shared_ptr<Node> function_def();
    std::shared_ptr<Node> method_def(const std::string &class_name);
    std::shared_ptr<Node> statement();

public:
    explicit Parser(Lexer *l) : lexer(l) {}
    std::shared_ptr<Node> parse();

};


#endif //TURNIP2_PARSER_H
