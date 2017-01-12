//
// Created by NEzyaka on 13.11.16.
//

#ifndef TURNIP2_TYPE_H
#define TURNIP2_TYPE_H

#include <string>

struct type {
    type(int t, std::string n) {
        value_type = t;
        user_type_name = n;
    }
    int value_type;
    std::string user_type_name;
};

#endif //TURNIP2_TYPE_H
