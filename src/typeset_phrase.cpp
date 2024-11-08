#include "typeset_phrase.h"

#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"
#include <algorithm>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Typeset {

Phrase::Phrase(){
    texts.push_back(new Text);
    texts[0]->setParent(this);
    texts[0]->id = 0;
}

Phrase::~Phrase(){
    for(Text* t : texts) delete t;
    for(Construct* c : constructs) delete c;
}

void Phrase::appendConstruct(Construct* c){
    c->setParent(this);
    c->id = constructs.size();
    constructs.push_back(c);
    Text* t = new Text;
    t->setParent(this);
    t->id = texts.size();
    texts.push_back(t);
}

void Phrase::appendConstruct(Construct* c, Text* t){
    c->setParent(this);
    c->id = constructs.size();
    constructs.push_back(c);
    t->setParent(this);
    t->id = texts.size();
    texts.push_back(t);
}

size_t Phrase::serialChars() const noexcept {
    size_t sze = 0;
    for(Text* t : texts) sze += t->numChars();
    for(Construct* c : constructs) sze += c->serialChars();

    return sze;
}

void Phrase::writeString(std::string& out) const noexcept {
    for(size_t i = 0; i < constructs.size(); i++){
        texts[i]->writeString(out);
        constructs[i]->writeString(out);
    }
    texts.back()->writeString(out);
}

std::string Phrase::toString() const {
    std::string str;
    str.reserve(serialChars());
    writeString(str);
    return str;
}

bool Phrase::writeUnicode(std::string& out, int8_t script) const noexcept {
    for(size_t i = 0; i < constructs.size(); i++){
        if(!texts[i]->writeUnicode(out, script)) return false;
        if(!constructs[i]->writeUnicode(out, script)) return false;
    }
    return texts.back()->writeUnicode(out, script);
}

Line* Phrase::asLine() noexcept {
    assert(isLine());
    return static_cast<Line*>(this);
}

Subphrase* Phrase::asSubphrase() noexcept {
    assert(!isLine());
    return static_cast<Subphrase*>(this);
}

Text* Phrase::text(size_t index) const noexcept {
    assert(index < texts.size());
    assert(texts[index]->id == index);
    return texts[index];
}

Construct* Phrase::construct(size_t index) const noexcept {
    assert(index < constructs.size());
    assert(constructs[index]->id == index);
    return constructs[index];
}

Text* Phrase::front() const noexcept {
    return text(0);
}

Text* Phrase::back() const noexcept {
    return texts.back();
}

Text* Phrase::nextTextInPhrase(const Construct* c) const noexcept {
    return text(c->id+1);
}

Text* Phrase::nextTextInPhrase(const Text* t) const noexcept {
    return t->id < constructs.size() ? text(t->id+1) : nullptr;
}

Text* Phrase::prevTextInPhrase(const Construct* c) const noexcept {
    return text(c->id);
}

Text* Phrase::prevTextInPhrase(const Text* t) const noexcept {
    return t->id ? text(t->id-1) : nullptr;
}

Construct* Phrase::nextConstructInPhrase(const Text *t) const noexcept {
    return t->id < constructs.size() ? construct(t->id) : nullptr;
}

Construct* Phrase::prevConstructInPhrase(const Text* t) const noexcept {
    return t->id ? constructs[t->id-1] : nullptr;
}

Text* Phrase::nextTextAsserted(const Text* t) const noexcept {
    assert(t->id+1 < texts.size());
    return text(t->id+1);
}

Text* Phrase::prevTextAsserted(const Text* t) const noexcept {
    assert(t->id);
    assert(text(t->id) == t);
    return text(t->id-1);
}

Construct* Phrase::nextConstructAsserted(const Text* t) const noexcept {
    assert(t->id < constructs.size());
    return construct(t->id);
}

Construct* Phrase::prevConstructAsserted(const Text* t) const noexcept {
    assert(t->id);
    return construct(t->id-1);
}

size_t Phrase::numTexts() const noexcept {
    return texts.size();
}

size_t Phrase::numConstructs() const noexcept {
    return constructs.size();
}

void Phrase::remove(size_t start) noexcept {
    remove(start, constructs.size());
}

void Phrase::remove(size_t start, size_t stop) noexcept {
    constructs.erase(constructs.begin()+start, constructs.begin()+stop);
    texts.erase(texts.begin()+start+1, texts.begin()+stop+1);
    for(size_t i = start; i < constructs.size(); i++){
        constructs[i]->id = i;
        texts[i+1]->id = i+1;
    }
}

