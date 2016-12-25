//
// Created by NEzyaka on 26.09.16.
//

#include "lexer.h"
#include <iostream>
#include <cstring>
#include <string>

void Lexer::load(std::vector<char> c) {
    ch = ' ';
    code = c;
    iter = code.begin();
}

void Lexer::error(const std::string &e) {
    throw std::string(std::to_string(line) + " -> " + e);
}

void Lexer::getc() {
    ch = *iter;
    iter++;
}

void Lexer::next_token(bool ignore) {
    again:
    switch (ch) {
        case '\n':
        case '\r':
            line++;
        case ' ':
            getc();
            goto again;
        case EOF:
            sym = EOI;
            break;
        case ',':
            getc();
            sym = COMMA;
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
        case '.': {
            getc();
            sym = POINT;
            break;
        }
        case '#': {
            do { getc();
            } while (ch != EOF && ch != '\n' && ch != '\r');

            if (ch != EOF) {
                next_token();
            }

            break;
        }
        case '"': {
            std::string str;
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
            if (isdigit(ch) != 0) {
                std::string buf;

                while ((isdigit(ch) != 0) || ch == '.') {
                    buf += ch;
                    getc();
                }

                if (strchr(buf.c_str(), '.') != nullptr) {
                    sym = NUM_F;
                    float_val = std::stod(buf);
                }
                else {
                    sym = NUM_I;
                    int_val = std::stoi(buf);
                }
            } else if (isalpha(ch) != 0) {
                std::string str;

                while ((isalnum(ch) != 0) || ch == '_') {
                    str += ch;
                    getc();
                }

                sym = -1;

                for (auto &&item : WORDS) {
                    if (str == item.first) {
                        sym = item.second;
                    }
                }

                if (str == "index") {
                    sym = ID;
                    str_val = str;
                }

                for (auto &&item : vars) {
                    if (str == item.first) {
                        sym = ID;
                        str_val = str;
                    }
                }

                for (auto &&item : functions) {
                    if (str == item.first) {
                        sym = FUNCTION_ID;
                        str_val = str;
                    }
                }

                for (auto &&item : types) {
                    if (str == item.first) {
                        sym = USER_TYPE;
                        str_val = str;
                    }
                }

                if (sym == -1) {
                    if (ignore) {
                        sym = ID;
                        str_val = str;
                    } else {
                        error("'" + str + "' was not declared in this scope");
                    }
                }
            }
        }
    }
}

bool Lexer::var_defined(const std::string &name) {
    return !(this->vars.find(name) == std::cend(this->vars));
}

bool Lexer::arr_defined(const std::string &name) {
    return !(this->arrays.find(name) == std::cend(this->arrays));
}

bool Lexer::fn_defined(const std::string &name) {
    return !(this->functions.find(name) == std::cend(this->functions));
}

bool Lexer::type_defined(const std::string &name) {
    return !(this->types.find(name) == std::cend(this->types));
}
