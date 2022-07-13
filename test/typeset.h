#ifndef TYPESET_H
#define TYPESET_H

#include <typeset_construct.h>
#include <typeset_controller.h>
#include <typeset_line.h>
#include <typeset_model.h>
#include <typeset_text.h>

#ifndef HOPE_TYPESET_HEADLESS
#include <typeset_view.h>
#endif

#include <fstream>
#include <iostream>
#include <sstream>

using namespace Hope;

inline std::string readFile(const std::string& filename){
    std::ifstream in(filename);
    if(!in.is_open()) std::cout << "Failed to open " << filename << std::endl;
    assert(in.is_open());

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string str = buffer.str();
    for(size_t i = 1; i < str.size(); i++)
        if(str[i] == '\r' && str[i-1] != OPEN)
            str[i] = '\0';
    str.erase( std::remove(str.begin(), str.end(), '\0'), str.end() );

    return str;
}

inline Typeset::Model* loadModel(const std::string& filename){
    return Hope::Typeset::Model::fromSerial( readFile(filename) );
}

inline bool allTypesetElementsFreed(){
    return Typeset::Text::all.empty() && Typeset::Construct::all.empty();
}

#endif // TYPESET_H
