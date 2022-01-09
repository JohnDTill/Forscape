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

namespace Hope {

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

size_t Phrase::serialChars() const noexcept{
    size_t sze = 0;
    for(Text* t : texts) sze += t->size();
    for(Construct* c : constructs) sze += c->serialChars();

    return sze;
}

void Phrase::writeString(std::string& out, size_t& curr) const noexcept{
    for(size_t i = 0; i < constructs.size(); i++){
        texts[i]->writeString(out, curr);
        constructs[i]->writeString(out, curr);
    }
    texts.back()->writeString(out, curr);
}

std::string Phrase::toString() const{
    std::string str;
    str.resize(serialChars());
    size_t curr = 0;
    writeString(str, curr);
    return str;
}

Line* Phrase::asLine() noexcept{
    assert(isLine());
    return static_cast<Line*>(this);
}

Subphrase* Phrase::asSubphrase() noexcept{
    assert(!isLine());
    return static_cast<Subphrase*>(this);
}

Text* Phrase::text(size_t index) const noexcept{
    assert(index < texts.size());
    return texts[index];
}

Construct* Phrase::construct(size_t index) const noexcept{
    assert(index < constructs.size());
    return constructs[index];
}

Text* Phrase::front() const noexcept{
    return text(0);
}

Text* Phrase::back() const noexcept{
    return texts.back();
}

Text* Phrase::nextTextInPhrase(const Construct* c) const noexcept{
    return text(c->id+1);
}

Text* Phrase::nextTextInPhrase(const Text* t) const noexcept{
    return t->id < constructs.size() ? text(t->id+1) : nullptr;
}

Text* Phrase::prevTextInPhrase(const Construct* c) const noexcept{
    return text(c->id);
}

Text* Phrase::prevTextInPhrase(const Text* t) const noexcept{
    return t->id ? text(t->id-1) : nullptr;
}

Construct* Phrase::nextConstructInPhrase(const Text *t) const noexcept{
    return t->id < constructs.size() ? construct(t->id) : nullptr;
}

Construct* Phrase::prevConstructInPhrase(const Text* t) const noexcept{
    return t->id ? constructs[t->id-1] : nullptr;
}

Text* Phrase::nextTextAsserted(const Text* t) const noexcept{
    assert(t->id+1 < texts.size());
    return text(t->id+1);
}

Text* Phrase::prevTextAsserted(const Text* t) const noexcept{
    assert(t->id);
    return text(t->id-1);
}

Construct* Phrase::nextConstructAsserted(const Text* t) const noexcept{
    assert(t->id < constructs.size());
    return construct(t->id);
}

Construct* Phrase::prevConstructAsserted(const Text* t) const noexcept{
    assert(t->id);
    return construct(t->id-1);
}

size_t Phrase::numTexts() const noexcept{
    return texts.size();
}

size_t Phrase::numConstructs() const noexcept{
    return constructs.size();
}

void Phrase::remove(size_t start) noexcept{
    remove(start, constructs.size());
}

void Phrase::remove(size_t start, size_t stop) noexcept{
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

void Phrase::giveUpOwnership() noexcept{
    texts.clear();
    constructs.clear();
}

bool Phrase::sameContent(const Phrase* other) const noexcept{
    if(texts.size() != other->texts.size()) return false;
    for(size_t i = 0; i < texts.size(); i++)
        if(text(i)->str != other->text(i)->str)
            return false;
    for(size_t i = 0; i < constructs.size(); i++)
        if(!construct(i)->sameContent(other->construct(i)))
            return false;
    return true;
}

void Phrase::findCaseInsensitive(const std::string& target, std::vector<Selection>& hits) const{
    for(size_t i = 0; i < constructs.size(); i++){
        texts[i]->findCaseInsensitive(target, hits);
        constructs[i]->findCaseInsensitive(target, hits);
    }
    back()->findCaseInsensitive(target, hits);
}

bool Phrase::hasConstructs() const noexcept{
    return !constructs.empty();
}

#ifdef HOPE_SEMANTIC_DEBUGGING
std::string Phrase::toStringWithSemanticTags() const{
    std::string out = texts[0]->toSerialWithSemanticTags();
    for(size_t i = 0; i < constructs.size(); i++){
        out += constructs[i]->toStringWithSemanticTags();
        out += texts[i+1]->toSerialWithSemanticTags();
    }

    return out;
}
#endif

#ifndef HOPE_TYPESET_HEADLESS
Text* Phrase::textLeftOf(double x) const noexcept{
    auto search = std::lower_bound(
                texts.rbegin(),
                texts.rend(),
                x,
                [](Text* t, double x){return t->x > x;}
            );

    return search != texts.rend() ? *search : texts.front();
}

Construct* Phrase::constructAt(double x, double y) const noexcept{
    auto search = std::lower_bound(
                constructs.rbegin(),
                constructs.rend(),
                x,
                [](Construct* c, double x){return c->x > x;}
            );

    if(search == constructs.rend() || !(*search)->contains(x,y)) return nullptr;
    else return *search;
}

void Phrase::updateSize() noexcept{
    texts[0]->updateWidth();
    width = texts[0]->getWidth();
    above_center = texts[0]->aboveCenter();
    under_center = texts[0]->underCenter();

    for(size_t i = 0; i < constructs.size(); i++){
        constructs[i]->updateSize();
        width += constructs[i]->width;
        above_center = std::max(above_center, constructs[i]->above_center);
        under_center = std::max(under_center, constructs[i]->under_center);
        texts[i+1]->updateWidth();
        width += texts[i+1]->getWidth();
    }

    if(width == 0) width = EMPTY_PHRASE_WIDTH_RATIO*height();
}

void Phrase::updateLayout() noexcept{
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

void Phrase::paint(Painter& painter, bool forward) const{
    #ifdef HOPE_TYPESET_LAYOUT_DEBUG
    painter.drawDebugPhrase(x, y, width, above_center, under_center);
    #endif

    painter.setScriptLevel(script_level);
    texts.front()->paint(painter, forward);
    for(size_t i = 0; i < constructs.size(); i++){
        constructs[i]->paint(painter);
        painter.setScriptLevel(script_level);
        texts[i+1]->paint(painter, forward);
    }
}

void Phrase::paintUntil(Painter& painter, Text* t_end, size_t index, bool forward) const{
    assert(t_end->id < texts.size() && text(t_end->id) == t_end);

    for(size_t i = 0; i < t_end->id; i++){
        painter.setScriptLevel(script_level);
        texts[i]->paint(painter, forward);
        constructs[i]->paint(painter);
    }

    painter.setScriptLevel(script_level);
    t_end->paintUntil(painter, index, forward);
}

void Phrase::paintAfter(Painter& painter, Text* t_start, size_t index, bool forward) const{
    assert(t_start->id < texts.size() && text(t_start->id) == t_start);

    painter.setScriptLevel(script_level);
    t_start->paintAfter(painter, index, forward);

    for(size_t i = t_start->id; i < constructs.size(); i++){
        constructs[i]->paint(painter);
        painter.setScriptLevel(script_level);
        texts[i+1]->paint(painter, forward);
    }
}

void Phrase::paintMid(Painter& painter, Text* tL, size_t iL, Text* tR, size_t iR, bool forward) const{
    painter.setScriptLevel(script_level);
    tL->paintAfter(painter, iL, forward);
    constructs[tL->id]->paint(painter);

    for(size_t i = tL->id+1; i < tR->id; i++){
        painter.setScriptLevel(script_level);
        texts[i]->paint(painter, forward);
        constructs[i]->paint(painter);
    }

    painter.setScriptLevel(script_level);
    tR->paintUntil(painter, iR, forward);
}

bool Phrase::contains(double x_test, double y_test) const noexcept{
    return (x_test >= x) & (x_test <= x+width) & containsY(y_test);
}

bool Phrase::containsY(double y_test) const noexcept{
    return (y_test >= y) & (y_test <= y+height());
}

Text* Phrase::textNearest(double x, double y) const{
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
#endif

}

}
