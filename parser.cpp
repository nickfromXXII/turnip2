//
// Created by NEzyaka on 28.09.16.
//

#include "parser.h"

#include <iostream>
#include <algorithm>

void Parser::error(const std::string &e) {
    throw std::string(std::to_string(lexer->line) + " -> " + e);
}

std::shared_ptr<Node> Parser::term() {
    std::shared_ptr<Node> x;

    if (lexer->sym == Lexer::ID) {
        x = std::make_shared<Node>(Node::VAR_ACCESS);
        x->location = lexer->location;
        x->var_name = lexer->str_val;

        lexer->next_token();

        x->value_type = lexer->vars.at(x->var_name)->value_type;
        x->user_type = lexer->vars.at(x->var_name)->user_type_name;
        if (lexer->sym == Lexer::L_ACCESS) {
            x->kind = Node::ARRAY_ACCESS;
            lexer->next_token();
            x->o1 = sum();

            if (lexer->sym != Lexer::R_ACCESS) {
                std::cerr << lexer->sym << std::endl;
                error("expected ']'");
            }

            lexer->next_token();
        }
        else if (lexer->sym == Lexer::POINT) {
            x->kind = Node::PROPERTY_ACCESS;
            lexer->next_token(true);

            x->property_name = lexer->str_val;

            try {
                lexer->types.at(lexer->vars.at(x->var_name)->user_type_name)->properties.at(x->property_name);
            } catch (std::out_of_range) {
                try {
                    lexer->types.at(lexer->vars.at(x->var_name)->user_type_name)->methods.at(x->property_name);
                }
                catch(std::out_of_range) {
                    error(
                            "object '" +
                            x->var_name +
                            "' of class '" +
                            lexer->vars.at(x->var_name)->user_type_name +
                            "' has no member named '" +
                            x->property_name + "'"
                    );
                }
            }

            x->value_type = lexer->vars.at(x->var_name)->value_type;
            x->user_type = lexer->vars.at(x->var_name)->user_type_name;

            lexer->next_token();

            if (lexer->sym == Lexer::L_PARENT) {
                x->kind = Node::METHOD_CALL;

                x->value_type = lexer->types.at(x->user_type)->methods.at(x->property_name)->type->value_type;
                x->user_type = lexer->types.at(x->user_type)->methods.at(x->property_name)->type->user_type_name;

                lexer->next_token();
                while (true) {
                    if (lexer->sym == Lexer::R_PARENT) {
                        break;
                    }

                    x->func_call_args.emplace_back(sum());

                    if (lexer->sym == Lexer::R_PARENT) {
                        break;
                    }

                    if (lexer->sym != Lexer::COMMA) {
                        error("expected ',' or ')' in arguments list");
                    }

                    lexer->next_token();
                }

                if (lexer->sym != Lexer::R_PARENT) {
                    error("expected ')' in arguments list");
                }

                lexer->next_token();
            }
        }

    } else if (lexer->sym == Lexer::NUM_I) {
        x = std::make_shared<Node>(Node::CONST);
        x->location = lexer->location;
        x->value_type = Node::INTEGER;

        x->int_val = lexer->int_val;

        lexer->next_token();
    } else if (lexer->sym == Lexer::NUM_F) {
        x = std::make_shared<Node>(Node::CONST);
        x->location = lexer->location;
        x->value_type = Node::FLOATING;

        x->float_val = lexer->float_val;

        lexer->next_token();
    } else if (lexer->sym == Lexer::STR) {
        x = std::make_shared<Node>(Node::CONST);
        x->location = lexer->location;
        x->value_type = Node::STRING;

        x->str_val = lexer->str_val;

        lexer->next_token();
    } else if (lexer->sym == Lexer::TRUE) {
        x = std::make_shared<Node>(Node::CONST);
        x->location = lexer->location;
        x->value_type = Node::BOOL;

        x->int_val = true;

        lexer->next_token();
    } else if (lexer->sym == Lexer::FALSE) {
        x = std::make_shared<Node>(Node::CONST);
        x->location = lexer->location;
        x->value_type = Node::BOOL;

        x->int_val = false;

        lexer->next_token();
    } else if (lexer->sym == Lexer::FUNCTION_ID) {
        x = std::make_shared<Node>(Node::FUNCTION_CALL);
        x->location = lexer->location;
        x->value_type = lexer->functions.at(lexer->str_val)->value_type;
        x->user_type = lexer->functions.at(lexer->str_val)->user_type_name;
        x->var_name = lexer->str_val;

        lexer->next_token();
        if (lexer->sym != Lexer::L_PARENT) {
            error("expected '(' in arguments list");
        }

        lexer->next_token();
        while (true) {
            if (lexer->sym == Lexer::R_PARENT) {
                break;
            }

            x->func_call_args.emplace_back(sum());

            if (lexer->sym == Lexer::R_PARENT) {
                break;
            }

            if (lexer->sym != Lexer::COMMA) {
                error("expected ',' or ')' in arguments list");
            }

            lexer->next_token();
        }

        if (lexer->sym != Lexer::R_PARENT) {
            error("expected ')' in arguments list");
        }

        lexer->next_token();

        if (lexer->sym == Lexer::POINT) {
            std::shared_ptr<Node> t = std::move(x);

            x = std::make_shared<Node>(Node::FUNC_OBJ_PROPERTY_ACCESS);
            x->location = lexer->location;
            x->o1 = t;
            x->var_name = t->var_name;
            x->value_type = t->value_type;
            x->user_type = t->user_type;
            lexer->next_token(true);

            x->property_name = lexer->str_val;

            try {
                lexer->types.at(lexer->functions.at(x->var_name)->user_type_name)->properties.at(x->property_name);
            } catch (std::out_of_range) {
                try {
                    lexer->types.at(lexer->functions.at(x->var_name)->user_type_name)->methods.at(x->property_name);
                }
                catch(std::out_of_range) {
                    error(
                            "object returned by function '" +
                            x->var_name +
                            "' of class '" +
                            lexer->functions.at(x->var_name)->user_type_name +
                            "' has no member named '" +
                            x->property_name + "'"
                    );
                }
            }

            x->value_type = lexer->functions.at(x->var_name)->value_type;
            x->user_type = lexer->functions.at(x->var_name)->user_type_name;

            lexer->next_token();

            if (lexer->sym == Lexer::L_PARENT) {
                x->kind = Node::FUNC_OBJ_METHOD_CALL;

                x->value_type = lexer->types.at(x->user_type)->methods.at(x->property_name)->type->value_type;
                x->user_type = lexer->types.at(x->user_type)->methods.at(x->property_name)->type->user_type_name;

                lexer->next_token();
                while (true) {
                    if (lexer->sym == Lexer::R_PARENT) {
                        break;
                    }

                    x->func_call_args.emplace_back(sum());

                    if (lexer->sym == Lexer::R_PARENT) {
                        break;
                    }

                    if (lexer->sym != Lexer::COMMA) {
                        error("expected ',' or ')' in arguments list");
                    }

                    lexer->next_token();
                }

                if (lexer->sym != Lexer::R_PARENT) {
                    error("expected ')' in arguments list");
                }

                lexer->next_token();
            }
        }
    } else if (lexer->sym == Lexer::USER_TYPE) {
        x = std::make_shared<Node>(Node::OBJECT_CONSTRUCT);
        x->location = lexer->location;
        x->value_type = Node::USER;
        x->user_type = lexer->str_val;
        x->var_name = lexer->str_val;

        lexer->next_token();
        if (lexer->sym != Lexer::L_PARENT) {
            error("expected '(' in arguments list");
        }

        lexer->next_token();
        while (true) {
            if (lexer->sym == Lexer::R_PARENT) {
                break;
            }

            x->func_call_args.emplace_back(sum());

            if (lexer->sym == Lexer::R_PARENT) {
                break;
            }

            if (lexer->sym != Lexer::COMMA) {
                error("expected ',' or ')' in arguments list");
            }

            lexer->next_token();
        }

        if (lexer->sym != Lexer::R_PARENT) {
            error("expected ')' in arguments list");
        }

        lexer->next_token();

    } else {
        x = paren_expr();
    }

    if (lexer->sym == Lexer::STAR || lexer->sym == Lexer::SLASH) {
        int kind = 0;
        switch (lexer->sym) {
            case Lexer::STAR:
                kind = Node::MUL;
                break;
            case Lexer::SLASH:
                kind = Node::DIV;
                break;
        }
        lexer->next_token();

        auto t = x;
        x = std::make_shared<Node>(kind);
        x->location = lexer->location;
        x->o1 = t;
        x->o2 = term();
    }

    return x;
}

