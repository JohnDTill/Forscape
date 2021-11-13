#ifndef HOPE_UNICODE_H
#define HOPE_UNICODE_H

#include <cassert>
#include <inttypes.h>

namespace Hope {

inline constexpr size_t glyphSize(uint8_t ch) noexcept{
    if(ch >> 7 == 0) return 1;
    assert((ch & (1 << 6)) != 0);
    if((ch & (1 << 5)) == 0) return 2;
    if((ch & (1 << 4)) == 0) return 3;
    return 4;
}

inline constexpr bool isContinuationCharacter(char ch) noexcept {
    return static_cast<uint8_t>(ch) >> 6 == 2;
}

inline constexpr bool isContinuationCharacter(uint32_t ch) noexcept {
    return ch >> 6 == 2;
}

inline constexpr bool isAscii(char ch) noexcept{
    return ch >> 7 == 0;
}

inline constexpr bool sixthBitSet(char ch) noexcept{
    return !(ch & (1 << 5));
}

inline constexpr bool sixthBitUnset(char ch) noexcept{
    return !(ch & (1 << 5));
}

inline constexpr bool fifthBitSet(char ch) noexcept{
    return !(ch & (1 << 4));
}

inline constexpr bool fifthBitUnset(char ch) noexcept{
    return !(ch & (1 << 4));
}

inline constexpr bool isNumeric(char ch) noexcept{
    return (ch >= '0') & (ch <= '9');
}

inline constexpr bool isAlpha(char ch) noexcept{
    return ((ch >= 'a') & (ch <= 'z')) |
           ((ch >= 'A') & (ch <= 'Z')) |
            (ch == '_');
}

inline constexpr bool isAlphaNumeric(char ch) noexcept{
    return ((ch >= 'a') & (ch <= 'z')) |
           ((ch >= 'A') & (ch <= 'Z')) |
           ((ch >= '0') & (ch <= '9')) |
            (ch == '_');
}

inline constexpr uint32_t constructScannerCode(uint32_t code) noexcept{
    uint32_t val = static_cast<uint32_t>(1 << 7) | code;
    assert(isContinuationCharacter(val));
    //Map constructs to continuation characters since they are free
    //   NOTE: this assumes valid UTF-8

    return val;
}

}

#endif // HOPE_UNICODE_H
