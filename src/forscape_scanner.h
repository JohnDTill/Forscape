#ifndef FORSCAPE_SCANNER_H
#define FORSCAPE_SCANNER_H

#include <forscape_common.h>
#include "forscape_error.h"
#include "forscape_token.h"
#include <vector>

namespace Forscape {

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
    void createToken(ForscapeTokenType type) alloc_except;
    void scanNumber() alloc_except;
    void scanIdentifier() alloc_except;
    void unrecognizedSymbol(uint32_t code) alloc_except;
    void scanConstruct(ForscapeTokenType type) alloc_except;
    void close() alloc_except;
    void error(ErrorCode code) alloc_except;
    void newline() alloc_except;
    void endOfFile() alloc_except;
    void incrementScope() noexcept;
    void decrementScope() noexcept;
    void scanFilePath() alloc_except;
    void scanFile(std::string_view path) alloc_except;
    void importModel(Typeset::Model* imported_model) alloc_except;

    Typeset::Model* model;
    Typeset::Controller* controller;
    std::vector<Error>& errors;
    static FORSCAPE_STATIC_MAP<std::string_view, ForscapeTokenType> keywords; //EVENTUALLY: look at keyword perfect hashing
    size_t scope_depth = 0;
};

}

}

#endif // FORSCAPE_SCANNER_H