std::shared_ptr<Node> Parser::sum() {
    std::shared_ptr<Node> t, x = term();

    while (lexer->sym == Lexer::PLUS || lexer->sym == Lexer::MINUS) {
        t = x;

        unsigned short kind = 255;
        switch (lexer->sym) {
            case Lexer::PLUS: {
                kind = Node::ADD;
                break;
            }
            case Lexer::MINUS:
                kind = Node::SUB;
                break;
        }
        x = std::make_shared<Node>(kind);
        x->location = lexer->location;

        lexer->next_token();

        x->o1 = t;
        x->o2 = term();

        x->value_type = x->o2->value_type;
    }

    return x;
}

std::shared_ptr<Node> Parser::test() {
    std::shared_ptr<Node> t;
    std::shared_ptr<Node> x = sum();

    switch (lexer->sym) {
        case Lexer::LESS: {
            lexer->next_token();

            std::vector<std::shared_ptr<Node>> operands = { x };
            std::vector<unsigned> operations;

            if (lexer->sym == Lexer::EQUAL) {
                lexer->next_token();
                operations.emplace_back(Node::LESS_EQUAL);
            } else if (lexer->sym == Lexer::ID || lexer->sym == Lexer::FUNCTION_ID
                || lexer->sym == Lexer::NUM_I || lexer->sym == Lexer::NUM_F || lexer->sym == Lexer::STR) {
                operations.emplace_back(Node::LESS);
            } else {
                error("expected '=' or operand");
            }

            while (true) {
                operands.emplace_back(sum());

                if (lexer->sym == Lexer::LESS) {
                    lexer->next_token();

                    if (lexer->sym == Lexer::EQUAL) {
                        lexer->next_token();
                        operations.emplace_back(Node::LESS_EQUAL);
                    } else if (lexer->sym == Lexer::ID || lexer->sym == Lexer::FUNCTION_ID
                             || lexer->sym == Lexer::NUM_I || lexer->sym == Lexer::NUM_F || lexer->sym == Lexer::STR) {
                        operations.emplace_back(Node::LESS);
                    } else {
                        error("expected '=' or operand");
                    }
                    continue;
                }

                break;
            }

            std::vector<std::shared_ptr<Node>> tests;
            for (std::vector<std::shared_ptr<Node>>::size_type i = 0; i != operands.size()-1; i++) {
                tests.emplace_back(std::make_shared<Node>(operations.at(i), operands.at(i), operands.at(i+1)));
            }

            x = nullptr;
            for (auto &&test : tests) {
                if (x != nullptr) {
                    t = x;
                    x = std::make_shared<Node>(Node::AND, t, test);
                } else x = test;
            }

            break;
        }
        case Lexer::MORE: {
            lexer->next_token();

            std::vector<std::shared_ptr<Node>> operands = { x };
            std::vector<unsigned> operations;

            if (lexer->sym == Lexer::EQUAL) {
                lexer->next_token();
                operations.emplace_back(Node::MORE_EQUAL);
            } else if (lexer->sym == Lexer::ID || lexer->sym == Lexer::FUNCTION_ID
                       || lexer->sym == Lexer::NUM_I || lexer->sym == Lexer::NUM_F || lexer->sym == Lexer::STR) {
                operations.emplace_back(Node::MORE);
            } else {
                error("expected '=' or operand");
            }

            while (true) {
                operands.emplace_back(sum());

                if (lexer->sym == Lexer::MORE) {
                    lexer->next_token();

                    if (lexer->sym == Lexer::EQUAL) {
                        lexer->next_token();
                        operations.emplace_back(Node::MORE_EQUAL);
                    } else if (lexer->sym == Lexer::ID || lexer->sym == Lexer::FUNCTION_ID
                               || lexer->sym == Lexer::NUM_I || lexer->sym == Lexer::NUM_F || lexer->sym == Lexer::STR) {
                        operations.emplace_back(Node::MORE);
                    } else {
                        error("expected '=' or operand");
                    }

                    operations.emplace_back(Node::MORE);
                    continue;
                }

                break;
            }

            std::vector<std::shared_ptr<Node>> tests;
            for (std::vector<std::shared_ptr<Node>>::size_type i = 0; i != operands.size()-1; i++) {
                tests.emplace_back(std::make_shared<Node>(operations.at(i), operands.at(i), operands.at(i+1)));
            }

            x = nullptr;
            for (auto &&test : tests) {
                if (x != nullptr) {
                    t = x;
                    x = std::make_shared<Node>(Node::AND, t, test);
                } else x = test;
            }

            break;
        }
        case Lexer::IS: {
            t = x;
            x = std::make_shared<Node>(Node::EQUAL);
            x->location = lexer->location;

            lexer->next_token();

            if (lexer->sym == Lexer::NOT) {
                x = std::make_shared<Node>(Node::NOT_EQUAL);
                x->location = lexer->location;
                lexer->next_token();
            }

            x->o1 = t;
            x->o2 = sum();

            break;
        }
    }

    return x;
}

