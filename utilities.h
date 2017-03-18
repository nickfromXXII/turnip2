//
// Created by NEzyaka on 28.09.16.
//

#ifndef TURNIP2_UTILITIES_H
#define TURNIP2_UTILITIES_H

#include "location.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace turnip2 {
    class Node;
    namespace types {
        struct Type {
            Type(int t, std::string n) {
                value_type = t;
                user_type_name = n;
            }

            int value_type;
            std::string user_type_name;
        };

        struct Member {
            Member(std::shared_ptr<Type> t, std::shared_ptr<Node> n, unsigned short a)
                : type(t), ast_node(n), access_type(a) {}

            std::shared_ptr<Type> type;
            std::shared_ptr<Node> ast_node;
            unsigned short access_type;
        };

        struct AbstractType {
            AbstractType(std::unordered_map<std::string, std::shared_ptr<Member>> p,
                         std::unordered_map<std::string, std::shared_ptr<Member>> m)
                : properties(p), methods(m) {}

            std::unordered_map<std::string, std::shared_ptr<Member>> properties;
            std::unordered_map<std::string, std::shared_ptr<Member>> methods;
        };
    }

    class Node {
    public:
        explicit Node(unsigned short k = 255, std::shared_ptr<class Node> op1 = nullptr,
                      std::shared_ptr<class Node> op2 = nullptr, std::shared_ptr<class Node> op3 = nullptr)
            : kind(k), o1(op1), o2(op2), o3(op3) {}

        Location location;

        enum node_type {
            VAR_ACCESS, CONST, ARG, ARRAY_ACCESS, FUNCTION_CALL,
            OBJECT_CONSTRUCT,
            PROPERTY_ACCESS, METHOD_CALL,
            FUNC_OBJ_PROPERTY_ACCESS, FUNC_OBJ_METHOD_CALL,
            ARRAY, ARG_LIST,
            ADD, SUB, MUL, DIV,
            LESS, MORE,
            LESS_EQUAL, MORE_EQUAL,
            EQUAL, NOT_EQUAL,
            SET, RETURN,
            IF, ELSE,
            AND, OR, NOT,
            DO, WHILE, REPEAT,
            VAR_DEF, INIT, DELETE,
            EMPTY, SEQ, EXPR,
            PRINTLN, INPUT,
            FUNCTION_DEFINE, CLASS_DEFINE
        };

        unsigned short kind;
        std::shared_ptr<class Node> o1, o2, o3;

        enum val_type {
            VOID, INTEGER, FLOATING, STRING, BOOL, USER
        };
        enum access_type {
            PRIVATE, PUBLIC, PROTECTED
        };

        int value_type = val_type::VOID;
        std::string user_type = "";

        int int_val = -1;
        double float_val = 0.0;
        std::string str_val = "";

        std::unordered_map<std::string, std::pair<int, std::shared_ptr<Node>>> class_def_properties;
        std::unordered_map<std::string, std::pair<int, std::shared_ptr<Node>>> class_def_methods;
        std::unordered_map<std::string, std::shared_ptr<types::Type>> func_def_args;
        std::vector<std::shared_ptr<Node>> func_call_args;

        std::string var_name = "";
        std::string property_name = "";
    };
}

#endif //TURNIP2_NODE_H
