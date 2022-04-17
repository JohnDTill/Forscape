#include <hope_scanner.h>
#include "report.h"
#include "typeset.h"

using namespace Hope;
using namespace Code;

static bool sameTypes(const std::vector<Token>& tokens, const std::vector<TokenType>& expected){
    if(tokens.size() != expected.size()) return false;
    for(size_t i = 0; i < tokens.size(); i++){
        if(tokens[i].type != expected[i])
            return false;
    }
    return true;
}

static std::string toString(const std::vector<TokenType>& types){
    std::string str = "{";
    str += std::to_string(types.front());
    for(size_t i = 1; i < types.size(); i++)
        str += ", " + std::to_string(types[i]);
    str += "}";

    return str;
}

static std::string toString(const std::vector<Token>& tokens){
    std::string str = "{";
    str += std::to_string(tokens.front().type);
    for(size_t i = 1; i < tokens.size(); i++)
        str += ", " + std::to_string(tokens[i].type);
    str += "}";

    return str;
}

inline bool testScanner(){
    bool passing = true;

    std::string input = "print(\"Hello world!\") //This is a test";
    std::vector<TokenType> expected = {PRINT, LEFTPAREN, STRING, RIGHTPAREN, COMMENT, ENDOFFILE};
    Typeset::Model* model = Typeset::Model::fromSerial(input);
    Code::Scanner* scanner = new Scanner(model);
    scanner->scanAll();

    if(!sameTypes(scanner->tokens, expected)){
        std::cout << "Line " << __LINE__ << ", Scanner does not get expected result:\n";
        std::cout << "Expected: " << toString(expected) << '\n'
                  << "Actual:   " << toString(scanner->tokens) << std::endl;
        passing = false;
    }

    delete model;
    delete scanner;

    input = "x + y = 2.5\n"
            "3*x + 2*y = 6\n";
    expected = {IDENTIFIER, PLUS, IDENTIFIER, EQUALS, INTEGER, PERIOD, INTEGER, NEWLINE,
        INTEGER, MULTIPLY, IDENTIFIER, PLUS, INTEGER, MULTIPLY, IDENTIFIER, EQUALS, INTEGER, NEWLINE, ENDOFFILE};
    model = Typeset::Model::fromSerial(input);
    scanner = new Scanner(model);
    scanner->scanAll();

    if(!sameTypes(scanner->tokens, expected)){
        std::cout << "Line " << __LINE__ << ", Scanner does not get expected result:\n";
        std::cout << "Expected: " << toString(expected) << '\n'
                  << "Actual:   " << toString(scanner->tokens) << std::endl;
        passing = false;
    }

    delete model;
    delete scanner;

    input = "x2";
    expected = {IDENTIFIER, TOKEN_SUPERSCRIPT, INTEGER, ARGCLOSE, ENDOFFILE};
    model = Typeset::Model::fromSerial(input);
    scanner = new Scanner(model);
    scanner->scanAll();

    if(!sameTypes(scanner->tokens, expected)){
        std::cout << "Line " << __LINE__ << ", Scanner does not get expected result:\n";
        std::cout << "Expected: " << toString(expected) << '\n'
                  << "Actual:   " << toString(scanner->tokens) << std::endl;
        passing = false;
    }

    delete model;
    delete scanner;

    input = "x ∈ ℝ";
    expected = {IDENTIFIER, MEMBER, DOUBLESTRUCK_R, ENDOFFILE};
    model = Typeset::Model::fromSerial(input);
    scanner = new Scanner(model);
    scanner->scanAll();

    if(!sameTypes(scanner->tokens, expected)){
        std::cout << "Line " << __LINE__ << ", Scanner does not get expected result:\n";
        std::cout << "Expected: " << toString(expected) << '\n'
                  << "Actual:   " << toString(scanner->tokens) << std::endl;
        passing = false;
    }

    delete model;
    delete scanner;

    input = "if is_active { return false; }";
    expected = {IF, IDENTIFIER, LEFTBRACKET, RETURN, FALSELITERAL, SEMICOLON, RIGHTBRACKET, ENDOFFILE};
    model = Typeset::Model::fromSerial(input);
    scanner = new Scanner(model);
    scanner->scanAll();

    if(!sameTypes(scanner->tokens, expected)){
        std::cout << "Line " << __LINE__ << ", Scanner does not get expected result:\n";
        std::cout << "Expected: " << toString(expected) << '\n'
                  << "Actual:   " << toString(scanner->tokens) << std::endl;
        passing = false;
    }

    delete model;
    delete scanner;
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Code scanner", passing);
    return passing;
}
