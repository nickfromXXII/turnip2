//
// Created by NEzyaka on 28.09.16.
//

#include "parser.h"

#include <iostream>
#include <algorithm>
#include <stdlib.h>

void Parser::error(const std::string &e) {
    throw std::string("Parse error at line " + std::to_string(lexer->line) + ": " + e);
}

Node *Parser::term() {
    Node *x;

    if (lexer->sym == Lexer::ID) {
        x = new Node(Node::VAR);
        x->var_name = lexer->str_val;

        lexer->next_token();
        if (lexer->sym == Lexer::L_ACCESS) {
            x->kind = Node::ARRAY_ACCESS;
            lexer->next_token();
            x->o1 = sum();

            if (lexer->sym != Lexer::R_ACCESS) {
                std::cerr << lexer->sym << std::endl;
                error("Expected ']'");
            }

            lexer->next_token();
            x->value_type = Node::integer;
        }

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

        x->value_type = x->o2->value_type;
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

    if (x->kind == Node::VAR || x->kind == Node::ARRAY_ACCESS) {
        if (lexer->sym == Lexer::EQUAL) {
            t = x;
            x = new Node(Node::SET);

            lexer->next_token();

            x->var_name = t->var_name;

            if (t->kind == Node::ARRAY_ACCESS)
                x->o2 = t->o1;

            x->o1 = expr();
            x->value_type = Node::integer;
        }
    }

    return x;
}

Node *Parser::function_arg() {
    Node *n;

    lexer->next_token(true);
    lexer->vars.push_back(lexer->str_val);

    n = new Node(Node::ARG);

    std::string var_name = lexer->str_val;
    n->var_name = var_name;

    lexer->next_token();
    if (lexer->sym != Lexer::TYPE)
        error("expected variable type");

    lexer->next_token();
    if (lexer->sym != Lexer::INTEGER && lexer->sym != Lexer::FLOATING)
        error("expected variable type");

    switch (lexer->sym) {
        case Lexer::INTEGER:
            n->value_type = Node::integer;
            break;
        case Lexer::FLOATING:
            n->value_type = Node::floating;
            break;
    }

    lexer->next_token();

    return n;
}

Node *Parser::paren_expr() {
    if (lexer->sym != Lexer::L_PARENT) {
        std::cerr << lexer->sym << std::endl;
        error("'(' expected");
    }

    lexer->next_token();
    Node *n = expr();

    if (lexer->sym != Lexer::R_PARENT)
        error("') expected");

    lexer->next_token();
    return n;
}

Node *Parser::function_args() {
    Node *n = new Node(Node::ARG_LIST);

    if (lexer->sym != Lexer::L_PARENT) {
        std::cerr << lexer->sym << std::endl;
        error("'(' expected");
    }

    //while (lexer->sym == ) {
        Node *arg = function_arg();
        n->args.insert(std::pair<std::string, int>(arg->var_name, arg->value_type));
    //}

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

            x->o1 = expr(); //paren_expr();
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

            x->o1 = expr(); //paren_expr();
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

            x->o2 = expr(); //paren_expr();

            if (lexer->sym != Lexer::SEMICOLON)
                error("';' expected");

            lexer->next_token();

            break;
        }
        case Lexer::REPEAT: {
            x = new Node(Node::REPEAT);
            lexer->next_token();

            x->o1 = sum(); //paren_expr();
            x->o2 = statement();

            break;
        }
        case Lexer::FUNCTION: {
            lexer->next_token(true);

            lexer->functions.push_back(lexer->str_val);

            x = new Node(Node::FUNCTION_DEFINE);
            x->var_name = lexer->str_val;

            lexer->next_token();
            x->o1 = function_args();

            if (lexer->sym != Lexer::TYPE)
                x->value_type = Node::null;
            else {
                lexer->next_token();

                if (lexer->sym != Lexer::INTEGER && lexer->sym != Lexer::FLOATING)
                    error("expected type of return value");

                switch (lexer->sym) {
                    case Lexer::INTEGER:
                        x->value_type = Node::integer;
                        break;
                    case Lexer::FLOATING:
                        x->value_type = Node::floating;
                        break;
                }
                lexer->next_token();
            }

            x->o2 = statement();
            break;
        }
        case Lexer::NEW: {
            lexer->next_token(true);
            lexer->vars.push_back(lexer->str_val);

            x = new Node(Node::NEW);

            std::string var_name = lexer->str_val;
            x->var_name = var_name;

            lexer->next_token();
            if (lexer->sym != Lexer::TYPE)
                error("expected variable type");

            lexer->next_token();
            if (lexer->sym != Lexer::INTEGER && lexer->sym != Lexer::FLOATING)
                error("expected variable type");

            switch (lexer->sym) {
                case Lexer::INTEGER:
                    x->value_type = Node::integer;
                    break;
                case Lexer::FLOATING:
                    x->value_type = Node::floating;
                    break;
            }

            lexer->next_token();
            if (lexer->sym == Lexer::EQUAL) {
                lexer->next_token();
                if (lexer->sym == Lexer::ARRAY) {
                    lexer->arrays.push_back(var_name);
                    lexer->next_token();
                    Node *arr = new Node(Node::ARRAY);

                    if (lexer->sym != Lexer::OF)
                        error("expected array size");

                    lexer->next_token();
                    arr->value_type = x->value_type;
                    arr->o1 = sum();

                    x->o1 = arr;
                } else {
                    lexer->vars.push_back(var_name);
                    x->kind = Node::INIT;
                    x->o1 = sum();
                }
            }

            break;
        }
        case Lexer::DELETE: {
            lexer->next_token();
            lexer->vars.erase(std::remove(lexer->vars.begin(), lexer->vars.end(), lexer->str_val), lexer->vars.end());

            x = new Node(Node::DELETE);
            x->var_name = lexer->str_val;

            lexer->next_token();

            break;
        }
        case Lexer::INPUT: {
            lexer->next_token();

            x = new Node(Node::INPUT);
            x->var_name = lexer->str_val;

            lexer->next_token();

            if (lexer->sym != Lexer::SEMICOLON)
                error("';' expected");

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
