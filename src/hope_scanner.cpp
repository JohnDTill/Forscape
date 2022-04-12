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

Scanner::Scanner(Typeset::Model* model) :
    model(model), errors(model->errors) {
}

void Scanner::scanAll(){
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

void Scanner::scanToken(){
    controller->skipWhitespace();

    uint32_t code = controller->scan();

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

void Scanner::scanString() {
    if(controller->selectToStringEnd()){
        controller->formatString();
        createToken(STRING);
    }else{
        controller->formatString();
        error(UNTERMINATED_STRING);
    }
}

void Scanner::forwardSlash() {
    if(controller->peek('/')) comment();
    else createToken(FORWARDSLASH);
}

void Scanner::comment() {
    controller->selectEndOfLine();
    controller->formatComment();
    controller->anchor.index += 2;

    createToken(COMMENT);
}

void Scanner::createToken(TokenType type) {
    tokens.push_back( Token(Typeset::Selection(controller->anchor, controller->active), type) );
}

void Scanner::scanNumber() {
    controller->selectToNumberEnd();
    createToken(INTEGER);
}

const std::unordered_map<std::string_view, TokenType> Scanner::keywords {
    HOPE_KEYWORD_MAP
};

void Scanner::scanIdentifier() {
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

void Scanner::unrecognizedSymbol(){
    error(UNRECOGNIZED_SYMBOL);
}

void Scanner::scanConstruct(TokenType type) {
    createToken(type);
    controller->consolidateToAnchor();
    controller->moveToNextChar();
}

void Scanner::close() {
    createToken(ARGCLOSE);
    controller->consolidateToActive();
}

void Scanner::error(ErrorCode code) {
    createToken(SCANNER_ERROR);
    errors.push_back(Error(tokens.back().sel, code));
}

void Scanner::newline() {
    controller->anchorLine()->scope_depth = scope_depth;
    createToken(NEWLINE);
}

void Scanner::endOfFile() {
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