std::shared_ptr<Node> Parser::expr() {
    std::shared_ptr<Node> t, x;

    bool _not = false;
    if (lexer->sym == Lexer::NOT) {
        _not = true;
        lexer->next_token();
    }

    if (lexer->sym != Lexer::ID) {
        if (_not) {
            auto n = std::make_shared<Node>(Node::NOT);
            x->location = lexer->location;
            n->o1 = test();
            return n;
        } else {
            return test();
        }
    }

    x = test();
    if (_not) {
        t = x;
        x = std::make_shared<Node>(Node::NOT);
        x->location = lexer->location;
        x->o1 = t;
    }

    if (lexer->sym == Lexer::AND) {
        lexer->next_token();
        t = x;
        x = std::make_shared<Node>(Node::AND);
        x->location = lexer->location;
        x->o1 = t;
        x->o2 = expr();
    }

    if (lexer->sym == Lexer::OR) {
        lexer->next_token();
        t = x;
        x = std::make_shared<Node>(Node::OR);
        x->location = lexer->location;
        x->o1 = t;
        x->o2 = expr();
    }

    if (x->kind == Node::VAR_ACCESS || x->kind == Node::ARRAY_ACCESS || x->kind == Node::PROPERTY_ACCESS) {
        if (lexer->sym == Lexer::EQUAL) {
            t = x;
            x = std::make_shared<Node>(Node::SET);
            x->location = lexer->location;

            lexer->next_token();

            x->var_name = t->var_name;

            if (t->kind == Node::ARRAY_ACCESS) {
                x->o2 = t->o1;

            } else if (t->kind == Node::PROPERTY_ACCESS) {
                x->property_name = t->property_name;

                try {
                    lexer->types.at(lexer->vars.at(x->var_name)->user_type_name)->properties.at(x->property_name);
                } catch (std::out_of_range) {
                    error(
                            "object '" +
                                    x->var_name +
                                    "' of class '" +
                                    lexer->vars.at(x->var_name)->user_type_name +
                                    "' has no member named '" +
                                    x->property_name + "'"
                    );
                }
            }

            x->o1 = expr();
            x->value_type = t->value_type;
            x->user_type = t->user_type;
        }
    }

    return x;
}

