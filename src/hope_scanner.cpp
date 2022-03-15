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
    markers.clear();
    scope_depth = 0;

    controller = new Typeset::Controller(model);
    controller->moveToStartOfDocument();

    do{
        tokens.push_back(scanToken());
    } while(tokens.back() != ENDOFFILE);

    delete controller;
}

TokenType Scanner::scanToken(){
    controller->skipWhitespace();

    uint32_t code = controller->scan();

    switch (code) {
        HOPE_SCANNER_CASES

        #ifdef HOPE_IDENTIFIERS_USE_MULTIPLE_CHARS
        case '_': return scanIdentifier();
        #endif

        case CLOSE: return close();
        case '\n': return newline();
        case '\0': return endOfFile();

        default:
            return unrecognizedSymbol();
    }
}

TokenType Scanner::scanString() {
    if(controller->selectToStringEnd()){
        controller->formatString();
        return createToken(STRING);
    }else{
        controller->formatString();
        return error(UNTERMINATED_STRING);
    }
}

TokenType Scanner::forwardSlash() {
    if(controller->peek('/')) return comment();
    else return createToken(FORWARDSLASH);
}

TokenType Scanner::comment() {
    controller->selectEndOfLine();
    controller->formatComment();
    controller->anchor.index += 2;

    return createToken(COMMENT);
}

TokenType Scanner::createToken(TokenType type) {
    markers.push_back( std::make_pair(controller->getAnchor(), controller->getActive()) );
    return type;
}

TokenType Scanner::scanNumber() {
    controller->selectToNumberEnd();
    return createToken(INTEGER);
}

const std::unordered_map<std::string_view, TokenType> Scanner::keywords {
    HOPE_KEYWORD_MAP
};

TokenType Scanner::scanIdentifier() {
    controller->selectToIdentifierEnd();

    auto lookup = keywords.find(controller->selectedFlatText());
    if(lookup != keywords.end()){
        controller->formatKeyword();
        return createToken(lookup->second);
    }else{
        controller->formatBasicIdentifier();
        return createToken(IDENTIFIER);
    }
}

TokenType Scanner::unrecognizedSymbol(){
    return error(UNRECOGNIZED_SYMBOL);
}

TokenType Scanner::scanConstruct(TokenType type) {
    TokenType t = createToken(type);
    controller->consolidateToAnchor();
    controller->moveToNextChar();
    return t;
}

TokenType Scanner::close() {
    markers.push_back( std::make_pair(controller->getAnchor(), controller->getActive()) );
    controller->consolidateToActive();
    return ARGCLOSE;
}

TokenType Scanner::error(ErrorCode code) {
    TokenType type = createToken(SCANNER_ERROR);
    errors.push_back(Error(Typeset::Selection(markers.back()), code));
    return type;
}

TokenType Scanner::newline() {
    controller->anchorLine()->scope_depth = scope_depth;
    return createToken(NEWLINE);
}

TokenType Scanner::endOfFile() {
    controller->anchorLine()->scope_depth = scope_depth;
    return createToken(ENDOFFILE);
}

void Scanner::incrementScope() noexcept{
    scope_depth++;
}

void Scanner::decrementScope() noexcept{
    if(scope_depth) scope_depth--;
}

}

}
