//
// Created by NEzyaka on 26.09.16.
//

#include "lexer.h"
#include <iostream>
#include <stdlib.h>

void Lexer::load(std::vector<char> c) {
    ch = ' ';
    code = c;
    iter = code.begin();
}

void Lexer::error(const std::string &e) {
    std::cout << "Analyze error at line " << std::to_string(line) << ": " << e << std::endl;
    exit(1);
}

void Lexer::getc() {
    ch = *iter;
    iter++;
}

void Lexer::next_token(bool ignore) {
    again:
    switch (ch) {
        case '\n': // FIXME
        case '\r':
            line++;
        case ' ':
            getc();
            goto again;
        case EOF:
            sym = EOI;
            break;
        case '{':
            getc();
            sym = L_BRACKET;
            break;
        case '}':
            getc();
            sym = R_BRACKET;
            break;
        case '(':
            getc();
            sym = L_PARENT;
            break;
        case ')':
            getc();
            sym = R_PARENT;
            break;
        case '+':
            getc();
            sym = PLUS;
            break;
        case '-':
            getc();
            sym = MINUS;
            break;
        case '*':
            getc();
            sym = STAR;
            break;
        case '/':
            getc();
            sym = SLASH;
            break;
        case '<':
            getc();
            sym = LESS;
            break;
        case '>':
            getc();
            sym = MORE;
            break;
        case '=':
            getc();
            sym = EQUAL;
            break;
        case ';':
            getc();
            sym = SEMICOLON;
            break;
        case '[': {
            getc();
            sym = L_ACCESS;
            break;
        }
        case ']': {
            getc();
            sym = R_ACCESS;
            break;
        }
        case ':': {
            getc();
            sym = TYPE;
            break;
        }
        case '"': {
            std::string str = "";
            getc();

            while (ch != '"') {
                str += ch;
                getc();
            }

            sym = STR;
            str_val = str;

            getc();
            break;
        }
        default: {
            if (isdigit(ch)) {
                int i = 0;

                while (isdigit(ch)) {
                    i = i * 10 + (ch - '0');
                    getc();
                }

                sym = NUM;
                num_val = i;
            } else if (isalpha(ch)) {
                std::string str = "";

                while (isalnum(ch) || ch == '_') {
                    str += ch;
                    getc();
                }

                sym = -1;

                for (auto &&item : WORDS) {
                    if (str == item.first)
                        sym = item.second;
                }

                if (sym == -1) {
                    if (str == "index") {
                        sym = ID;
                        str_val = str;
                    }

                    for (auto &&item : vars) {
                        if (str == item)
                            sym = ID;
                            str_val = str;
                    }

                    if (sym == -1) {
                        if (ignore) {
                            sym = ID;
                            str_val = str;
                        }
                    }
                }
            }
        }
    }
}
