//
// Created by NEzyaka on 26.09.16.
//

#ifndef TURNIP2_LEXER_H
#define TURNIP2_LEXER_H


#include <vector>
#include <fstream>
#include <map>

class Lexer {
    std::vector<char> code;
    std::vector<char>::iterator iter;

    char ch;

    void error(const std::string &e);
    void getc();

public:
    void load(std::vector<char> c);
    void next_token(bool ignore = false);

    int sym;
    int line = 1;

    int num_val;
    std::string str_val;

    std::vector<std::string> vars;
    std::vector<std::string> arrays;
    std::vector<std::string> functions;

    enum token_types {
        NUM, STR, ID, FUNCTION_ID,
        ARRAY, OF, INTEGER, FLOATING,
        IF, NOT, ELSE,
        WHILE, DO, REPEAT,
        NEW, DELETE,
        L_ACCESS, R_ACCESS,
        L_BRACKET, R_BRACKET,
        L_PARENT, R_PARENT,
        PLUS, MINUS, STAR, SLASH,
        LESS, MORE, IS, EQUAL, TYPE, SEMICOLON,
        PRINT, INPUT,
        FUNCTION, RETURN,
        COMMA, EOI
    };

    std::map<std::string, int> WORDS = {
            {"array", ARRAY},
            {"of", OF},
            {"int", INTEGER},
            {"float", FLOATING},
            {"if", IF},
            {"else", ELSE},
            {"while", WHILE},
            {"do", DO},
            {"repeat", REPEAT},
            {"new", NEW},
            {"del", DELETE},
            {"is", IS},
            {"not", NOT},
            {"print", PRINT},
            {"input", INPUT},
            {"function", FUNCTION},
            {"return", RETURN}
    };

};


#endif //TURNIP2_LEXER_H
