#ifndef FORSCAPE_SERIAL_UNICODE_H
#define FORSCAPE_SERIAL_UNICODE_H

#include "construct_codes.h"
#include "forscape_serial.h"
#include "forscape_unicode.h"
#include "unicode_subscripts.h"
#include "unicode_superscripts.h"
#include <cassert>
#include <inttypes.h>
#include <limits>
#include <string>
#include <string_view>

namespace Forscape {

class UnicodeConverter{
private:
    const std::string& src;
    size_t index = 0;
    std::string out;

    UnicodeConverter(const std::string& src)
        : src(src){
        assert(isValidSerial(src));
    }

    uint32_t advance(size_t& index) const noexcept{
        return static_cast<uint8_t>(src[index++]);
    }

    uint32_t getCode(size_t& index) const noexcept{
        uint32_t ch = advance(index);
        if(isAscii(ch)){
            return ch;
        }else if(sixthBitUnset(ch)){
            return ch | (advance(index) << 8);
        }else if(fifthBitUnset(ch)){
            uint32_t bit2 = advance(index) << 8;
            uint32_t bit3 = advance(index) << 16;
            return ch | bit2 | bit3;
        }else{
            uint32_t bit2 = advance(index) << 8;
            uint32_t bit3 = advance(index) << 16;
            uint32_t bit4 = advance(index) << 24;
            return ch | bit2 | bit3 | bit4;
        }
    }

    bool superscriptFails(size_t& index) const noexcept{
        for(;;) switch (getCode(index)) {
            FORSCAPE_SUPERSCRIPTS_CASES
            case ' ':
                break;

            case CLOSE:
                return false;

            default:
                return true;
        }
    }

    bool subscriptFails(size_t& index) const noexcept{
        for(;;) switch (getCode(index)) {
            FORSCAPE_SUBSCRIPTS_CASES
            case ' ':
                break;

            case CLOSE:
                return false;

            default:
                return true;
        }
    }

    bool twoArgFails(size_t& index) const noexcept{
        char ch = src[index++];
        while(ch != CLOSE)
            if(ch == OPEN) return true;
            else ch = src[index++];
        ch = src[index++];
        while(ch != CLOSE)
            if(ch == OPEN) return true;
            else ch = src[index++];
        return false;
    }

    bool oneArgFails(size_t& index) const noexcept{
        char ch = src[index++];
        while(ch != CLOSE)
            if(ch == OPEN) return true;
            else ch = src[index++];
        return false;
    }

    bool matrixFails(size_t& index) const noexcept{
        uint8_t rows = src[index++];
        uint8_t cols = src[index++];
        uint16_t elems = static_cast<uint16_t>(rows)*cols;
        while(elems-->0) if(oneArgFails(index)) return true;
        return false;
    }

    bool accentFails(size_t& index) const noexcept{
        uint8_t ch = src[index];
        if(ch == OPEN) return true;
        else if(ch == CLOSE) return true;
        index += codepointSize(ch);

        return src[index++] != CLOSE;
    }

    bool canConvert() const noexcept{
        size_t index = 0;
        while(index < src.size()){
            if(src[index++] == OPEN){
                switch (src[index++]) {
                    case SUPERSCRIPT:
                        if(superscriptFails(index)) return false;
                        break;

                    case INTEGRAL1:
                    case DOUBLEINTEGRAL1:
                    case TRIPLEINTEGRAL1:
                    case INTEGRALCONV1:
                    case DOUBLEINTEGRALCONV1:
                    case TRIPLEINTEGRALCONV1:
                    case BIGSUM1:
                    case BIGPROD1:
                    case BIGCOPROD1:
                    case BIGUNION1:
                    case BIGINTERSECTION1:
                    case SUBSCRIPT:
                        if(subscriptFails(index)) return false;
                        break;

                    case SQRT:
                        if(oneArgFails(index)) return false;
                        break;

                    case NRT:
                        if(superscriptFails(index) || oneArgFails(index)) return false;
                        break;

                    case BINOMIAL:
                    case FRACTION:
                        if(twoArgFails(index)) return false;
                        break;

                    case MATRIX:
                        if(matrixFails(index)) return false;
                        break;

                    case ACCENTHAT:
                    case ACCENTTILDE:
                    case ACCENTBAR:
                    case ACCENTBREVE:
                    case ACCENTDOT:
                    case ACCENTDDOT:
                    case ACCENTDDDOT:
                        if(accentFails(index)) return false;
                        break;

                    case BIGSUM0:
                    case BIGPROD0:
                    case BIGCOPROD0:
                    case BIGUNION0:
                    case BIGINTERSECTION0:
                    case INTEGRAL0:
                    case DOUBLEINTEGRAL0:
                    case TRIPLEINTEGRAL0:
                    case INTEGRALCONV0:
                    case DOUBLEINTEGRALCONV0:
                    case TRIPLEINTEGRALCONV0:
                    case MARKERLINK:
                        break;

                    default: return false;
                }
            }
        }

        return true;
    }

    void writeSuperscript(){
        for(;;) switch (getCode(index)) {
            FORSCAPE_SUPERSCRIPTS_CONVERSION
            case ' ': out += ' '; break;
            case CLOSE: return;
            default: assert(false);
        }
    }

    void writeSubscript(){
        for(;;) switch (getCode(index)) {
            FORSCAPE_SUBSCRIPTS_CONVERSION
            case ' ': out += ' '; break;
            case CLOSE: return;
            default: assert(false);
        }
    }

    template<uint8_t byte0, uint8_t byte1>
    void writeAccent(){
        char ch = src[index++];
        out += ch;
        for(size_t i = 1; i < codepointSize(ch); i++)
            out += src[index++];
        out += byte0;
        out += byte1;
        assert(src[index] == CLOSE);
        index++;
    }

