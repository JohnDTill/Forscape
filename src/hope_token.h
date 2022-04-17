#ifndef HOPE_TOKEN_H
#define HOPE_TOKEN_H

#include <code_tokentype.h>
#include "typeset_selection.h"

namespace Hope {

namespace Code {

struct Token {
    Typeset::Selection sel;
    TokenType type;

    Token(const Typeset::Selection& sel, TokenType type) noexcept
        : sel(sel), type(type) {}
};

}

}

#endif // HOPE_TOKEN_H
