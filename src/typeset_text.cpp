#include "typeset_text.h"

#include "hope_unicode.h"
#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_selection.h"
#include "typeset_subphrase.h"

#ifndef HOPE_TYPESET_HEADLESS
#include "typeset_painter.h"
#endif

#include <algorithm>
#include <cassert>
#include <iostream>

namespace Hope {

namespace Typeset {

#ifdef TYPESET_MEMORY_DEBUG
std::unordered_set<Text*> Text::all;

Text::Text() {
    all.insert(this);
}

Text::~Text() {
    all.erase(this);
}
#endif

void Text::setParent(Phrase* p) noexcept{
    parent = p;
}

void Text::writeString(std::string& out, size_t& curr) const noexcept{
    for(const char& ch : str) out[curr++] = ch;
}

void Text::writeString(std::string& out, size_t& curr, size_t pos, size_t len) const noexcept{
    size_t stop = (len == std::string::npos) ? size() : pos + len;
    for(size_t i = pos; i < stop; i++)
        out[curr++] = at(i);
}

bool Text::isTopLevel() const noexcept{
    return parent->isLine();
}

bool Text::isNested() const noexcept{
    return !isTopLevel();
}

size_t Text::size() const noexcept{
    return str.size();
}

bool Text::empty() const noexcept{
    return str.empty();
}

char Text::at(size_t index) const noexcept{
    return str[index];
}

std::string Text::substr(size_t pos, size_t len) const{
    return str.substr(pos, len);
}

std::string_view Text::codepointAt(size_t index) const noexcept{
    return std::string_view(str.data()+index, codepointSize(at(index)));
}

std::string_view Text::graphemeAt(size_t index) const noexcept{
    return std::string_view(str.data()+index, graphemeSize(str, index));
}

size_t Text::leadingSpaces() const noexcept{
    for(size_t i = 0; i < size(); i++)
        if(str[i] != ' ') return i;
    return size();
}

std::string_view Text::checkKeyword(size_t iR) const noexcept{
    auto slash = str.rfind('\\', iR);
    return slash == std::string::npos ?
           std::string_view() :
                std::string_view(str.data()+slash+1, iR-(slash+1));
}

void Text::findCaseInsensitive(const std::string& target, std::vector<Selection>& hits){
    auto result = str.find(target);
    while(result != std::string::npos){
        hits.push_back( Selection(this, result, result+target.size()) );
        result = str.find(target, result+target.size());
    }
}

bool Text::precedes(Text* other) const noexcept{
    assert(getModel() == other->getModel());

    //DO THIS - this is not entirely accurate
    #ifndef HOPE_TYPESET_HEADLESS
    if(y != other->y) return y < other->y;
    else return x < other->x;
    #else
    assert(false); //DO THIS - this should work without geometry
    return false;
    #endif
}

Phrase* Text::getParent() const noexcept{
    //Not great to expose this detail, but I can't find a better way, and it should be stable
    return parent;
}

Line* Text::getLine() const noexcept{
    Phrase* p = parent;
    while(!p->isLine()) p = static_cast<Subphrase*>(p)->parent->parent;
    return static_cast<Line*>(p);
}

Model* Text::getModel() const noexcept{
    return getLine()->parent;
}

Text* Text::nextTextInPhrase() const noexcept{
    return parent->nextTextInPhrase(this);
}

Text* Text::prevTextInPhrase() const noexcept{
    return parent->prevTextInPhrase(this);
}

Construct* Text::nextConstructInPhrase() const noexcept{
    return parent->nextConstructInPhrase(this);
}

Construct* Text::prevConstructInPhrase() const noexcept{
    return parent->prevConstructInPhrase(this);
}

Text* Text::nextTextAsserted() const noexcept{
    return parent->nextTextAsserted(this);
}

Text* Text::prevTextAsserted() const noexcept{
    return parent->prevTextAsserted(this);
}

Construct* Text::nextConstructAsserted() const noexcept{
    return parent->nextConstructAsserted(this);
}

Construct* Text::prevConstructAsserted() const noexcept{
    return parent->prevConstructAsserted(this);
}

SemanticType Text::getTypeLeftOf(size_t index) const noexcept{
    if(tags.empty() || tags.front().index > index){
        return getTypePrev();
    }else{
        for(auto tag = tags.rbegin(); tag != tags.rend(); tag++)
            if(tag->index <= index) return tag->type;
        assert(false);
        return SEM_DEFAULT;
    }
}

SemanticType Text::getTypePrev() const noexcept{
    for(Text* t = prevTextInPhrase(); t != nullptr; t = t->prevTextInPhrase()){
        assert(t->parent == parent);
        if(!t->tags.empty())
            return t->tags.back().type;
    }

    return SEM_DEFAULT;
}

void Text::tag(SemanticType type, size_t start, size_t stop){
    assert(std::is_sorted(tags.begin(), tags.end(), [](auto& a, auto& b){return a.index < b.index;}));

    SemanticType type_after = getTypeLeftOf(stop);

    auto it = std::remove_if(
                 tags.begin(),
                 tags.end(),
                 [start, stop](const SemanticTag& tag) {
                     return tag.index >= start && tag.index <= stop;
                 }
            );

    tags.erase(it, tags.end());

    assert(std::is_sorted(tags.begin(), tags.end(), [](auto& a, auto& b){return a.index < b.index;}));

    it = tags.begin();
    while(it != tags.end() && it->index < start) it++;
    tags.insert(it, {SemanticTag(start, type), SemanticTag(stop, type_after)});

    assert(std::is_sorted(tags.begin(), tags.end(), [](auto& a, auto& b){return a.index < b.index;}));
}

void Text::tagBack(SemanticType type){
    assert(tags.empty() || tags.back().index != size());
    tags.push_back( SemanticTag(size(), type) );
}

#ifdef HOPE_SEMANTIC_DEBUGGING
std::string Text::toSerialWithSemanticTags() const{
    size_t start = 0;
    std::string out;
    for(const SemanticTag& tag : tags){
        out += str.substr(start, tag.index-start);
        out += "<tag|" + std::to_string(tag.type) + ">";
        start = tag.index;
    }
    out += str.substr(start);

    return out;
}
#endif

#ifndef HOPE_TYPESET_HEADLESS
double Text::aboveCenter() const {
    return getAboveCenter(SEM_DEFAULT, scriptDepth());
}

double Text::underCenter() const {
    return getUnderCenter(SEM_DEFAULT, scriptDepth());
}

double Text::height() const {
    return getHeight(SEM_DEFAULT, scriptDepth());
}

void Text::updateWidth(){
    width = 0;
    uint8_t depth = scriptDepth();
    SemanticType type = getTypePrev();
    size_t start = 0;

    for(const SemanticTag& tag : tags){
        width += Hope::Typeset::getWidth(type, depth, str.substr(start, tag.index-start));
        start = tag.index;
        type = tag.type;
    }
    width += Hope::Typeset::getWidth(type, depth, str.substr(start));
}

double Text::xLocal(size_t index) const{
    double left = 0;
    uint8_t depth = scriptDepth();
    SemanticType type = getTypePrev();
    size_t start = 0;

    for(const SemanticTag& tag : tags){
        if(tag.index >= index) break;
        left += Hope::Typeset::getWidth(type, depth, str.substr(start, tag.index-start));
        start = tag.index;
        type = tag.type;
    }
    left += Hope::Typeset::getWidth(type, depth, str.substr(start, index-start));

    return left;
}

double Text::xPhrase(size_t index) const{
    double left = xLocal(index);

    const Text* t = this;
    while(Construct* c = t->prevConstructInPhrase()){
        left += c->width;
        t = c->prev();
        left += t->getWidth();
    }

    return left;
}

double Text::xGlobal(size_t index) const{
    return x + xLocal(index);
}

double Text::xRight() const noexcept{
    return x + width;
}

double Text::getWidth() const noexcept{
    //DO THIS - assert width is up to date
    return width;
}

uint8_t Text::scriptDepth() const noexcept{
    return parent->script_level;
}

size_t Text::indexNearest(double x_in) const noexcept{
    x_in -= x;

    if(x_in <= 0) return 0;
    else if(x_in >= width) return size();

    double left = 0;
    uint8_t depth = scriptDepth();
    SemanticType type = getTypePrev();
    size_t index = 0;

    for(const SemanticTag& tag : tags){
        while(index < tag.index){
            size_t glyph_size = codepointSize(str[index]);
            double w = glyph_size == 1 ?
                       Hope::Typeset::getWidth(type, depth, str[index]) :
                       Hope::Typeset::getWidth(type, depth, str.substr(index, glyph_size));
            if(left+w/2 >= x_in) return index;
            left += w;
            index += glyph_size;
        }

        type = tag.type;
    }
    while(index < size()){
        size_t glyph_size = codepointSize(str[index]);
        double w = glyph_size == 1 ?
                   Hope::Typeset::getWidth(type, depth, str[index]) :
                   Hope::Typeset::getWidth(type, depth, str.substr(index, glyph_size));
        if(left+w/2 >= x_in) return index;
        left += w;
        index += glyph_size;
    }

    return size();
}

size_t Text::indexLeft(double x_in) const noexcept{
    x_in -= x;

    if(x_in <= 0) return 0;
    else if(x_in >= width) return size();

    double left = 0;
    uint8_t depth = scriptDepth();
    SemanticType type = getTypePrev();
    size_t index = 0;

    for(const SemanticTag& tag : tags){
        while(index < tag.index){
            size_t glyph_size = codepointSize(str[index]);
            left += glyph_size == 1 ?
                    Hope::Typeset::getWidth(type, depth, str[index]) :
                    Hope::Typeset::getWidth(type, depth, str.substr(index, glyph_size));
            if(left > x_in) return index;
            index += glyph_size;
        }

        type = tag.type;
    }
    while(index < size()){
        size_t glyph_size = codepointSize(str[index]);
        left += glyph_size == 1 ?
                Hope::Typeset::getWidth(type, depth, str[index]) :
                Hope::Typeset::getWidth(type, depth, str.substr(index, glyph_size));
        if(left > x_in) return index;
        index += glyph_size;
    }

    return size();
}

void Text::paint(Painter& painter, bool forward) const{
    size_t start = 0;
    double x = this->x;
    for(const SemanticTag& tag : tags){
        std::string substr = str.substr(start, tag.index-start);
        painter.drawText(x, y, substr, forward);
        x += painter.getWidth(substr);
        start = tag.index;
        painter.setType(tag.type);
    }
    painter.drawText(x, y, str.substr(start), forward);
}

void Text::paintUntil(Painter& painter, size_t stop, bool forward) const{
    size_t start = 0;
    double x = this->x;
    for(const SemanticTag& tag : tags){
        if(tag.index >= stop) break;
        std::string substr = str.substr(start, tag.index-start);
        painter.drawText(x, y, substr, forward);
        x += painter.getWidth(substr);
        start = tag.index;
        painter.setType(tag.type);
    }
    painter.drawText(x, y, str.substr(start, stop - start), forward);
}

void Text::paintAfter(Painter& painter, size_t start, bool forward) const{
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            std::string substr = str.substr(start, tag.index-start);
            painter.drawText(x, y, substr, forward);
            x += painter.getWidth(substr);
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    painter.drawText(x, y, str.substr(start), forward);
}

void Text::paintMid(Painter& painter, size_t start, size_t stop, bool forward) const{
    painter.setScriptLevel(scriptDepth());
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            if(tag.index >= stop) break;
            std::string substr = str.substr(start, tag.index-start);
            painter.drawText(x, y, substr, forward);
            x += painter.getWidth(substr);
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    painter.drawText(x, y, str.substr(start, stop-start), forward);
}

void Text::paintGrouping(Painter& painter, size_t start) const{
    assert(start < size());
    size_t stop = start + codepointSize(str[start]);

    painter.setScriptLevel(scriptDepth());
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            if(tag.index >= stop) break;
            std::string substr = str.substr(start, tag.index-start);
            double width = painter.getWidth(substr);
            painter.drawHighlightedGrouping(x, y, width, substr);
            x += width;
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    std::string substr = str.substr(start, stop-start);
    double width = painter.getWidth(substr);
    painter.drawHighlightedGrouping(x, y, width, substr);
}

bool Text::containsX(double x_test) const noexcept{
    return (x_test >= x) & (x_test <= x + width);
}

bool Text::containsY(double y_test) const noexcept{
    return (y_test >= y) & (y_test <= y + height());
}

bool Text::containsXInBounds(double x_test, size_t start, size_t stop) const noexcept{
    return x_test >= xGlobal(start) && x_test <= xGlobal(stop);
}

void Text::resize(){
    updateWidth();
    parent->resize();
}
#endif

}

}
