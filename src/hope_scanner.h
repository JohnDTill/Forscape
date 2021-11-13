#ifndef HOPE_SCANNER_H
#define HOPE_SCANNER_H

#include "hope_error.h"
#include "hope_tokentype.h"
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
    TokenType scanString() noexcept;
    TokenType forwardSlash() noexcept;
    TokenType comment() noexcept;
    TokenType createToken(TokenType type) noexcept;
    TokenType scanNumber() noexcept;
    TokenType scanIdentifier() noexcept;
    TokenType unrecognizedSymbol();
    TokenType scanConstruct(TokenType type) noexcept;
    TokenType close() noexcept;
    TokenType error(ErrorCode code);
    TokenType newline() noexcept;
    TokenType endOfFile() noexcept;
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