    template<uint8_t byte0, uint8_t byte1, uint8_t byte2>
    void writeAccent(){
        writeAccent<byte0, byte1>();
        out += byte2;
    }

    void writeBinomial(){
        out += '(';
        char ch;
        while((ch = src[index++]) != CLOSE) out += ch;
        out += " ¦ ";
        while((ch = src[index++]) != CLOSE) out += ch;
        out += ')';
    }

    void writeFraction(){
        //EVENTUALLY: better conversion with precedence to avoid stupid parenthesis
        //            e.g. frac{1}{2} should not convert to (1)/(2)
        out += '(';
        char ch;
        while((ch = src[index++]) != CLOSE) out += ch;
        out += ")/(";
        while((ch = src[index++]) != CLOSE) out += ch;
        out += ')';
    }

    void writeSqrt(){
        out += "√(";
        char ch;
        while((ch = src[index++]) != CLOSE) out += ch;
        out += ')';

        //∛ and ∜ for n roots
    }

    void writeNrt(){
        char curr = src[index];
        if(curr == '3' && src[index+1] == CLOSE){
            out += "∛(";
        }else if(curr == '4' && src[index+1] == CLOSE){
            out += "∜(";
        }else{
            writeSuperscript();
            writeSqrt();
            return;
        }

        index += 2;
        char ch;
        while((ch = src[index++]) != CLOSE) out += ch;
        out += ')';
    }

    void writeRow(uint8_t cols){
        char ch;
        while((ch = src[index++]) != CLOSE) out += ch;
        for(uint8_t j = 1; j < cols; j++){
            out += ", ";
            char ch;
            while((ch = src[index++]) != CLOSE) out += ch;
        }
    }

    void writeMatrix(){
        uint8_t rows = src[index++];
        uint8_t cols = src[index++];

        out += '[';
        writeRow(cols);
        for(uint8_t i = 1; i < rows; i++){
            out += "; ";
            writeRow(cols);
        }
        out += ']';
    }

    std::string convert(){
        assert(canConvert());
        out.reserve(src.size());

        while(index < src.size()){
            char ch = src[index++];
            if(ch == OPEN){
                switch (src[index++]) {
                    case SUPERSCRIPT:
                        writeSuperscript(); break;

                    case SUBSCRIPT:
                        writeSubscript(); break;

                    case BINOMIAL:
                        writeBinomial(); break;

                    case FRACTION:
                        writeFraction(); break;

                    case SQRT:
                        writeSqrt(); break;

                    case NRT:
                        writeNrt(); break;

                    case MATRIX:
                        writeMatrix(); break;

                    case ACCENTHAT: writeAccent<0xCC, 0x82>(); break;
                    case ACCENTTILDE: writeAccent<0xCC, 0x83>(); break;
                    case ACCENTBAR: writeAccent<0xCC, 0x85>(); break;
                    case ACCENTBREVE: writeAccent<0xCC, 0x86>(); break;
                    case ACCENTDOT: writeAccent<0xCC, 0x87>(); break;
                    case ACCENTDDOT: writeAccent<0xCC, 0x88>(); break;
                    case ACCENTDDDOT: writeAccent<0xE2, 0x83, 0x9B>(); break;
                    //Four dots: 0xE2 0x83 0x9C
                    //Combining right arrow: 0xE2 0x83 0x97

                    case BIGSUM0: out += "∑"; break;
                    case BIGPROD0: out += "∏"; break;
                    case BIGCOPROD0: out += "∐"; break;
                    case BIGUNION0: out += "⋃"; break;
                    case BIGINTERSECTION0: out += "⋂"; break;
                    //Nary logical ⋀ ⋁ also defined
                    case INTEGRAL0: out += "∫"; break;
                    case DOUBLEINTEGRAL0: out += "∬"; break;
                    case TRIPLEINTEGRAL0: out += "∭"; break;
                    case INTEGRALCONV0: out += "∮"; break;
                    case DOUBLEINTEGRALCONV0: out += "∯"; break;
                    case TRIPLEINTEGRALCONV0: out += "∰"; break;

                    case BIGSUM1: out += "∑"; writeSubscript(); break;
                    case BIGPROD1: out += "∏"; writeSubscript(); break;
                    case BIGCOPROD1: out += "∐"; writeSubscript(); break;
                    case BIGUNION1: out += "⋃"; writeSubscript(); break;
                    case BIGINTERSECTION1: out += "⋂"; writeSubscript(); break;
                    case INTEGRAL1: out += "∫"; writeSubscript(); break;
                    case DOUBLEINTEGRAL1: out += "∬"; writeSubscript(); break;
                    case TRIPLEINTEGRAL1: out += "∭"; writeSubscript(); break;
                    case INTEGRALCONV1: out += "∮"; writeSubscript(); break;
                    case DOUBLEINTEGRALCONV1: out += "∯"; writeSubscript(); break;
                    case TRIPLEINTEGRALCONV1: out += "∰"; writeSubscript(); break;

                    case MARKERLINK:
                        out += "Line ";
                        out += std::to_string(static_cast<uint8_t>(src[index++]));
                        out += ':';
                        index++;
                        break;
                }
            }else{
                out += ch;
            }
        }

        return out;
    }

public:
    static bool canConvert(const std::string& str) noexcept{
        UnicodeConverter converter(str);
        return converter.canConvert();
    }

    static std::string convert(const std::string& str){
        UnicodeConverter converter(str);
        return converter.convert();
    }
};

}

#endif // FORSCAPE_SERIAL_UNICODE_H
