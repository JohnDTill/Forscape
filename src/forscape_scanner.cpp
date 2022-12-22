#include "forscape_scanner.h"

#include "construct_codes.h"
#include "forscape_program.h"
#include "forscape_unicode.h"
#include "typeset_controller.h"
#include "typeset_model.h"
#include "typeset_line.h"
#include <string_view>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

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
        FORSCAPE_SCANNER_CASES

        #ifdef FORSCAPE_IDENTIFIERS_USE_MULTIPLE_CHARS
        case '_': scanIdentifier(); break;
        #endif

        case CLOSE: close(); break;
        case '\n': newline(); break;
        case '\0': endOfFile(); break;

        default:
            unrecognizedSymbol(code);
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

void Scanner::createToken(ForscapeTokenType type) alloc_except {
    tokens.push_back( Token(Typeset::Selection(controller->anchor, controller->active), type) );
}

void Scanner::scanNumber() alloc_except {
    controller->selectToNumberEnd();
    createToken(INTEGER);
}

FORSCAPE_STATIC_MAP<std::string_view, ForscapeTokenType> Scanner::keywords {
    FORSCAPE_KEYWORD_MAP
};

void Scanner::scanIdentifier() alloc_except {
    controller->selectToIdentifierEnd();

    auto lookup = keywords.find(controller->selectedFlatText());
    if(lookup == keywords.end()){
        controller->formatBasicIdentifier();
        createToken(IDENTIFIER);
    }else{
        ForscapeTokenType type = lookup->second;
        controller->formatKeyword();
        createToken(type);

        //Handle special scanning for file paths here
        if(type == IMPORT){
            scanFilePath();
        }else if(type == FROM){
            scanFilePath();
            controller->skipWhitespace();
            controller->selectToIdentifierEnd();
            if(controller->selection().strView() == "import")
                controller->formatKeyword();
            else if(errors.empty())
                errors.push_back(Error(controller->selection(), CONSUME));
        }
    }
}

void Scanner::unrecognizedSymbol(uint32_t code) alloc_except {
    if(isZeroWidth(code)){
        controller->anchor.decrementCodepoint();
        controller->format(SEM_DEFAULT);
    }

    error(UNRECOGNIZED_SYMBOL);
}

void Scanner::scanConstruct(ForscapeTokenType type) alloc_except {
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

void Scanner::scanFilePath() alloc_except {
    //EVENTUALLY: what are the pros & cons of requiring fully hardcoded import paths?
    //e.g. would there be advantages to the import path being calculated by a compile-time expression?

    controller->skipWhitespace();
    controller->selectToPathEnd();

    if(!controller->hasSelection()){
        errors.push_back(Error(controller->selection(), EXPECTED_FILEPATH));
    }else{
        createToken(FILEPATH);
        controller->formatSimple(SEM_FILE);
    }
}

}

}
