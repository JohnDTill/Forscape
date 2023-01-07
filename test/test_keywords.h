#ifndef TYPESET_KEYWORDS_H
#define TYPESET_KEYWORDS_H

#include "report.h"
#include <forscape_serial.h>
#include <typeset_keywords.h>

using namespace Forscape;

inline bool testKeywords(){
    bool passing = true;

    std::string failing;

    for(const auto& entry : Typeset::Keywords::map){
        bool valid = isValidSerial(entry.second);
        passing &= valid;
        if(!valid) failing += entry.first + " => " + entry.second + '\n';
    }

    if(!passing) printf("The following keywords are not valid:\n%s", failing.c_str());

    report("Typeset keywords", passing);
    return passing;
}


#endif // TYPESET_KEYWORDS_H
