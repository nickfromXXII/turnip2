//
// Created by NEzyaka on 28.09.16.
//

#ifndef TURNIP2_PARSER_H
#define TURNIP2_PARSER_H

#include "lexer.h"
#include "node.h"

class Parser {
    Lexer *lexer;

    void error(const std::string &e);
    Node *term();
    Node *sum();
    Node *test();
    Node *expr();
    Node *function_arg();
    Node *paren_expr();
    Node *function_args();
    Node *statement();

public:
    Parser(Lexer *l) { lexer = l; }
    Node *parse();

};


#endif //TURNIP2_PARSER_H
