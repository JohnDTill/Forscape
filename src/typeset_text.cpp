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

namespace Hope {

namespace Typeset {

#ifdef TYPESET_MEMORY_DEBUG
HOPE_UNORDERED_SET<Text*> Text::all;

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

void Text::writeString(std::string& out, size_t& curr) const noexcept {
    memcpy(&out[curr], str.data(), numChars());
    curr += numChars();
}

void Text::writeString(std::string& out, size_t& curr, size_t pos) const noexcept {
    writeString(out, curr, pos, numChars()-pos);
}

void Text::writeString(std::string& out, size_t& curr, size_t pos, size_t len) const noexcept {
    memcpy(&out[curr], &str[pos], len);
    curr += len;
}

bool Text::isTopLevel() const noexcept{
    return parent->isLine();
}

bool Text::isNested() const noexcept{
    return !isTopLevel();
}

size_t Text::numChars() const noexcept{
    return str.size();
}

bool Text::empty() const noexcept{
    return str.empty();
}

void Text::setString(std::string_view str) noexcept {
    this->str = str;

    #ifndef HOPE_TYPESET_HEADLESS
    resize();
    #endif
}

std::string Text::substr(size_t pos, size_t len) const{
    return str.substr(pos, len);
}

char Text::charAt(size_t index) const noexcept{
    return str[index];
}

std::string_view Text::codepointAt(size_t index) const noexcept{
    return std::string_view(str.data()+index, codepointSize(charAt(index)));
}

std::string_view Text::graphemeAt(size_t index) const noexcept{
    return std::string_view(str.data()+index, numBytesInGrapheme(str, index));
}

size_t Text::leadingSpaces() const noexcept{
    for(size_t i = 0; i < numChars(); i++)
        if(charAt(i) != ' ') return i;
    return numChars();
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
    if(parent == other->parent) return id < other->id;

    size_t depth = parent->nestingDepth();
    size_t other_depth = other->parent->nestingDepth();
    while(other_depth > depth){
        Construct* c = static_cast<Subphrase*>(other->parent)->parent;
        other = c->parent->text(c->id);
        other_depth--;
    }
    if(other->parent == parent) return id <= other->id;
    const Text* t = this;
    while(depth > other_depth){
        Construct* c = static_cast<Subphrase*>(t->parent)->parent;
        t = c->parent->text(c->id);
        depth--;
    }
    if(other->parent == t->parent) return t->id < other->id;
    while(depth){
        Construct* c = static_cast<Subphrase*>(t->parent)->parent;
        Construct* other_c = static_cast<Subphrase*>(other->parent)->parent;
        if(c == other_c) return t->parent->id < other->parent->id;
        t = c->parent->text(c->id);
        other = c->parent->text(c->id);
        if(other->parent == t->parent) return t->id < other->id;
        depth--;
    }

    return t->parent->id < other->parent->id;
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

void Text::tag(SemanticType type, size_t start, size_t stop) alloc_except {
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

void Text::tagBack(SemanticType type) alloc_except {
    assert(tags.empty() || tags.back().index != numChars());
    tags.push_back( SemanticTag(numChars(), type) );
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
double Text::aboveCenter() const noexcept {
    return ABOVE_CENTER[scriptDepth()];
}

double Text::underCenter() const noexcept {
    return UNDER_CENTER[scriptDepth()];
}

double Text::height() const noexcept {
    return CHARACTER_HEIGHTS[scriptDepth()];
}

double Text::xLocal(size_t index) const noexcept {
    return CHARACTER_WIDTHS[scriptDepth()] * countGraphemes(std::string_view(str.data(), index));
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
    return x + getWidth();
}

double Text::yBot() const noexcept{
    return y + height();
}

double Text::getWidth() const noexcept {
    return width;
}

void Text::updateWidth() noexcept {
    width = CHARACTER_WIDTHS[scriptDepth()] * countGraphemes(str);
}

uint8_t Text::scriptDepth() const noexcept {
    return parent->script_level;
}

size_t Text::charIndexNearest(double x_in) const noexcept {
    return charIndexLeft(x_in + CHARACTER_WIDTHS[scriptDepth()]/2);
}

size_t Text::charIndexLeft(double x_in) const noexcept {
    double grapheme_index = (x_in-x) / CHARACTER_WIDTHS[scriptDepth()];
    if(grapheme_index < 0) return 0;
    size_t index = 0;
    for(size_t i = 0; i < static_cast<size_t>(grapheme_index) && index < numChars(); i++)
        index += numBytesInGrapheme(str, index);
    return index;
}

void Text::paint(Painter& painter, bool forward) const {
    size_t start = 0;
    double x = this->x;
    double char_width = CHARACTER_WIDTHS[scriptDepth()];
    for(const SemanticTag& tag : tags){
        std::string_view substr(&str[start], tag.index-start);
        painter.drawText(x, y, substr, forward);
        x += char_width * countGraphemes(substr);
        start = tag.index;
        painter.setType(tag.type);
    }
    painter.drawText(x, y, std::string_view(&str[start], numChars()-start), forward);
}

void Text::paintUntil(Painter& painter, size_t stop, bool forward) const {
    size_t start = 0;
    double x = this->x;
    double char_width = CHARACTER_WIDTHS[scriptDepth()];
    for(const SemanticTag& tag : tags){
        if(tag.index >= stop) break;
        std::string_view substr(&str[start], tag.index-start);
        painter.drawText(x, y, substr, forward);
        x += char_width * countGraphemes(substr);
        start = tag.index;
        painter.setType(tag.type);
    }
    painter.drawText(x, y, std::string_view(&str[start], stop-start), forward);
}

void Text::paintAfter(Painter& painter, size_t start, bool forward) const {
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));
    double char_width = CHARACTER_WIDTHS[scriptDepth()];

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            std::string_view substr(&str[start], tag.index-start);
            painter.drawText(x, y, substr, forward);
            x += char_width * countGraphemes(substr);
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    painter.drawText(x, y, std::string_view(&str[start], numChars()-start), forward);
}

void Text::paintMid(Painter& painter, size_t start, size_t stop, bool forward) const {
    painter.setScriptLevel(scriptDepth());
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));
    double char_width = CHARACTER_WIDTHS[scriptDepth()];

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            if(tag.index >= stop) break;
            std::string_view substr(&str[start], tag.index-start);
            painter.drawText(x, y, substr, forward);
            x += char_width * countGraphemes(substr);
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    painter.drawText(x, y, std::string_view(&str[start], stop-start), forward);
}

void Text::paintGrouping(Painter& painter, size_t start) const {
    assert(start < numChars());
    size_t stop = start + codepointSize(str[start]);

    painter.setScriptLevel(scriptDepth());
    double x = this->x + xLocal(start);
    painter.setType(getTypeLeftOf(start));
    double char_width = CHARACTER_WIDTHS[scriptDepth()];

    for(const SemanticTag& tag : tags){
        if(tag.index > start){
            if(tag.index >= stop) break;
            std::string_view substr(&str[start], tag.index-start);
            double width = char_width * countGraphemes(substr);
            painter.drawHighlightedGrouping(x, y, width, substr);
            x += width;
            start = tag.index;
            painter.setType(tag.type);
        }
    }

    std::string_view substr(&str[start], stop-start);
    double width = char_width * countGraphemes(substr);
    painter.drawHighlightedGrouping(x, y, width, substr);
}

bool Text::containsX(double x_test) const noexcept {
    return (x_test >= x) & (x_test <= x + getWidth());
}

bool Text::containsY(double y_test) const noexcept {
    return (y_test >= y) & (y_test <= y + height());
}

bool Text::containsXInBounds(double x_test, size_t start, size_t stop) const noexcept {
    return x_test >= xGlobal(start) && x_test <= xGlobal(stop);
}

void Text::resize() noexcept {
    width = STALE;
    parent->resize();
}
#endif

}

}