void Phrase::insert(const std::vector<Construct*>& c, const std::vector<Text*>& t){
    insert(c.front()->id, c, t);
}

void Phrase::insert(size_t index, const std::vector<Construct*>& c, const std::vector<Text*>& t){
    for(size_t i = index; i < constructs.size(); i++){
        constructs[i]->id += c.size();
        texts[i+1]->id += c.size();
    }
    constructs.insert(constructs.begin()+index, c.begin(), c.end());
    texts.insert(texts.begin()+index+1, t.begin(), t.end());

    for(size_t i = 0; i < texts.size(); i++){
        texts[i]->id = i;
        texts[i]->setParent(this);
    }
    for(size_t i = 0; i < constructs.size(); i++){
        constructs[i]->id = i;
        constructs[i]->parent = this;
    }
}

void Phrase::giveUpOwnership() noexcept {
    texts.clear();
    constructs.clear();
}

bool Phrase::sameContent(const Phrase* other) const noexcept {
    if(texts.size() != other->texts.size()) return false;
    for(size_t i = 0; i < texts.size(); i++)
        if(text(i)->getString() != other->text(i)->getString())
            return false;
    for(size_t i = 0; i < constructs.size(); i++)
        if(!construct(i)->sameContent(other->construct(i)))
            return false;
    return true;
}

void Phrase::search(const std::string& target, std::vector<Selection>& hits, bool use_case, bool word) const {
    for(size_t i = 0; i < constructs.size(); i++){
        texts[i]->search(target, hits, use_case, word);
        constructs[i]->search(target, hits, use_case, word);
    }
    back()->search(target, hits, use_case, word);
}

bool Phrase::hasConstructs() const noexcept {
    return !constructs.empty();
}

size_t Phrase::nestingDepth() const noexcept {
    const Phrase* p = this;
    size_t nesting_depth = 0;
    while(!p->isLine()){
        nesting_depth++;
        p = static_cast<const Subphrase*>(p)->parent->parent;
    }
    return nesting_depth;
}

bool Phrase::empty() const noexcept {
    return texts.size() == 1 && front()->empty();
}

#ifdef FORSCAPE_SEMANTIC_DEBUGGING
std::string Phrase::toStringWithSemanticTags() const {
    std::string out = texts[0]->toSerialWithSemanticTags();
    for(size_t i = 0; i < constructs.size(); i++){
        out += constructs[i]->toStringWithSemanticTags();
        out += texts[i+1]->toSerialWithSemanticTags();
    }

    return out;
}
#endif

#ifndef FORSCAPE_TYPESET_HEADLESS
Text* Phrase::textLeftOf(double x) const noexcept {
    auto search = std::lower_bound(
                texts.rbegin(),
                texts.rend(),
                x,
                [](Text* t, double x){return t->x > x;}
            );

    return search != texts.rend() ? *search : texts.front();
}

Text* Phrase::textRightOf(double x) const noexcept {
    Text* t = textLeftOf(x);
    if(t->xRight() > x || t == back()) return t;
    else return texts[t->id+1];
}

Construct* Phrase::constructAt(double x, double y) const noexcept {
    auto search = std::lower_bound(
                constructs.rbegin(),
                constructs.rend(),
                x,
                [](Construct* c, double x){return c->x > x;}
            );

    if(search == constructs.rend() || !(*search)->contains(x,y)) return nullptr;
    else return *search;
}

void Phrase::updateSize() noexcept {
    text(0)->updateWidth();
    width = texts[0]->getWidth();
    above_center = texts[0]->aboveCenter();
    under_center = texts[0]->underCenter();

    for(size_t i = 0; i < constructs.size();){
        construct(i)->updateSize();
        width += constructs[i]->width;
        above_center = std::max(above_center, constructs[i]->above_center);
        under_center = std::max(under_center, constructs[i]->under_center);
        i++;
        text(i)->updateWidth();
        width += texts[i]->getWidth();
    }

    if(width == 0 && !isLine()) width = EMPTY_PHRASE_WIDTH_RATIO*height();
}

void Phrase::updateLayout() noexcept {
    double x_child = x;
    double y_child = y;

    Text* t = texts[0];
    t->x = x_child;
    t->y = y_child + above_center - t->aboveCenter();

    for(size_t i = 0; i < constructs.size(); i++){
        x_child += t->getWidth();
        Construct* c = constructs[i];
        c->x = x_child;
        c->y = y_child + above_center - c->above_center;
        c->updateLayout();
        x_child += c->width;
        t = texts[i+1];
        t->x = x_child;
        t->y = y_child + above_center - t->aboveCenter();
    }
}

