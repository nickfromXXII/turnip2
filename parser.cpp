//
// Created by NEzyaka on 28.09.16.
//

#include "parser.h"

#include <iostream>
#include <stdlib.h>

void Parser::error(const std::string &e) {
    std::cerr << "Parse error at line " << std::to_string(lexer->line) << ": " << e << std::endl;
    exit(1);
}

Node *Parser::term() {
    Node *x;

    if (lexer->sym == Lexer::ID) {
        x = new Node(Node::VAR);
        x->var_name = lexer->str_val;

        lexer->next_token();
    } else if (lexer->sym == Lexer::NUM) {
        x = new Node(Node::CONST);
        x->value_type = Node::integer;

        x->int_val = lexer->num_val;

        lexer->next_token();
    } else if (lexer->sym == Lexer::STR) {
        x = new Node(Node::CONST);
        x->value_type = Node::string;

        x->str_val = lexer->str_val;

        lexer->next_token();
    } else x = paren_expr();

    return x;
}

Node *Parser::sum() {
    Node *t, *x = term();

    while (lexer->sym == Lexer::PLUS || lexer->sym == Lexer::MINUS || lexer->sym == Lexer::STAR || lexer->sym == Lexer::SLASH) {
        t = x;

        int kind = -1;
        switch (lexer->sym) {
            case Lexer::PLUS: {
                kind = Node::ADD;
                break;
            }
            case Lexer::MINUS:
                kind = Node::SUB;
                break;
            case Lexer::STAR:
                kind = Node::MUL;
                break;
            case Lexer::SLASH:
                kind = Node::DIV;
                break;
        }
        x = new Node(kind);

        lexer->next_token();

        x->o1 = t;
        x->o2 = term();
    }

    return x;
}

Node *Parser::test() {
    Node *t, *x = sum();

    switch (lexer->sym) {
        case Lexer::LESS: {
            t = x;
            x = new Node(Node::LESS_TEST);

            lexer->next_token();

            if (lexer->sym == Lexer::EQUAL) {
                x = new Node(Node::LESS_IS_TEST);
                lexer->next_token();
            }

            x->o1 = t;
            x->o2 = sum();

            break;
        }
        case Lexer::MORE: {
            t = x;
            x = new Node(Node::MORE_TEST);

            lexer->next_token();

            if (lexer->sym == Lexer::EQUAL) {
                x = new Node(Node::MORE_IS_TEST);
                lexer->next_token();
            }

            x->o1 = t;
            x->o2 = sum();

            break;
        }
        case Lexer::IS: {
            t = x;
            x = new Node(Node::IS_TEST);

            lexer->next_token();

            if (lexer->sym == Lexer::NOT) {
                x = new Node(Node::IS_NOT_TEST);
                lexer->next_token();
            }

            x->o1 = t;
            x->o2 = sum();

            break;
        }
    }

    return x;
}

Node *Parser::expr() {
    Node *t, *x;

    if (lexer->sym != Lexer::ID)
        return test();

    x = test();

    if (x->kind == Node::VAR) {
        if (lexer->sym == Lexer::EQUAL) {
            t = x;
            x = new Node(Node::SET);

            lexer->next_token();

            x->var_name = t->var_name;
            x->o1 = expr();
        }
    }

    return x;
}

Node *Parser::paren_expr() {
    if (lexer->sym != Lexer::L_PARENT)
        error("'(' expected");

    lexer->next_token();
    Node *n = expr();

    if (lexer->sym != Lexer::R_PARENT)
        error("') expected");

    lexer->next_token();
    return n;
}

Node *Parser::statement() {
    Node *t, *x;

    switch (lexer->sym) {
        case Lexer::IF: {
            x = new Node(Node::IF);
            lexer->next_token();

            x->o1 = paren_expr();
            x->o2 = statement();

            if (lexer->sym == Lexer::ELSE) {
                x->kind = Node::ELSE;
                lexer->next_token();
                x->o3 = statement();
            }

            break;
        }
        case Lexer::WHILE: {
            x = new Node(Node::WHILE);
            lexer->next_token();

            x->o1 = paren_expr();
            x->o2 = statement();

            break;
        }
        case Lexer::DO: {
            x = new Node(Node::DO);
            lexer->next_token();

            x->o1 = statement();

            if (lexer->sym != Lexer::WHILE)
                error("'while' expected");

            lexer->next_token();

            x->o2 = paren_expr();

            if (lexer->sym != Lexer::SEMICOLON)
                error("';' expected");

            lexer->next_token();

            break;
        }
        case Lexer::FUNCTION: {
            lexer->next_token(true);

            lexer->functions.push_back(lexer->str_val);

            x = new Node(Node::FUNCTION_DEFINE);
            x->var_name = lexer->str_val;

            lexer->next_token();
            lexer->next_token();
            lexer->next_token();

            x->o2 = statement();

            break;
        }
        case Lexer::DEF: {
            lexer->next_token(true);
            lexer->vars.push_back(lexer->str_val);

            x = new Node(Node::DEF);
            x->var_name = lexer->str_val;

            lexer->next_token();

            if (lexer->sym == Lexer::AS) {
                lexer->next_token();
                x->value_type = Lexer::integer;
                x->o1 = sum();
            }

            break;
        }
        case Lexer::INPUT: {
            lexer->next_token();

            x = new Node(Node::INPUT);
            x->var_name = lexer->str_val;

            lexer->next_token();
            lexer->next_token();

            break;
        }
        case Lexer::PRINT: {
            lexer->next_token();

            x = new Node(Node::PRINT);
            x->o1 = sum();

            lexer->next_token();

            break;
        }
        case Lexer::RETURN: {
            lexer->next_token();

            x = new Node(Node::RETURN);
            x->o1 = sum();

            break;
        }
        case Lexer::SEMICOLON: {
            x = new Node(Node::EMPTY);
            lexer->next_token();

            break;
        }
        case Lexer::L_BRACKET: {
            x = new Node(Node::EMPTY);
            lexer->next_token();

            while (lexer->sym != Lexer::R_BRACKET) {
                t = x;
                x = new Node(Node::SEQ);

                x->o1 = t;
                x->o2 = statement();
            }
            lexer->next_token();

            break;
        }
        default: {
            x = new Node(Node::EXPR);
            x->o1 = expr();

            if (lexer->sym == Lexer::SEMICOLON)
                lexer->next_token();
            else error("';' expected");
        }
    }

    return x;
}

Node *Parser::parse() {
    lexer->next_token();

    Node *n = new Node(Node::PROG);
    n->o1 = statement(); // FIXME

    if (lexer->sym != Lexer::EOI)
        error("Invalid statement syntax");

    return n;
}
