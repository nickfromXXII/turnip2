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
    int line = -1;

    int num_val;
    std::string str_val;

    enum data_types { null, integer, string };
    std::vector<std::string> vars;
    std::vector<std::string> functions;

    enum token_types {
        NUM, STR, ID, FUNCTION_ID, IF, NOT, ELSE, WHILE, DO, DEF, AS,
        L_BRACKET, R_BRACKET, L_PARENT, R_PARENT, QUOTE,
        PLUS, MINUS, STAR, SLASH, LESS, MORE, IS, EQUAL, SEMICOLON,
        EOI, PRINT, INPUT, FUNCTION, RETURN
    };

    std::map<std::string, int> WORDS = {
            {"if", IF},
            {"else", ELSE},
            {"do", DO},
            {"while", WHILE},
            {"is", IS},
            {"not", NOT},
            {"print", PRINT},
            {"input", INPUT},
            {"def", DEF},
            {"as", AS},
            {"function", FUNCTION},
            {"return", RETURN}
    };

};


#endif //TURNIP2_LEXER_H
