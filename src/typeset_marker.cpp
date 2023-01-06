#include "typeset_marker.h"

#include "forscape_unicode.h"
#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_model.h"
#include "typeset_phrase.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"
#include <unicode_zerowidth.h>
#include <cassert>

namespace Forscape {

namespace Typeset {

Marker::Marker() {}

Marker::Marker(const Model* model) noexcept
    : text(model->lastText()), index(text->numChars()) {}

Marker::Marker(Text* t, size_t i) noexcept
    : text(t), index(i) {}

bool Marker::precedesInclusive(const Marker& other) const noexcept{
    assert(getModel() == other.getModel());
    if(text != other.text) return text->precedes(other.text);
    else return index <= other.index;
}

void Marker::setToFrontOf(Text* t) noexcept{
    text = t;
    index = 0;
}

void Marker::setToBackOf(Text* t) noexcept{
    text = t;
    index = t->numChars();
}

void Marker::setToFrontOf(const Phrase* p) noexcept{
    setToFrontOf(p->front());
}

void Marker::setToBackOf(const Phrase* p) noexcept{
    setToBackOf(p->back());
}

bool Marker::isTopLevel() const noexcept{
    return text->isTopLevel();
}

bool Marker::isNested() const noexcept{
    return text->isNested();
}

char Marker::charRight() const noexcept{
    assert(notAtTextEnd());
    return text->charAt(index);
}

char Marker::charLeft() const noexcept{
    assert(notAtTextStart());
    return text->charAt(index-1);
}

Phrase* Marker::phrase() const noexcept{
    return text->getParent();
}

Subphrase* Marker::subphrase() const noexcept{
    assert(!isTopLevel());
    return phrase()->asSubphrase();
}

Line* Marker::getLine() const noexcept{
    return text->getLine();
}

Line* Marker::parentAsLine() const noexcept{
    assert(isTopLevel());
    return phrase()->asLine();
}

Line* Marker::nextLine() const noexcept{
    return parentAsLine()->next();
}

Line* Marker::prevLine() const noexcept{
    return parentAsLine()->prev();
}

bool Marker::atTextEnd() const noexcept{
    return index == text->numChars();
}

bool Marker::atTextStart() const noexcept{
    return index == 0;
}

bool Marker::notAtTextEnd() const noexcept{
    return index < text->numChars();
}

bool Marker::notAtTextStart() const noexcept{
    return index;
}

bool Marker::atFirstTextInPhrase() const noexcept{
    return text->id == 0;
}

void Marker::incrementCodepoint() noexcept{
    assert(notAtTextEnd());
    index += codepointSize(charRight());
}

void Marker::decrementCodepoint() noexcept{
    assert(notAtTextStart());
    do{ index--; } while(isContinuationCharacter(charRight()));
}

void Marker::incrementGrapheme() noexcept{
    assert(notAtTextEnd());

    incrementCodepoint();
    size_t backup = index;
    while(index < text->numChars() && isZeroWidth(scanGlyph()))
        backup = index;
    index = backup;
}

void Marker::decrementGrapheme() noexcept{
    assert(notAtTextStart());

    while(index && isZeroWidth(codepointLeft()))
        decrementCodepoint();
    if(index) decrementCodepoint();
}

void Marker::incrementToNextWord() noexcept{
    char ch = charRight();

    if(isAlphaNumeric(ch)){
        do{
            incrementGrapheme();
        } while(notAtTextEnd() && isAlphaNumeric( charRight() ));
    }else{
        do {
            incrementGrapheme();
        } while(notAtTextEnd() && !isAlphaNumeric( charRight() ));
    }
}

void Marker::decrementToPrevWord() noexcept{
    decrementGrapheme();
    char ch = charRight();

    if(isAlphaNumeric(ch)){
        while(index && isAlphaNumeric( charRight() ))
            decrementGrapheme();
        if(index!=0 || !isAlphaNumeric( text->charAt(0) ))
            incrementGrapheme();
    }else{
        while(index && !isAlphaNumeric( charRight() ))
            decrementGrapheme();
        if(index!=0 || isAlphaNumeric( text->charAt(0) ))
            incrementGrapheme();
    }
}

bool Marker::operator==(const Marker& other) const noexcept{
    return (text == other.text) & (index == other.index);
}

bool Marker::operator!=(const Marker& other) const noexcept{
    return (text != other.text) | (index != other.index);
}

size_t Marker::countSpacesLeft() const noexcept{
    size_t i = index;
    for(;;){
        if(i == 0) return index - i;
        i--;
        if(text->charAt(i) != ' ') return index - (i + 1);
    }
}

std::string_view Marker::checkKeyword() const noexcept{
    return text->checkKeyword(index);
}

uint32_t Marker::codepointLeft() const noexcept{
    if(index == 0) return 0;

    Marker m = *this;
    m.decrementCodepoint();

    return m.scanGlyph();
}

bool Marker::onlySpacesLeft() const noexcept{
    if(!atFirstTextInPhrase()) return false;
    size_t i = index;
    while(i){
        i--;
        if(text->charAt(i) != ' ') return false;
    }
    return true;
}

std::string_view Marker::strRight() const noexcept{
    return std::string_view(text->data()+index);
}

std::string_view Marker::strLeft() const noexcept{
    return std::string_view(text->data(), index);
}

bool Marker::compareRight(const Marker& other) const noexcept{
    return strRight() == other.strRight();
}

bool Marker::compareLeft(const Marker& other) const noexcept{
    return strLeft() == other.strLeft();
}

Model* Marker::getModel() const noexcept{
    return text->getModel();
}

bool Marker::goToCommandStart() noexcept {
    assert(index > 0);
    size_t open_args = 0;
    bool found_command = false;
    for(;;){
        if(index == 0){
            if(text->id == 0) return found_command;
            text = text->prevTextAsserted();
            index = text->numChars();
        }else{
            switch (text->charAt(--index)) {
                case user_close:
                    if(index == 0 || text->charAt(index-1) != user_cmd) open_args++;
                    break;
                case user_open:
                    if((index == 0 || text->charAt(index-1) != user_cmd) && open_args-- == 0) return false;
                    break;
                case user_cmd: {
                    const bool escaped = (index > 0 && text->charAt(index-1) == user_cmd);
                    found_command |= !escaped;
                    index -= escaped;
                    break;
                }
                case ' ': if(open_args == 0) return found_command;
            }
        }
    }
}

#ifndef FORSCAPE_TYPESET_HEADLESS
double Marker::x() const{
    return text->xGlobal(index);
}

double Marker::y() const noexcept {
    return text->y;
}

double Marker::yBot() const noexcept {
    return text->yBot();
}

void Marker::setToPointOf(Text* t, double setpoint) {
    text = t;
    index = text->charIndexNearest(setpoint);
}

void Marker::setToLeftOf(Text* t, double setpoint) {
    text = t;
    index = text->charIndexLeft(setpoint);
}

std::pair<ParseNode, ParseNode> Marker::parseNodesAround() const noexcept{
    return std::make_pair(parseNodeLeft(), parseNodeRight());
}

ParseNode Marker::parseNodeLeft() const noexcept{
    if(index > 0) return text->parseNodeAtIndex(index-1);
    else if(text->id > 0) return text->prevConstructAsserted()->pn;
    else return NONE;
}

ParseNode Marker::parseNodeRight() const noexcept{
    if(index < text->numChars()) return text->parseNodeAtIndex(index);
    else if(const Construct* c = text->nextConstructInPhrase()) return c->pn;
    else return NONE;
}

size_t Marker::absoluteIndexInPhrase() const noexcept {
    size_t absolute_index = index;
    for(size_t i = text->id; i-->0;) absolute_index += phrase()->text(i)->numChars();
    return absolute_index;
}

Marker Marker::fromAbsoluteIndex(const Phrase& p, size_t absolute_index) noexcept {
    Text* t = p.front();
    for(size_t num_chars = t->numChars(); absolute_index > num_chars; num_chars = t->numChars()){
        absolute_index -= num_chars;
        t = t->nextTextAsserted();
    }
    return Marker(t, absolute_index);
}
#endif

uint32_t Marker::advance() noexcept{
    assert(notAtTextEnd());
    return static_cast<uint8_t>(text->charAt(index++));
}

bool Marker::peek(char ch) const noexcept{
    return notAtTextEnd() && charRight() == ch;
}

void Marker::skipWhitespace() noexcept{
    while(notAtTextEnd() && (charRight() == ' ')) index++;
}

uint32_t Marker::scan() noexcept{
    if(notAtTextEnd()){
        return scanGlyph();
    }else if(Construct* c = text->nextConstructInPhrase()){
        setToFrontOf(c->next());
        return constructScannerCode(c->constructCode());
    }else if(isNested()){
        setToFrontOf(subphrase()->textRightOfSubphrase());
        return CLOSE;
    }else if(Line* l = nextLine()){
        setToFrontOf(l->front());
        return '\n';
    }else{
        return '\0';
    }
}

uint32_t Marker::scanGlyph() noexcept{
    uint32_t ch = advance();
    if(isAscii(ch)){
        return ch;
    }else if(sixthBitUnset(ch)){
        return ch | (advance() << 8);
    }else if(fifthBitUnset(ch)){
        uint32_t bit2 = advance() << 8;
        uint32_t bit3 = advance() << 16;
        return ch | bit2 | bit3;
    }else{
        uint32_t bit2 = advance() << 8;
        uint32_t bit3 = advance() << 16;
        uint32_t bit4 = advance() << 24;
        return ch | bit2 | bit3 | bit4;
    }
}

void Marker::selectToIdentifierEnd() noexcept{
    while(notAtTextEnd() && isAlphaNumeric(charRight())) index++;
}

void Marker::selectToNumberEnd() noexcept{
    while(notAtTextEnd() && isNumeric(charRight())) index++;
}

bool Marker::selectToStringEnd() noexcept{
    do {
        while(atTextEnd()){
            if(Text* t = text->nextTextInPhrase()) text = t;
            else return false;
        }
    } while(text->charAt(index++) != '"');

    return true;
}

}

}
