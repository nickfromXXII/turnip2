//
// Created by NEzyaka on 26.09.16.
//

#ifndef TURNIP2_LEXER_H
#define TURNIP2_LEXER_H

#include "type.h"
#include "location.h"

#include <vector>
#include <fstream>
#include <map>
#include <memory>

class Lexer {
    std::vector<char> code;
    std::vector<char>::iterator iter;

    char ch;

    void error(const std::string &e);
    void getc();

public:
    void load(std::vector<char> c);
    void next_token(bool ignore = false);
    bool var_defined(const std::string &name);
    bool arr_defined(const std::string &name);
    bool fn_defined(const std::string &name);
    bool type_defined(const std::string &name);

    int sym;

    unsigned line = 1;
    unsigned column = 1;
    Location location;

    int int_val;
    double float_val;
    std::string str_val;

    std::map<std::string, std::pair<std::map<std::string, std::shared_ptr<type>>, std::map<std::string, std::shared_ptr<type>>>> types;
    std::map<std::string, std::shared_ptr<type>> vars;
    std::map<std::string, std::shared_ptr<type>> arrays;
    std::map<std::string, std::shared_ptr<type>> functions;

    enum token_types {
        USER_TYPE, POINT,
        NUM_I, NUM_F, STR, ID, FUNCTION_ID,
        ARRAY, OF, INT, FLOAT,
        CLASS, PRIVATE, PUBLIC, PROTECTED,
        IF, ELSE,
        AND, OR, NOT,
        WHILE, DO, REPEAT,
        NEW, DELETE,
        L_ACCESS, R_ACCESS,
        L_BRACKET, R_BRACKET,
        L_PARENT, R_PARENT,
        PLUS, MINUS, STAR, SLASH,
        LESS, MORE, IS, EQUAL, TYPE, SEMICOLON,
        PRINTLN, INPUT,
        FUNCTION, RETURN,
        COMMA, EOI
    };

    std::map<std::string, int> WORDS = {
            {"array", ARRAY},
            {"of", OF},
            {"int", INT},
            {"float", FLOAT},
            {"class", CLASS},
            {"private", PRIVATE},
            {"public", PUBLIC},
            {"protected", PROTECTED},
            {"if", IF},
            {"else", ELSE},
            {"while", WHILE},
            {"do", DO},
            {"repeat", REPEAT},
            {"new", NEW},
            {"del", DELETE},
            {"is", IS},
            {"and", AND},
            {"or", OR},
            {"not", NOT},
            {"println", PRINTLN},
            {"input", INPUT},
            {"function", FUNCTION},
            {"return", RETURN}
    };

};


#endif //TURNIP2_LEXER_H