std::shared_ptr<Node> Parser::var_def(bool isClassProperty) {
    if (!isClassProperty) {
        lexer->next_token(true);
    }

    std::shared_ptr<Node> x = std::make_shared<Node>(Node::VAR_DEF);
    x->location = lexer->location;

    std::string var_name = lexer->str_val;
    x->var_name = var_name;
    last_vars.emplace_back(var_name);

    lexer->next_token();
    if (lexer->sym != Lexer::TYPE) {
        error("expected variable type");
    }

    lexer->next_token();
    if (lexer->sym != Lexer::INT && lexer->sym != Lexer::FLOAT
        && lexer->sym != Lexer::STRING && lexer->sym != Lexer::BOOL
        && lexer->sym != Lexer::USER_TYPE) {
        error("expected variable type");
    }

    switch (lexer->sym) {
        case Lexer::INT:
            x->value_type = Node::INTEGER;
            break;
        case Lexer::FLOAT:
            x->value_type = Node::FLOATING;
            break;
        case Lexer::STRING:
            x->value_type = Node::STRING;
            break;
        case Lexer::BOOL:
            x->value_type = Node::BOOL;
            break;
        case Lexer::USER_TYPE:
            x->value_type = Node::USER;
            x->user_type = lexer->str_val;
            break;
    }

    if (!isClassProperty) {
        lexer->next_token();
        if (lexer->sym == Lexer::EQUAL) {
            lexer->next_token();
            if (lexer->sym == Lexer::ARRAY) {
                if (lexer->arr_defined(var_name))
                    error("'" + var_name + "' is already defined");

                lexer->next_token();
                std::shared_ptr<Node> arr(new Node(Node::ARRAY));

                if (lexer->sym != Lexer::OF) {
                    error("expected array size");
                }

                lexer->next_token();
                arr->value_type = x->value_type;
                arr->user_type = x->user_type;
                arr->o1 = sum();

                x->o1 = arr;

                lexer->arrays.emplace(var_name, std::make_shared<types::Type>(x->value_type, x->user_type));
            } else {
                x->kind = Node::INIT;
                x->o1 = expr();
            }
        }
        lexer->vars.emplace(var_name, std::make_shared<types::Type>(x->value_type, x->user_type));
    }

    if (lexer->sym != Lexer::SEMICOLON) {
        if (!(isClassProperty && (lexer->sym == Lexer::INT ||lexer->sym == Lexer::FLOAT || lexer->sym == Lexer::STRING || lexer->sym == Lexer::BOOL || lexer->sym == Lexer::USER_TYPE))) {
            error("expected ';'");
        }
    }

    return x;
}

