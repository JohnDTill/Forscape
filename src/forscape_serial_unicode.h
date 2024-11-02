#ifndef FORSCAPE_SERIAL_UNICODE_H
#define FORSCAPE_SERIAL_UNICODE_H

#include "forscape_serial.h"
#include "forscape_unicode.h"
#include "unicode_subscripts.h"
#include "unicode_superscripts.h"
#include <cassert>
#include <inttypes.h>
#include <string_view>

namespace Forscape {

inline bool writeSuperscript(std::string& out, std::string_view src){
    for(size_t index = 0; index < src.size(); index += codepointSize(src[index])){
        switch (codepointInt(src, index)) {
            FORSCAPE_SUPERSCRIPTS_CONVERSION
            case ' ': out += ' '; break;
            default: return false;
        }
    }

    return true;
}

inline bool writeSubscript(std::string& out, std::string_view src){
    for(size_t index = 0; index < src.size(); index += codepointSize(src[index])){
        switch (codepointInt(src, index)) {
            FORSCAPE_SUBSCRIPTS_CONVERSION
            case ' ': out += ' '; break;
            default: return false;
        }
    }

    return true;
}

inline bool convertToUnicode(std::string& out, std::string_view src, int8_t script){
    bool success = true;
    switch(script){
        case 0: out += src; break;
        case 1: success = writeSuperscript(out, src); break;
        case -1: success = writeSubscript(out, src); break;
        default: assert(false);
    }

    return success;
}

}

#endif // FORSCAPE_SERIAL_UNICODE_H
