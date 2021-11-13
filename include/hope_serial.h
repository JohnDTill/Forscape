#ifndef HOPE_SERIAL_H
#define HOPE_SERIAL_H

#include "hope_construct_codes.h"
#include <cassert>
#include <inttypes.h>
#include <limits>
#include <string>

namespace Hope {

inline bool isValidSerial(const std::string& src) noexcept{
    uint32_t depth = 0;
    size_t index = 0;
    while(index < src.size()){
        auto ch = src[index++];
        if(ch == OPEN){
            if(index >= src.size()) return false;
            switch (src[index++]) {
                HOPE_SERIAL_NULLARY_CASES
                    break;
                HOPE_SERIAL_UNARY_CASES
                    depth += 1;
                    break;
                HOPE_SERIAL_BINARY_CASES
                    depth += 2;
                    break;
                case CASES: {
                    if(index >= src.size()) return false;
                    uint8_t sze = static_cast<uint8_t>(src[index++]);
                    if(sze == 0 || sze > std::numeric_limits<uint8_t>::max()/2)
                        return false;
                    depth += 2*sze;
                    break;
                }
                HOPE_SERIAL_MATRIX_CASES {
                    if(index+1 >= src.size()) return false;
                    uint16_t rows = static_cast<uint16_t>(src[index++]);
                    uint16_t cols = static_cast<uint16_t>(src[index++]);
                    uint16_t sze = rows*cols;
                    if(sze == 0) return false;
                    depth += sze;
                    break;
                }
                default:
                    return false;
            }
            if(depth > (std::numeric_limits<uint32_t>::max() >> 1))
                return false;
        }else if(ch == CLOSE){
            if(depth == 0) return false;
            depth--;
        }
    }
    return depth == 0;
}

}

#endif // HOPE_SERIAL_H