std::shared_ptr<Node> Parser::function_arg() {
    std::shared_ptr<Node> n;

    lexer->next_token(true);

    if (lexer->sym == Lexer::R_PARENT) {
        return std::make_shared<Node>(Node::EMPTY);
    }

    std::string var_name = lexer->str_val;

    n = std::make_shared<Node>(Node::ARG);
    n->location = lexer->location;
    n->var_name = var_name;

    lexer->next_token();
    if (lexer->sym != Lexer::TYPE) {
        std::cout << lexer->sym << std::endl;
        error("expected argument type");
    }

    lexer->next_token();
    if (lexer->sym != Lexer::INT && lexer->sym != Lexer::FLOAT
        && lexer->sym != Lexer::STRING && lexer->sym != Lexer::BOOL
        && lexer->sym != Lexer::USER_TYPE) {
        std::cout << lexer->sym << std::endl;
        error("expected argument type");
    }

    switch (lexer->sym) {
        case Lexer::INT:
            n->value_type = Node::INTEGER;
            break;
        case Lexer::FLOAT:
            n->value_type = Node::FLOATING;
            break;
        case Lexer::STRING:
            n->value_type = Node::STRING;
            break;
        case Lexer::BOOL:
            n->value_type = Node::BOOL;
            break;
        case Lexer::USER_TYPE:
            n->value_type = Node::USER;
            n->user_type = lexer->str_val;
            break;
    }

    lexer->vars[var_name] = std::make_shared<types::Type>(types::Type(n->value_type, n->user_type));
    lexer->next_token();

    return n;
}

