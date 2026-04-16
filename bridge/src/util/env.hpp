#include <cstdlib>
#include <string>

std::string sgetenv(std::string key) {
    if(key.empty()) return "";

    char* res = getenv(key.c_str());
    if(res == NULL) {
        return "";
    }

    return std::string(res);
}

#pragma once