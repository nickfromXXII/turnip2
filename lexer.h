//
// Created by NEzyaka on 26.09.16.
//

#ifndef TURNIP2_LEXER_H
#define TURNIP2_LEXER_H

#include "location.h"
#include "utilities.h"

#include <vector>
#include <fstream>
#include <unordered_map>
#include <memory>

using namespace turnip2;

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

    std::unordered_map<std::string, std::shared_ptr<types::AbstractType>> types;
    std::unordered_map<std::string, std::shared_ptr<types::Type>> vars;
    std::unordered_map<std::string, std::shared_ptr<types::Type>> arrays;
    std::unordered_map<std::string, std::shared_ptr<types::Type>> functions;

    enum token_types {
        USER_TYPE, POINT, INHERIT,
        NUM_I, NUM_F, STR, ID, FUNCTION_ID,
        ARRAY, OF, INT, FLOAT, STRING, BOOL,
        CLASS, PRIVATE, PUBLIC, PROTECTED, OVERRIDE,
        IF, ELSE,
        AND, OR, NOT, TRUE, FALSE,
        WHILE, DO, REPEAT,
        VAR, DELETE,
        L_ACCESS, R_ACCESS,
        L_BRACKET, R_BRACKET,
        L_PARENT, R_PARENT,
        PLUS, MINUS, STAR, SLASH,
        LESS, MORE, IS, EQUAL, TYPE, SEMICOLON,
        PRINTLN, INPUT,
        FUNCTION, RETURN,
        COMMA, EOI
    };

    std::unordered_map<std::string, int> WORDS = {
        {"array",     ARRAY},
        {"of",        OF},
        {"int",       INT},
        {"float",     FLOAT},
        {"string",    STRING},
        {"bool",      BOOL},
        {"class",     CLASS},
        {"private",   PRIVATE},
        {"public",    PUBLIC},
        {"protected", PROTECTED},
        {"override",  OVERRIDE},
        {"if",        IF},
        {"else",      ELSE},
        {"while",     WHILE},
        {"do",        DO},
        {"repeat",    REPEAT},
        {"var",       VAR},
        {"del",       DELETE},
        {"is",        IS},
        {"and",       AND},
        {"or",        OR},
        {"not",       NOT},
        {"true",      TRUE},
        {"false",     FALSE},
        {"println",   PRINTLN},
        {"input",     INPUT},
        {"function",  FUNCTION},
        {"return",    RETURN}
    };

};


#endif //TURNIP2_LEXER_H
