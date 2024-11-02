#include "typeset_parser.h"

#include "forscape_serial.h"
#include "typeset_keywords.h"
#include "typeset_syntax.h"

//EVENTUALLY: write a test for the keyword parser

namespace Forscape {

namespace Typeset {

static std::string substitution;
static size_t index;
static bool failed;
static bool no_command_parsed;

static bool isCommandEnd(char ch) noexcept {
    return ch == syntax_cmd || ch == syntax_open || ch == syntax_close || ch == ' ';
}

template<bool ret_on_close = true, bool matrix = false>
void parseText(const std::string& src);

template<bool with_open>
void parseArg(const std::string& src){
    if(with_open) failed |= (src[index++] != syntax_open);
    parseText(src);
    substitution += CLOSE_STR;
}

template<const char* encoding_keyword>
void parseOne(const std::string& src){
    substitution += CONSTRUCT_STR;
    substitution += std::string_view(encoding_keyword);
    substitution += OPEN_STR;
    if(src[index] == syntax_open){
        index++;
        parseArg<false>(src);
    }else{
        substitution += CLOSE_STR;
    }
}

template<const char* encoding_keyword>
void parseTwo(const std::string& src){
    substitution += CONSTRUCT_STR;
    substitution += std::string_view(encoding_keyword);
    substitution += OPEN_STR;
    if(src[index] == syntax_open){
        index++;
        parseArg<false>(src);
        parseArg<true>(src);
        assert(index < src.size());
    }else{
        substitution += CLOSE_STR CLOSE_STR;
    }
}

template<const char* encoding_keyword>
void parseReversed(const std::string& src){
    substitution += CONSTRUCT_STR;
    substitution += std::string_view(encoding_keyword);
    substitution += OPEN_STR;
    if(src[index] == syntax_open){
        index++;
        size_t start = substitution.size();
        parseArg<false>(src);
        std::string str = substitution.substr(start);
        substitution.erase(substitution.begin()+start, substitution.end());
        parseArg<true>(src);
        substitution.insert(substitution.end(), str.begin(), str.end());
    }else{
        substitution += CLOSE_STR CLOSE_STR;
    }
}

template<const char* encode_str>
void parseScript(const std::string& src){
    //EVENTUALLY: don't need open/close?
    parseOne<encode_str>(src);
}

void parseCases(const std::string& src){
    substitution += CONSTRUCT_STR "{";
    if(src[index] == syntax_open){
        const size_t patch = substitution.size();
        substitution += OPEN_STR;
        size_t nargs = 0;

        do {
            nargs++;
            index++;
            parseArg<false>(src);
            parseArg<true>(src);
            assert(index < src.size());
        } while(src[index] == syntax_open);

        substitution.insert(patch, std::to_string(nargs));
    }else{
        substitution += "2" OPEN_STR CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR;
    }
}

void parseMatrix(const std::string& src){
    substitution += CONSTRUCT_STR "[";
    if(src[index] == syntax_open){
        index++;
        const size_t dim_index = substitution.size();
        substitution += OPEN_STR;

        size_t cols = 0;
        size_t rows = 1;

        do {
            cols++;
            parseText<true, true>(src);
            substitution += CLOSE_STR;
            assert(index < src.size());
        } while(src[index-1] == ',');

        while(src[index-1] == ';'){
            rows++;
            size_t cols_here = 0;
            do {
                cols_here++;
                parseText<true, true>(src);
                substitution += CLOSE_STR;
                assert(index < src.size());
            } while(src[index-1] == ',');
            failed |= (cols_here != cols);
        }

        failed |= (cols > 255 || rows > 255);

        substitution.insert(dim_index, std::to_string(rows) + 'x' + std::to_string(cols) + ']');
    }else{
        substitution += "2x2]" OPEN_STR CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR;
    }
}

DECLARE_CONSTRUCT_CSTRINGS

typedef void (*ParseRule)(const std::string&);
FORSCAPE_STATIC_MAP<std::string, ParseRule> rules {
    {"^", parseScript<SUPERSCRIPT_CSTRING>},
    {"_", parseScript<SUBSCRIPT_CSTRING>},
    {"^_", parseTwo<DUALSCRIPT_CSTRING>},
    {"bar", parseOne<ACCENTBAR_CSTRING>},
    {"binom", parseTwo<BINOMIAL_CSTRING>},
    {"breve", parseOne<ACCENTBREVE_CSTRING>},
    {"cases", parseCases},
    {"coprod1", parseOne<BIGCOPROD1_CSTRING>}, //EVENTUALLY: don't expose the user to argument numbers in commands
    {"coprod2", parseReversed<BIGCOPROD2_CSTRING>},
    {"dot", parseOne<ACCENTDOT_CSTRING>},
    {"ddot", parseOne<ACCENTDDOT_CSTRING>},
    {"dddot", parseOne<ACCENTDDDOT_CSTRING>},
    {"frac", parseTwo<FRACTION_CSTRING>},
    {"hat", parseOne<ACCENTHAT_CSTRING>},
    {"iint1", parseOne<DOUBLEINTEGRAL1_CSTRING>},
    {"iiint1", parseOne<TRIPLEINTEGRAL1_CSTRING>},
    {"inf", parseOne<INF_CSTRING>},
    {"int1", parseOne<INTEGRAL1_CSTRING>},
    {"int2", parseReversed<INTEGRAL2_CSTRING>},
    {"intersection1", parseOne<BIGINTERSECTION1_CSTRING>},
    {"intersection2", parseReversed<BIGINTERSECTION2_CSTRING>},
    {"lim", parseTwo<LIMIT_CSTRING>},
    {"mat", parseMatrix},
    {"max", parseOne<MAX_CSTRING>},
    {"min", parseOne<MIN_CSTRING>},
    {"nrt", parseTwo<NRT_CSTRING>},
    {"oint1", parseOne<INTEGRALCONV1_CSTRING>},
    {"oiint1", parseOne<DOUBLEINTEGRALCONV1_CSTRING>},
    {"oint2", parseReversed<INTEGRALCONV2_CSTRING>},
    {"prod1", parseOne<BIGPROD1_CSTRING>},
    {"prod2", parseReversed<BIGPROD2_CSTRING>},
    {"sqrt", parseOne<SQRT_CSTRING>},
    {"sum1", parseOne<BIGSUM1_CSTRING>},
    {"sum2", parseReversed<BIGSUM2_CSTRING>},
    {"sup", parseOne<SUP_CSTRING>},
    {"tilde", parseOne<ACCENTTILDE_CSTRING>},
    {"union1", parseOne<BIGUNION1_CSTRING>},
    {"union2", parseReversed<BIGUNION2_CSTRING>},
};

void parseCommand(const std::string& src){
    size_t start = index;
    while (index < src.size()-1 && !isCommandEnd(src[index])) index++;
    std::string_view cmd(src.data()+start, index-start);

    auto lookup = rules.find(cmd);
    if(lookup != rules.end()){
        lookup->second(src);
    }else{
        //EVENTUALLY: this could use a string_view (which is challenging because the map is editable)
        const std::string& result = Keywords::lookup(std::string(cmd));
        failed |= result.empty();
        substitution.insert(substitution.end(), result.cbegin(), result.cend());
    }
}

template<bool ret_on_close, bool matrix>
void parseText(const std::string& src){
    while(index < src.size()-1){
        char ch = src[index++];
        switch (ch) {
            case syntax_cmd: parseCommand(src); no_command_parsed = false; break;
            case syntax_close: if(ret_on_close) return;
            case ',': if(matrix) return;
            case ';': if(matrix) return;
            default: substitution.push_back(ch);
        }
    }

    if(ret_on_close) failed = true;
}

const std::string& getSubstitution(const std::string& src) {
    substitution.clear();
    failed = false;
    no_command_parsed = true;

    index = 0;
    parseText<false>(src);

    if(failed || no_command_parsed) substitution.clear();
    assert(isValidSerial(substitution));

    return substitution;
}

}

}