std::shared_ptr<Node> Parser::paren_expr() {
    if (lexer->sym != Lexer::L_PARENT) {
        std::cerr << lexer->sym << std::endl;
        error("expected '('");
    }

    lexer->next_token();
    std::shared_ptr<Node> n = expr();

    if (lexer->sym != Lexer::R_PARENT) {
        error("expected ')'");
    }

    lexer->next_token();
    return n;
}

std::shared_ptr<Node> Parser::function_args() {
    std::shared_ptr<Node> n(new Node(Node::ARG_LIST));

    if (lexer->sym != Lexer::L_PARENT) {
        std::cerr << lexer->sym << std::endl;
        error("expected '(' in arguments list");
    }

    while (lexer->sym != Lexer::R_PARENT) {
        std::shared_ptr<Node> arg = function_arg();
        if (arg->kind == Node::EMPTY) {
            break;
        }

        n->func_def_args.emplace(arg->var_name, std::make_shared<types::Type>(arg->value_type, arg->user_type));
    }

    if (lexer->sym != Lexer::R_PARENT) {
        std::cerr << lexer->sym << std::endl;
        error("expected ')' in arguments list");
    }

    lexer->next_token();
    return n;
}

std::shared_ptr<Node> Parser::function_def() {
    lexer->next_token(true); // eat 'function' keyword
    std::string func_name = lexer->str_val;

    if (lexer->fn_defined(func_name))
        error("function '" + func_name + "' is already defined");

    std::shared_ptr<Node> x = std::make_shared<Node>(Node::FUNCTION_DEFINE);
    x->location = lexer->location;
    x->var_name = func_name;

    lexer->next_token();
    x->o1 = function_args();

    if (lexer->sym != Lexer::TYPE) {
        x->value_type = Node::VOID;
    } else {
        lexer->next_token();

        if (lexer->sym != Lexer::INT && lexer->sym != Lexer::FLOAT
            && lexer->sym != Lexer::STRING && lexer->sym != Lexer::BOOL
            && lexer->sym != Lexer::USER_TYPE) {
            error("expected type of return value");
        }

        switch (lexer->sym) {
            case Lexer::INT:
                x->value_type = Node::INTEGER;
                break;
            case Lexer::FLOAT:
                x->value_type = Node::FLOATING;
                break;
            case Lexer::STRING:
                x->value_type = Node::STRING;
                break;
            case Lexer::BOOL:
                x->value_type = Node::BOOL;
                break;
            case Lexer::USER_TYPE:
                x->value_type = Node::USER;
                x->user_type = lexer->str_val;
                break;
        }
        lexer->next_token();
    }
    lexer->functions.emplace(func_name, std::make_shared<types::Type>(x->value_type, x->user_type));

    x->o2 = statement();

    for (auto &&var : last_vars) {
        lexer->vars.erase(var);
        last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
    }

    return x;
}

