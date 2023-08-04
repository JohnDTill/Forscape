#ifndef FORSCAPE_UNICODE_H
#define FORSCAPE_UNICODE_H

#include <cassert>
#include <inttypes.h>
#include <string>
#include <unicode_zerowidth.h>

namespace Forscape {

inline constexpr size_t codepointSize(uint8_t ch) noexcept {
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

inline constexpr bool isAscii(char ch) noexcept {
    return ch >> 7 == 0;
}

inline constexpr bool seventhBitSet(char ch) noexcept {
    return (ch & (1 << 6));
}

inline constexpr bool sixthBitSet(char ch) noexcept {
    return (ch & (1 << 5));
}

inline constexpr bool sixthBitUnset(char ch) noexcept {
    return !(ch & (1 << 5));
}

inline constexpr bool fifthBitSet(char ch) noexcept {
    return (ch & (1 << 4));
}

inline constexpr bool fifthBitUnset(char ch) noexcept {
    return !(ch & (1 << 4));
}

inline constexpr bool isNumeric(char ch) noexcept {
    return (ch >= '0') & (ch <= '9');
}

inline constexpr bool isAlpha(char ch) noexcept {
    return ((ch >= 'a') & (ch <= 'z')) |
           ((ch >= 'A') & (ch <= 'Z')) |
            (ch == '_');
}

inline constexpr bool isAlphaNumeric(char ch) noexcept {
    return ((ch >= 'a') & (ch <= 'z')) |
           ((ch >= 'A') & (ch <= 'Z')) |
           ((ch >= '0') & (ch <= '9')) |
            (ch == '_');
}

inline constexpr bool isPathChar(char ch) noexcept {
    return ch != ' ';
}

static uint32_t expand(char ch) noexcept {
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
    } while(index < str.size() && !isAscii(str[index]) && isZeroWidth(codepointInt(str, index)));

    return index - start;
}

inline size_t graphemeSizeLeft(const std::string& str, size_t index) noexcept {
    size_t end = index;

    do {
        assert(index != 0);
        while(isContinuationCharacter(str[--index]));
    } while( !isAscii(str[index]) && isZeroWidth(codepointInt(str, index)) );

    return end-index;
}

inline std::string fromCodepointBytes(uint32_t bytes){
    std::string str;
    str += static_cast<uint8_t>(bytes);

    if(bytes & (1 << 7)){
        str += static_cast<uint8_t>(bytes >> 8);
        if(bytes & (1 << 6)){
            str += static_cast<uint8_t>(bytes >> 16);
            if(bytes & (1 << 5)) str += static_cast<uint8_t>(bytes >> 24);
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

template<uint8_t N>
inline constexpr uint32_t firstBits(uint8_t ch) noexcept {
    return ch & ((1 << N) - 1);
}

//Found perfect hash of unicode numbers, but using full uint32_t bytes was better
/*
template<typename StringType>
inline uint32_t codepoint(StringType str, size_t index) noexcept {
    assert(index < str.size());

    uint32_t first = static_cast<uint8_t>(str[index]);
    if(isAscii(first)) return first;
    assert(seventhBitSet(first));
    if(sixthBitUnset(first)){
        uint32_t b2 = firstBits<6>(str[index+1]);
        return (firstBits<5>(first) << 6) | b2;
    }else if(fifthBitSet(first)){
        uint32_t b2 = firstBits<6>(str[index+1]);
        uint32_t b3 = firstBits<6>(str[index+2]);
        uint32_t b4 = firstBits<6>(str[index+3]);
        return (firstBits<3>(first) << 18) | (b2 << 12) | (b3 << 6) | b4;
    }else{
        uint32_t b2 = firstBits<6>(str[index+1]);
        uint32_t b3 = firstBits<6>(str[index+2]);
        return (firstBits<4>(first) << 12) | (b2 << 6) | b3;
    }
}
*/

template<typename StringType>
inline size_t countGraphemes(StringType str) noexcept {
    size_t num_graphemes = 0;
    size_t index = 0;
    while(index < str.size()){
        if(isAscii(str[index])){
            index++;
            num_graphemes++;
        }else{
            num_graphemes += !isZeroWidth(codepointInt(str, index));
            index += codepointSize(str[index]);
        }
    }

    return num_graphemes;
}

template<typename StringType>
inline size_t charIndexOfGrapheme(StringType str, size_t grapheme_index, size_t index = 0) noexcept {
    size_t num_graphemes = 0;
    while(index < str.size() && num_graphemes < grapheme_index){
        if(isAscii(str[index])){
            index++;
            num_graphemes++;
        }else{
            num_graphemes += !isZeroWidth(codepointInt(str, index));
            index += codepointSize(str[index]);
        }
    }

    return index;
}

//Note: This should only be actively checked where external inputs are supplied.
//      Internally, strings are known to be valid UTF-8, which may be verified by assertions.
inline bool isIllFormedUtf8(const std::string& str) noexcept {
    size_t index = 0;
    while(index < str.size()){
        const char ch = str[index++];
        if(!isAscii(ch)){
            if(!seventhBitSet(ch)) return true;

            if(sixthBitSet(ch)){
                if(fifthBitSet(ch)){
                    if(index >= str.size()-2
                       || !isContinuationCharacter(str[index++])
                       || !isContinuationCharacter(str[index++])
                       || !isContinuationCharacter(str[index++])) return true;
                }else{
                    if(index >= str.size()-1
                       || !isContinuationCharacter(str[index++])
                       || !isContinuationCharacter(str[index++])) return true;
                }
            }else{
                if(index >= str.size() || !isContinuationCharacter(str[index++])) return true;
            }
        }
    }

    return str.size() > 0 && isZeroWidth(codepointInt(str, 0)); //EVENTUALLY: should accommodate leading zero-width char?
}

}

#endif // FORSCAPE_UNICODE_H
