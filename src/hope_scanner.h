#ifndef HOPE_SCANNER_H
#define HOPE_SCANNER_H

#include "hope_error.h"
#include <code_tokentype.h>
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
    TokenType scanToken();

    std::vector<TokenType> tokens;
    std::vector<std::pair<Typeset::Marker, Typeset::Marker> > markers;

private:
    TokenType scanString();
    TokenType forwardSlash();
    TokenType comment();
    TokenType createToken(TokenType type);
    TokenType scanNumber();
    TokenType scanIdentifier();
    TokenType unrecognizedSymbol();
    TokenType scanConstruct(TokenType type);
    TokenType close();
    TokenType error(ErrorCode code);
    TokenType newline();
    TokenType endOfFile();
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