std::shared_ptr<Node> Parser::statement() {
    std::shared_ptr<Node> t, x;

    switch (lexer->sym) {
        case Lexer::CLASS: {
            x = std::make_shared<Node>(Node::CLASS_DEFINE);
            x->location = lexer->location;

            lexer->next_token(true);
            std::string class_name = lexer->str_val;

            if (lexer->type_defined(class_name))
                error("type '" + class_name + "' is already defined");

            x->var_name = class_name;
            lexer->vars.emplace("this", std::make_shared<types::Type>(Node::USER, class_name));

            std::unordered_map<std::string, std::shared_ptr<types::Member>> properties;
            std::unordered_map<std::string, std::shared_ptr<types::Member>> methods;
            lexer->types.emplace(class_name, std::make_shared<types::AbstractType>(properties, methods));

            lexer->next_token();
            if (lexer->sym != Lexer::L_BRACKET) {
                if (lexer->sym == Lexer::INHERIT) {
                    lexer->next_token();
                    std::string base_class_name = lexer->str_val;
                    lexer->next_token();

                    if (lexer->sym != Lexer::L_BRACKET) {
                        error("expected '{'");
                    }

                    //x = std::make_shared<Node>(Node::CLASS_INHERIT);
                    //x->kind = Node::CLASS_INHERIT;
                    //x->var_name = class_name;
                    //x->property_name = base_class_name;

                    auto base_class = lexer->types.at(base_class_name);
                    lexer->types.at(class_name) = base_class;
                    properties = base_class->properties;
                    methods = base_class->methods;
                    methods.erase(base_class_name);

                    for (auto &&property : properties) {
                        x->class_def_properties.emplace(
                            property.first,
                            std::make_pair(
                                base_class->properties.at(property.first)->access_type,
                                base_class->properties.at(property.first)->ast_node
                            )
                        );
                    }
                    for (auto &&method : methods) {
                        for (auto &&iter : method.second->ast_node->func_def_args) {
                            if (iter.first == "this") {
                                iter.second->user_type_name = class_name;
                            }
                        }
                        x->class_def_methods.emplace_hint(
                            std::begin(x->class_def_methods),
                            method.first,
                            std::make_pair(
                                base_class->methods.at(method.first)->access_type,
                                base_class->methods.at(method.first)->ast_node
                            )
                        );
                    }
                } else {
                    error("expected '{'");
                }
            }
            lexer->next_token(true);

            while (true) {
                int access_type = Node::PRIVATE;
                if (lexer->sym == Lexer::PRIVATE || lexer->sym == Lexer::PUBLIC || lexer->sym == Lexer::PROTECTED) {
                    switch (lexer->sym)
                    {
                        case Lexer::PRIVATE:
                            access_type = Node::PRIVATE;
                            break;
                        case Lexer::PUBLIC:
                            access_type = Node::PUBLIC;
                            break;
                        case Lexer::PROTECTED:
                            access_type = Node::PROTECTED;
                            break;
                    }
                    lexer->next_token(true);
                }

                if (lexer->sym == Lexer::FUNCTION) {
                    std::shared_ptr<Node> method_node = function_def();
                    std::shared_ptr<types::Member> method = std::make_shared<types::Member>(
                        std::make_shared<types::Type>(method_node->value_type, method_node->user_type),
                        method_node,
                        access_type
                    );
                    x->class_def_methods.emplace(method_node->var_name, std::make_pair(access_type, method_node));
                    methods.emplace_hint(std::end(methods), method_node->var_name, method);
                    lexer->types.at(class_name)->methods = methods;
                }
                else if (lexer->sym == Lexer::ID) {
                    std::shared_ptr<Node> property_node = var_def(true);
                    std::shared_ptr<types::Member> property = std::make_shared<types::Member>(
                        std::make_shared<types::Type>(property_node->value_type, property_node->user_type),
                        property_node,
                        access_type
                    );
                    x->class_def_properties.emplace(property_node->var_name, std::make_pair(access_type, property_node));
                    properties.emplace(property_node->var_name, property);
                    lexer->types.at(class_name)->properties = properties;

                    lexer->next_token();
                    if (lexer->sym != Lexer::SEMICOLON) {
                        error("expected ';'");
                    }
                    lexer->next_token();
                } else if (lexer->sym == Lexer::R_BRACKET) {
                    lexer->next_token();
                    if (lexer->sym != Lexer::SEMICOLON) {
                        error("expected ';'");
                    }

                    lexer->next_token();
                    break;
                }
            }
            lexer->vars.erase("this");
            break;
        }
        case Lexer::IF: {
            x = std::make_shared<Node>(Node::IF);
            x->location = lexer->location;
            lexer->next_token();

            x->o1 = expr(); //paren_expr();

            std::vector<std::string> _temp(last_vars);
            last_vars.clear();

            x->o2 = statement();

            for (auto &&var : last_vars) {
                lexer->vars.erase(var);
                last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
            }
            last_vars = _temp;

            if (lexer->sym == Lexer::ELSE) {
                x->kind = Node::ELSE;
                lexer->next_token();

                std::vector<std::string> __temp(last_vars);
                last_vars.clear();

                x->o3 = statement();

                for (auto &&var : last_vars) {
                    lexer->vars.erase(var);
                    last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), var));
                }
                last_vars = __temp;
            }

            break;
        }
        case Lexer::WHILE: {
            x = std::make_shared<Node>(Node::WHILE);
            x->location = lexer->location;
            lexer->next_token();

            x->o1 = expr(); //paren_expr();
            x->o2 = statement();

            break;
        }
        case Lexer::DO: {
            x = std::make_shared<Node>(Node::DO);
            x->location = lexer->location;
            lexer->next_token();

            x->o1 = statement();

            if (lexer->sym != Lexer::WHILE) {
                error("expected 'while'");
            }

            lexer->next_token();

            x->o2 = expr(); //paren_expr();

            if (lexer->sym != Lexer::SEMICOLON) {
                error("expected ';'");
            }

            lexer->next_token();

            break;
        }
        case Lexer::REPEAT: {
            x = std::make_shared<Node>(Node::REPEAT);
            x->location = lexer->location;
            lexer->next_token();

            x->o1 = sum(); //paren_expr();
            lexer->vars.try_emplace("index", std::make_shared<types::Type>(Node::INTEGER, ""));
            x->o2 = statement();

            break;
        }
        case Lexer::FUNCTION: {
            x = function_def();
            break;
        }
        case Lexer::VAR: {
            x = var_def();
            break;
        }
        case Lexer::DELETE: {
            lexer->next_token();
            last_vars.erase(std::find(std::cbegin(last_vars), std::cend(last_vars), lexer->str_val));
            lexer->vars.erase(lexer->str_val);

            x = std::make_shared<Node>(Node::DELETE);
            x->location = lexer->location;
            x->var_name = lexer->str_val;

            lexer->next_token();

            break;
        }
        case Lexer::INPUT: {
            lexer->next_token();

            x = std::make_shared<Node>(Node::INPUT);
            x->location = lexer->location;
            x->var_name = lexer->str_val;

            lexer->next_token();

            if (lexer->sym != Lexer::SEMICOLON) {
                error("expected ';'");
            }

            lexer->next_token();

            break;
        }
        case Lexer::PRINTLN: {
            lexer->next_token();

            x = std::make_shared<Node>(Node::PRINTLN);
            x->location = lexer->location;
            x->o1 = sum();

            lexer->next_token();

            break;
        }
        case Lexer::RETURN: {
            lexer->next_token();

            x = std::make_shared<Node>(Node::RETURN);
            x->location = lexer->location;
            x->o1 = sum();

            break;
        }
        case Lexer::SEMICOLON: {
            x = std::make_shared<Node>(Node::EMPTY);
            x->location = lexer->location;
            lexer->next_token();

            break;
        }
        case Lexer::L_BRACKET: {
            x = std::make_shared<Node>(Node::EMPTY);
            x->location = lexer->location;
            lexer->next_token();

            while (lexer->sym != Lexer::R_BRACKET) {
                t = x;
                x = std::make_shared<Node>(Node::SEQ);
                x->location = lexer->location;

                x->o1 = t;
                x->o2 = statement();
            }
            lexer->next_token();

            break;
        }
        default: {
            x = std::make_shared<Node>(Node::EXPR);
            x->location = lexer->location;
            x->o1 = expr();

            if (lexer->sym == Lexer::SEMICOLON) {
                lexer->next_token();
            } else {
                error("expected ';'");
            }
        }
    }

    return x;
}

std::shared_ptr<Node> Parser::parse() {
    std::shared_ptr<Node> t, x;

    x = std::make_shared<Node>(Node::EMPTY);
    x->location = lexer->location;
    lexer->next_token();

    while (lexer->sym != Lexer::EOI) {
        t = x;
        x = std::make_shared<Node>(Node::SEQ);
        x->location = lexer->location;

        x->o1 = t;
        x->o2 = statement();
    }

    if (lexer->sym != Lexer::EOI) {
        error("Invalid statement syntax");
    }

    return x;
}
