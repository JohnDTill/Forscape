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
    Scanner(Typeset::Model* model) noexcept;
    void scanAll() alloc_except;
    void scanToken() alloc_except;

    std::vector<Token> tokens;

private:
    void scanString() alloc_except;
    void forwardSlash() alloc_except;
    void comment() alloc_except;
    void createToken(TokenType type) alloc_except;
    void scanNumber() alloc_except;
    void scanIdentifier() alloc_except;
    void unrecognizedSymbol() alloc_except;
    void scanConstruct(TokenType type) alloc_except;
    void close() alloc_except;
    void error(ErrorCode code) alloc_except;
    void newline() alloc_except;
    void endOfFile() alloc_except;
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
