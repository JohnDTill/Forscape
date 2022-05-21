#ifndef HOPE_UNICODE_H
#define HOPE_UNICODE_H

#include <cassert>
#include <inttypes.h>
#include <string>
#include <unicode_zerowidth.h>

namespace Hope {

inline constexpr size_t codepointSize(uint8_t ch) noexcept{
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

static uint32_t expand(char ch) noexcept{
    return static_cast<uint32_t>(static_cast<uint8_t>(ch));
}

template<typename StringType>
inline uint32_t codepointInt(StringType str, size_t index = 0) noexcept {
    assert(index < str.size());

    uint8_t ch = str[index];

    if(isAscii(ch)){
        return ch;
    }else if(sixthBitUnset(ch)){
        uint32_t bit2 = expand(str[index+1]) << 8;
        return ch | bit2;
    }else if(fifthBitUnset(ch)){
        uint32_t bit2 = expand(str[index+1]) << 8;
        uint32_t bit3 = expand(str[index+2]) << 16;
        return ch | bit2 | bit3;
    }else{
        uint32_t bit2 = expand(str[index+1]) << 8;
        uint32_t bit3 = expand(str[index+2]) << 16;
        uint32_t bit4 = expand(str[index+3]) << 24;
        return ch | bit2 | bit3 | bit4;
    }
}

inline bool isSingleCodepoint(const std::string& str) noexcept {
    if(str.empty()) return false;
    return codepointSize(str[0]) == (str.size() - (str.back() == '\0'));
}

inline size_t numBytesInGrapheme(const std::string& str, size_t index) noexcept {
    size_t start = index;
    do {
        index += codepointSize(str[index]);
    } while(index < str.size() && isZeroWidth(codepointInt(str, index)));

    return index - start;
}

inline size_t graphemeSizeLeft(const std::string& str, size_t index) noexcept {
    size_t end = index;

    do {
        assert(index != 0);
        while(isContinuationCharacter(str[--index]));
    } while( isZeroWidth(codepointInt(str, index)) );

    return end-index;
}

template<typename StringType>
inline size_t countGraphemes(StringType str) noexcept {
    size_t num_graphemes = 0;
    size_t index = 0;
    while(index < str.size()){
        num_graphemes += !isZeroWidth(codepointInt(str, index));
        index += codepointSize(str[index]);
    }

    return num_graphemes;
}

inline std::string fromCode(uint32_t code){
    std::string str;
    str += static_cast<uint8_t>(code);

    if(code & (1 << 7)){
        str += static_cast<uint8_t>(code >> 8);
        if(code & (1 << 6)){
            str += static_cast<uint8_t>(code >> 16);
            if(code & (1 << 5)) str += static_cast<uint8_t>(code >> 24);
        }
    }

    return str;
}

inline constexpr uint32_t constructScannerCode(uint32_t code) noexcept {
    uint32_t val = static_cast<uint32_t>(1 << 7) | code;
    assert(isContinuationCharacter(val));
    //Map constructs to continuation characters since they are free
    //   NOTE: this assumes valid UTF-8

    return val;
}

}

#endif // HOPE_UNICODE_H
