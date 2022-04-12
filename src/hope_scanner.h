#ifndef HOPE_SCANNER_H
#define HOPE_SCANNER_H

#include "hope_error.h"
#include "hope_token.h"
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Typeset {
    class Controller;
}

namespace Code {

class Scanner {
public:
    Scanner(Typeset::Model* model);
    void scanAll();
    void scanToken();

    std::vector<Token> tokens;

private:
    void scanString();
    void forwardSlash();
    void comment();
    void createToken(TokenType type);
    void scanNumber();
    void scanIdentifier();
    void unrecognizedSymbol();
    void scanConstruct(TokenType type);
    void close();
    void error(ErrorCode code);
    void newline();
    void endOfFile();
    void incrementScope() noexcept;
    void decrementScope() noexcept;

    Typeset::Model* model;
    Typeset::Controller* controller;
    std::vector<Error>& errors;
    static const std::unordered_map<std::string_view, TokenType> keywords;
    size_t scope_depth = 0;
};

}

}

#endif // HOPE_SCANNER_H
