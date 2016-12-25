//
// Created by NEzyaka on 28.09.16.
//

#ifndef TURNIP2_PARSER_H
#define TURNIP2_PARSER_H

#include "lexer.h"
#include "node.h"

#include <vector>

class Parser {
    Lexer *lexer;
    std::vector<std::string> last_vars;

    void error(const std::string &e);
    std::shared_ptr<Node> term();
    std::shared_ptr<Node> sum();
    std::shared_ptr<Node> test();
    std::shared_ptr<Node> expr();
    std::shared_ptr<Node> function_arg();
    std::shared_ptr<Node> paren_expr();
    std::shared_ptr<Node> function_args();
    std::shared_ptr<Node> statement();

public:
    explicit Parser(Lexer *l) : lexer(l) {}
    std::shared_ptr<Node> parse();

};


#endif //TURNIP2_PARSER_H
