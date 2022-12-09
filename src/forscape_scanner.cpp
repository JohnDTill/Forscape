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
    if(lookup != keywords.end()){
        controller->formatKeyword();

        if(lookup->second == INCLUDE){
            createToken(INCLUDE);
            controller->skipWhitespace();
            controller->selectToPathEnd();
            if(!controller->hasSelection()){
                errors.push_back(Error(controller->selection(), EXPECTED_FILEPATH));
            }else{
                createToken(FILEPATH);
                controller->formatSimple(SEM_FILE);
                scanFile(controller->selectedFlatText());
            }
        }else{
            createToken(lookup->second);
        }
    }else{
        controller->formatBasicIdentifier();
        createToken(IDENTIFIER);
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

void Scanner::scanFile(std::string_view path) noexcept {
    //DO THIS - guard against cyclical includes
    Program::ptr_or_code ptr_or_code = Program::instance()->openFromRelativePath(path);

    switch (ptr_or_code) {
        case Program::FILE_NOT_FOUND: error(FILE_NOT_FOUND); break;
        case Program::FILE_CORRUPTED: error(FILE_CORRUPTED); break;
        case Program::FILE_ALREADY_OPEN: break;
        default: importModel(reinterpret_cast<Typeset::Model*>(ptr_or_code));
    }
}

void Scanner::importModel(Typeset::Model* imported_model) noexcept {
    //DO THIS: this horrible kludge!

    Typeset::Model* old_model = model;
    Typeset::Controller* old_controller = controller;
    size_t old_scope_depth = scope_depth;

    model = imported_model;
    model->clearFormatting();
    controller = new Typeset::Controller(model);
    controller->moveToStartOfDocument();

    do{
        scanToken();
    } while(tokens.back().type != ENDOFFILE);
    tokens.pop_back();

    delete controller;

    model = old_model;
    controller = old_controller;
    scope_depth = old_scope_depth;
}

}

}