void Phrase::paint(Painter& painter) const {
    #ifdef FORSCAPE_TYPESET_LAYOUT_DEBUG
    painter.drawDebugPhrase(x, y, width, above_center, under_center);
    #endif

    painter.setScriptLevel(script_level);
    texts.front()->paint(painter);
    for(size_t i = 0; i < constructs.size(); i++){
        constructs[i]->paint(painter);
        painter.setScriptLevel(script_level);
        texts[i+1]->paint(painter);
    }
}

void Phrase::paintUntil(Painter& painter, Text* t_end, size_t index) const {
    assert(t_end->id < texts.size() && text(t_end->id) == t_end);

    for(size_t i = 0; i < t_end->id; i++){
        painter.setScriptLevel(script_level);
        texts[i]->paint(painter);
        constructs[i]->paint(painter);
    }

    painter.setScriptLevel(script_level);
    t_end->paintUntil(painter, index);
}

void Phrase::paintAfter(Painter& painter, Text* t_start, size_t index) const {
    assert(t_start->id < texts.size() && text(t_start->id) == t_start);

    painter.setScriptLevel(script_level);
    t_start->paintAfter(painter, index);

    for(size_t i = t_start->id; i < constructs.size(); i++){
        constructs[i]->paint(painter);
        painter.setScriptLevel(script_level);
        texts[i+1]->paint(painter);
    }
}

void Phrase::paintMid(Painter& painter, Text* tL, size_t iL, Text* tR, size_t iR) const {
    assert(tL->getParent() == tR->getParent() && tL->id < tR->id);

    painter.setScriptLevel(script_level);
    tL->paintAfter(painter, iL);
    constructs[tL->id]->paint(painter);

    for(size_t i = tL->id+1; i < tR->id; i++){
        painter.setScriptLevel(script_level);
        texts[i]->paint(painter);
        constructs[i]->paint(painter);
    }

    painter.setScriptLevel(script_level);
    tR->paintUntil(painter, iR);
}

void Phrase::paint(Painter& painter, double xL, double xR) const {
    Text* tL = textLeftOf(xL);
    Text* tR = textRightOf(xR);

    Typeset::Marker r_mark(tR, tR->charIndexLeft(xR));
    if(!r_mark.atTextEnd()) r_mark.incrementGrapheme();

    if(tL == tR){
        painter.setScriptLevel(script_level);
        tL->paintMid(painter, tL->charIndexLeft(xL), r_mark.index);
    }else{
        paintMid(painter, tL, tL->charIndexLeft(xL), tR, r_mark.index);
    }
}

bool Phrase::contains(double x_test, double y_test) const noexcept {
    return (x_test >= x) & (x_test <= x+width) & containsY(y_test);
}

bool Phrase::containsY(double y_test) const noexcept {
    return (y_test >= y) & (y_test <= y+height());
}

Text* Phrase::textNearest(double x, double y) const {
    if(x <= this->x){
        return texts.front();
    }else if(x >= this->x + width){
        return texts.back();
    }else{
        Text* t = textLeftOf(x);

        if(t->containsX(x)){
            return t;
        }else if(Construct* c = t->nextConstructInPhrase()){
            if(Subphrase* s = c->argAt(x, y)) return s->textNearest(x, y);
            else return x > c->x + c->width/2 ? c->next() : t;
        }else{
            return t;
        }
    }
}

ParseNode Phrase::parseNodeAt(double x, double y) const noexcept {
    if(x < this->x || x > this->x + width || !containsY(y)) return NONE;

    Text* t = textLeftOf(x);

    if(t->containsX(x)){
        if(t->containsY(y)) return t->parseNodeAtX(x);
        else return NONE;
    }else if(Construct* c = t->nextConstructInPhrase()){
        if(c->contains(x, y)) return c->parseNodeAt(x, y);
        else return NONE;
    }else{
        return NONE;
    }
}

#ifndef NDEBUG
void Phrase::populateDocMapParseNodes(std::unordered_set<ParseNode>& nodes) const noexcept {
    for(size_t i = 0; i < constructs.size(); i++){
        text(i)->populateDocMapParseNodes(nodes);
        construct(i)->populateDocMapParseNodes(nodes);
    }
    back()->populateDocMapParseNodes(nodes);
}
#endif
#endif

}

}
