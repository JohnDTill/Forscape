#ifndef FORSCAPE_TOKEN_H
#define FORSCAPE_TOKEN_H

#include <code_tokentype.h>
#include "typeset_selection.h"

namespace Forscape {

namespace Code {

struct Token {
    Typeset::Selection sel;
    ForscapeTokenType type;

    Token(const Typeset::Selection& sel, ForscapeTokenType type) noexcept
        : sel(sel), type(type) {}
};

}

}

#endif // FORSCAPE_TOKEN_H
