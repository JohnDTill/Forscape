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

    return buffer.str();
}

inline Typeset::Model* loadModel(const std::string& filename){
    return Hope::Typeset::Model::fromSerial( readFile(filename) );
}

inline bool allTypesetElementsFreed(){
    return Typeset::Text::all.empty() && Typeset::Construct::all.empty();
}

#endif // TYPESET_H
