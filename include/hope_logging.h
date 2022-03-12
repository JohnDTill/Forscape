#ifndef HOPE_LOGGING_H
#define HOPE_LOGGING_H

#include "spdlog/spdlog.h"

namespace Hope {

inline std::string cStr(const std::string& str){
    std::string out;
    out += '"';
    for(char ch : str){
        if(ch == '\n'){
            out += "\\n";
        }else{
            if(ch == '"' || ch == '\\') out += '\\';
            out += ch;
        }
    }
    out += '"';

    return out;
}

}

#endif // HOPE_LOGGING_H
