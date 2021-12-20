#include "typeset_marker.h"

#include "hope_unicode.h"
#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_model.h"
#include "typeset_phrase.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"
#include <cassert>

namespace Hope {

namespace Typeset {

Marker::Marker()
    : text(nullptr), index(0) {}

Marker::Marker(const Model* model) noexcept
    : text(model->lastText()), index(text->size()) {}

Marker::Marker(Text* t, size_t i) noexcept
    : text(t), index(i) {}

void Marker::setToFrontOf(Text* t) noexcept{
    text = t;
    index = 0;
}

void Marker::setToBackOf(Text* t) noexcept{
    text = t;
    index = t->size();
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
    return text->at(index);
}

char Marker::charLeft() const noexcept{
    assert(notAtTextStart());
    return text->at(index-1);
}

Phrase* Marker::phrase() const noexcept{
    return text->getParent();
}

Subphrase* Marker::subphrase() const noexcept{
    assert(!isTopLevel());
    return phrase()->asSubphrase();
}

Line* Marker::line() const noexcept{
    assert(isTopLevel());
    return phrase()->asLine();
}

Line* Marker::nextLine() const noexcept{
    return line()->next();
}

Line* Marker::prevLine() const noexcept{
    return line()->prev();
}

bool Marker::atTextEnd() const noexcept{
    return index == text->size();
}

bool Marker::atTextStart() const noexcept{
    return index == 0;
}

bool Marker::notAtTextEnd() const noexcept{
    return index < text->size();
}

bool Marker::notAtTextStart() const noexcept{
    return index;
}

bool Marker::atFirstTextInPhrase() const noexcept{
    return text->id == 0;
}

void Marker::incrementIndex() noexcept{
    assert(notAtTextEnd());
    index += glyphSize(charRight());
}

void Marker::decrementIndex() noexcept{
    assert(notAtTextStart());
    do{ index--; } while(isContinuationCharacter(charRight()));
}

void Marker::incrementToNextWord() noexcept{
    char ch = charRight();

    if(isAlphaNumeric(ch)){
        do{
            index++;
        } while(notAtTextEnd() && isAlphaNumeric( charRight() ));
    }else{
        do {
            incrementIndex();
        } while(notAtTextEnd() && !isAlphaNumeric( charRight() ));
    }
}

void Marker::decrementToPrevWord() noexcept{
    decrementIndex();
    char ch = charRight();

    if(isAlphaNumeric(ch)){
        while(index && isAlphaNumeric( charRight() ))
            decrementIndex();
        if(index!=0 || !isAlphaNumeric( text->at(0) ))
            incrementIndex();
    }else{
        while(index && !isAlphaNumeric( charRight() ))
            decrementIndex();
        if(index!=0 || isAlphaNumeric( text->at(0) ))
            incrementIndex();
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
        if(text->at(i) != ' ') return index - (i + 1);
    }
}

std::string_view Marker::checkKeyword() const noexcept{
    return text->checkKeyword(index);
}

uint32_t Marker::codepointLeft() const noexcept{
    if(index == 0) return 0;

    Marker m = *this;
    m.decrementIndex();

    return m.scanGlyph();
}

bool Marker::onlySpacesLeft() const noexcept{
    if(!atFirstTextInPhrase()) return false;
    size_t i = index;
    while(i){
        i--;
        if(text->at(i) != ' ') return false;
    }
    return true;
}

std::string_view Marker::strRight() const noexcept{
    return std::string_view(text->str.data()+index);
}

std::string_view Marker::strLeft() const noexcept{
    return std::string_view(text->str.data(), index);
}

bool Marker::compareRight(const Marker& other) const noexcept{
    return strRight() == other.strRight();
}

bool Marker::compareLeft(const Marker& other) const noexcept{
    return strLeft() == other.strLeft();
}

#ifndef HOPE_TYPESET_HEADLESS
double Marker::x() const{
    return text->xGlobal(index);
}

void Marker::setToPointOf(Text* t, double setpoint) {
    text = t;
    index = text->indexNearest(setpoint);
}

void Marker::setToLeftOf(Text* t, double setpoint) {
    text = t;
    index = text->indexLeft(setpoint);
}
#endif

uint32_t Marker::advance() noexcept{
    assert(notAtTextEnd());
    return static_cast<uint8_t>(text->at(index++));
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
    } while(text->at(index++) != '"');

    return true;
}

}

}
