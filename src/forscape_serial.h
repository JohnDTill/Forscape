#ifndef FORSCAPE_SERIAL_H
#define FORSCAPE_SERIAL_H

#include "construct_codes.h"
#include <cassert>
#include <inttypes.h>
#include <limits>
#include <string>

#ifndef NDEBUG
#include "forscape_unicode.h"
#endif

#include <iostream>  // DO THIS: delete

namespace Forscape {

#define PARSE_DIM(name, terminator) \
    uint16_t name;\
    {\
        const char first_char = src[index++];\
        if(first_char < '0' || first_char > '9') return false;\
        const char second_char = src[index++];\
        if(second_char == terminator) name = (first_char - '0');\
        else{\
            if(second_char < '0' || second_char > '9' || src[index++] != terminator) return false;\
            name = (first_char - '0') * 10 + (second_char - '0');\
        }\
    }

inline bool isValidSerial(const std::string& src) noexcept {
    assert(!isIllFormedUtf8(src));

    uint32_t depth = 0;
    size_t index = 0;
    while(index < src.size()){
        if(isUnicodeChar<CONSTRUCT_STRVIEW>(&src[index])){
            index += codepointSize(CON_0);
            if(index >= src.size()) return false;
            // Escape characters
            else if(isUnicodeChar<CONSTRUCT_STRVIEW>(&src[index])) index += codepointSize(CON_0);
            else if(isUnicodeChar<OPEN_STRVIEW>(&src[index])) index += codepointSize(OPEN_0);
            else if(isUnicodeChar<CLOSE_STRVIEW>(&src[index])) index += codepointSize(CLOSE_0);
            // Construct codes
            else switch(src[index]){
                // Matrix
                case '[': {
                    index++;
                    if(index+7 >= src.size()) return false;
                    PARSE_DIM(rows, 'x');
                    PARSE_DIM(cols, ']');
                    const uint16_t sze = rows*cols;
                    if(sze == 0) return false;
                    depth += sze;
                    if(!isUnicodeChar<OPEN_STRVIEW>(&src[index])) return false;
                    index += codepointSize(OPEN_0);
                    break;
                }

                // Cases
                case '{': {
                    index++;
                    if(index+3 >= src.size()) return false;
                    PARSE_DIM(rows, OPEN_0);
                    if(rows == 0) return false;
                    depth += 2*rows;
                    index--;
                    if(!isUnicodeChar<OPEN_STRVIEW>(&src[index])) return false;
                    index += codepointSize(OPEN_0);
                    break;
                }

                default:
                    // Scan to end of keyword
                    const size_t start = index;
                    for(char ch = src[index]; ch != OPEN_0 && ch != '0'; ch = src[index])
                        if(++index >= src.size()) return false;
                    const size_t sze = index-start+(src[index]=='0');
                    if(sze == 0) return false;
                    const std::string_view keyword(src.data()+start, sze);
                    index += codepointSize(src[index]);
                    switch(encodingHash(keyword)){
                        case encodingHash(SETTINGS_STR):{
                            bool subsequent = false;
                            for(;;){
                                if(index >= src.size()) return false;
                                const char ch = src[index++];
                                if(ch == CLOSE_0) break;
                                else if(subsequent && ch != ',') return false;

                                // Parse setting - no validation of strings
                                for(char ch = src[index]; ch != '='; ch = src[index])
                                    if(++index >= src.size()) return false;
                                for(char ch = src[index]; ch != ',' && ch != CLOSE_0; ch = src[index])
                                    if(++index >= src.size()) return false;

                                subsequent = true;
                            }
                            if(!isUnicodeChar<CLOSE_STRVIEW>(&src[index-1])) return false;
                            index += (codepointSize(CLOSE_0) - 1);
                            break;
                        }
                        FORSCAPE_SERIAL_NULLARY_CASES  // Return false if hashed keyword not matched
                        FORSCAPE_SERIAL_UNARY_CASES  // Return false if hashed keyword not matched, depth += 1 and break
                        FORSCAPE_SERIAL_BINARY_CASES  // Return false if hashed keyword not matched, depth += 2 and break
                        default:
                            return false;
                    }
            }
        }else if(isUnicodeChar<CLOSE_STRVIEW>(&src[index])){
            index += codepointSize(CLOSE_0);
            if(depth == 0) return false;
            depth--;
            // The open in subsequent arguments is not strictly required
            if(index < src.size() && isUnicodeChar<OPEN_STRVIEW>(&src[index]))
                index += codepointSize(OPEN_0);
        }else if(src[index] == '\n' && depth != 0){
            return false;
        }else if(isUnicodeChar<OPEN_STRVIEW>(&src[index])){
            return false;
        }else{
            index++;
        }
    }
    return depth == 0;
}

#undef PARSE_DIM

inline void typesetEscape(std::string& out, const std::string_view in) noexcept {
    for(const char& ch : in){
        // Escape special characters in the typeset encoding
        if(isUnicodeChar<CONSTRUCT_STRVIEW>(&ch) || isUnicodeChar<OPEN_STRVIEW>(&ch) || isUnicodeChar<CLOSE_STRVIEW>(&ch))
            out += CONSTRUCT_STR;
        out += ch;
    }
}

}

#endif // FORSCAPE_SERIAL_H
