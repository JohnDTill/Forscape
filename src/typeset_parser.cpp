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
    substitution.push_back(CLOSE);
}

template<char ch>
void parseOne(const std::string& src){
    substitution.push_back(OPEN);
    substitution.push_back(ch);
    if(src[index] == syntax_open){
        index++;
        parseArg<false>(src);
    }else{
        substitution.push_back(CLOSE);
    }
}

template<char ch>
void parseTwo(const std::string& src){
    substitution.push_back(OPEN);
    substitution.push_back(ch);
    if(src[index] == syntax_open){
        index++;
        parseArg<false>(src);
        parseArg<true>(src);
        assert(index < src.size());
    }else{
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
    }
}

template<char ch>
void parseReversed(const std::string& src){
    substitution.push_back(OPEN);
    substitution.push_back(ch);
    if(src[index] == syntax_open){
        index++;
        size_t start = substitution.size();
        parseArg<false>(src);
        std::string str = substitution.substr(start);
        substitution.erase(substitution.begin()+start, substitution.end());
        parseArg<true>(src);
        substitution.insert(substitution.end(), str.begin(), str.end());
    }else{
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
    }
}

template<char ch>
void parseScript(const std::string& src){
    //EVENTUALLY: don't need open/close?
    return parseOne<ch>(src);
}

void parseCases(const std::string& src){
    substitution.push_back(OPEN);
    substitution.push_back(CASES);
    if(src[index] == syntax_open){
        const size_t patch = substitution.size();
        size_t nargs = 0;
        substitution.push_back('0');

        do {
            nargs++;
            index++;
            parseArg<false>(src);
            parseArg<true>(src);
            assert(index < src.size());
        } while(src[index] == syntax_open);

        substitution[patch] = nargs;
    }else{
        substitution.push_back(2);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
    }
}

void parseMatrix(const std::string& src){
    substitution.push_back(OPEN);
    substitution.push_back(MATRIX);
    if(src[index] == syntax_open){
        index++;
        const size_t dims = substitution.size();
        substitution.push_back('0');
        substitution.push_back('0');

        size_t cols = 0;
        size_t rows = 1;

        do {
            cols++;
            parseText<true, true>(src);
            substitution.push_back(CLOSE);
            assert(index < src.size());
        } while(src[index-1] == ',');

        while(src[index-1] == ';'){
            rows++;
            size_t cols_here = 0;
            do {
                cols_here++;
                parseText<true, true>(src);
                substitution.push_back(CLOSE);
                assert(index < src.size());
            } while(src[index-1] == ',');
            failed |= (cols_here != cols);
        }

        failed |= (cols > 255 || rows > 255);

        substitution[dims] = rows;
        substitution[dims+1] = cols;
    }else{
        substitution.push_back(2);
        substitution.push_back(2);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
        substitution.push_back(CLOSE);
    }
}

typedef void (*ParseRule)(const std::string&);
FORSCAPE_STATIC_MAP<std::string, ParseRule> rules {
    {"^", parseScript<SUPERSCRIPT>},
    {"_", parseScript<SUBSCRIPT>},
    {"^_", parseTwo<DUALSCRIPT>},
    {"bar", parseOne<ACCENTBAR>},
    {"binom", parseTwo<BINOMIAL>},
    {"breve", parseOne<ACCENTBREVE>},
    {"cases", parseCases},
    {"coprod1", parseOne<BIGCOPROD1>}, //EVENTUALLY: don't expose the user to argument numbers in commands
    {"coprod2", parseReversed<BIGCOPROD2>},
    {"dot", parseOne<ACCENTDOT>},
    {"ddot", parseOne<ACCENTDDOT>},
    {"dddot", parseOne<ACCENTDDDOT>},
    {"frac", parseTwo<FRACTION>},
    {"hat", parseOne<ACCENTHAT>},
    {"iint1", parseOne<DOUBLEINTEGRAL1>},
    {"iiint1", parseOne<TRIPLEINTEGRAL1>},
    {"inf", parseOne<INF>},
    {"int1", parseOne<INTEGRAL1>},
    {"int2", parseReversed<INTEGRAL2>},
    {"intersection1", parseOne<BIGINTERSECTION1>},
    {"intersection2", parseReversed<BIGINTERSECTION2>},
    {"lim", parseTwo<LIMIT>},
    {"mat", parseMatrix},
    {"max", parseOne<MAX>},
    {"min", parseOne<MIN>},
    {"nrt", parseTwo<NRT>},
    {"oint1", parseOne<INTEGRALCONV1>},
    {"oiint1", parseOne<DOUBLEINTEGRALCONV1>},
    {"oint2", parseReversed<INTEGRALCONV2>},
    {"prod1", parseOne<BIGPROD1>},
    {"prod2", parseReversed<BIGPROD2>},
    {"sqrt", parseOne<SQRT>},
    {"sum1", parseOne<BIGSUM1>},
    {"sum2", parseReversed<BIGSUM2>},
    {"sup", parseOne<SUP>},
    {"tilde", parseOne<ACCENTTILDE>},
    {"union1", parseOne<BIGUNION1>},
    {"union2", parseReversed<BIGUNION2>},
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
