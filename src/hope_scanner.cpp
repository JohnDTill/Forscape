#include "hope_scanner.h"

#include <construct_codes.h>
#include <typeset_controller.h>
#include <typeset_model.h>
#include <typeset_line.h>
#include <string_view>
#include <unordered_map>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

Scanner::Scanner(Typeset::Model* model) noexcept :
    model(model), errors(model->errors) {
}

void Scanner::scanAll() alloc_except {
    model->clearFormatting();
    tokens.clear();
    scope_depth = 0;

    controller = new Typeset::Controller(model);
    controller->moveToStartOfDocument();

    do{
        scanToken();
    } while(tokens.back().type != ENDOFFILE);

    delete controller;
}

void Scanner::scanToken() alloc_except {
    controller->skipWhitespace();

    uint32_t code = controller->scan();

    //EVENTUALLY: look at symbol perfect hashing
    switch (code) {
        HOPE_SCANNER_CASES

        #ifdef HOPE_IDENTIFIERS_USE_MULTIPLE_CHARS
        case '_': scanIdentifier(); break;
        #endif

        case CLOSE: close(); break;
        case '\n': newline(); break;
        case '\0': endOfFile(); break;

        default:
            unrecognizedSymbol();
    }
}

void Scanner::scanString() alloc_except {
    if(controller->selectToStringEnd()){
        controller->formatString();
        createToken(STRING);
    }else{
        controller->formatString();
        error(UNTERMINATED_STRING);
    }
}

void Scanner::forwardSlash() alloc_except {
    if(controller->peek('/')) comment();
    else createToken(FORWARDSLASH);
}

void Scanner::comment() alloc_except {
    controller->selectEndOfLine();
    controller->formatComment();
    controller->anchor.index += 2;

    createToken(COMMENT);
}

void Scanner::createToken(TokenType type) alloc_except {
    tokens.push_back( Token(Typeset::Selection(controller->anchor, controller->active), type) );
}

void Scanner::scanNumber() alloc_except {
    controller->selectToNumberEnd();
    createToken(INTEGER);
}

static_map<std::string_view, TokenType> Scanner::keywords {
    HOPE_KEYWORD_MAP
};

void Scanner::scanIdentifier() alloc_except {
    controller->selectToIdentifierEnd();

    auto lookup = keywords.find(controller->selectedFlatText());
    if(lookup != keywords.end()){
        controller->formatKeyword();
        createToken(lookup->second);
    }else{
        controller->formatBasicIdentifier();
        createToken(IDENTIFIER);
    }
}

void Scanner::unrecognizedSymbol() alloc_except {
    error(UNRECOGNIZED_SYMBOL);
}

void Scanner::scanConstruct(TokenType type) alloc_except {
    createToken(type);
    controller->consolidateToAnchor();
    controller->moveToNextChar();
}

void Scanner::close() alloc_except {
    createToken(ARGCLOSE);
    controller->consolidateToActive();
}

void Scanner::error(ErrorCode code) alloc_except {
    createToken(SCANNER_ERROR);
    errors.push_back(Error(tokens.back().sel, code));
}

void Scanner::newline() alloc_except {
    controller->anchorLine()->scope_depth = scope_depth;
    createToken(NEWLINE);
}

void Scanner::endOfFile() alloc_except {
    controller->anchorLine()->scope_depth = scope_depth;
    createToken(ENDOFFILE);
}

void Scanner::incrementScope() noexcept{
    scope_depth++;
}

void Scanner::decrementScope() noexcept{
    if(scope_depth) scope_depth--;
}

}

}
