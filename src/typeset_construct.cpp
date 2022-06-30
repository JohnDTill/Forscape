#include "typeset_construct.h"

#include "typeset_selection.h"
#include "typeset_subphrase.h"
#include <algorithm>
#include <cassert>

namespace Hope {

namespace Typeset {

#ifdef TYPESET_MEMORY_DEBUG
HOPE_UNORDERED_SET<Construct*> Construct::all;

Construct::Construct(){
    all.insert(this);
}
#endif

Construct::~Construct(){
    for(Subphrase* s : args) delete s;

    #ifdef TYPESET_MEMORY_DEBUG
    all.erase(this);
    #endif
}

void Construct::setParent(Phrase* p) noexcept {
    parent = p;
}

Subphrase* Construct::front() const noexcept {
    return args.empty() ? nullptr : args[0];
}

Subphrase* Construct::back() const noexcept{
    return args.empty() ? nullptr : args.back();
}

Text* Construct::frontText() const noexcept{
    return args.empty() ? nullptr : args[0]->front();
}

Text* Construct::frontTextAsserted() const noexcept{
    assert(!args.empty());
    return args[0]->front();
}

Text* Construct::next() const noexcept{
    return parent->nextTextInPhrase(this);
}

Text* Construct::prev() const noexcept{
    return parent->prevTextInPhrase(this);
}

Text* Construct::textEnteringFromLeft() const noexcept{
    return args.empty() ? next() : args[0]->front();
}

Text* Construct::textEnteringFromRight() const noexcept{
    return args.empty() ? prev() : args.back()->front();
}

Text* Construct::textRightOfSubphrase(const Subphrase* caller) const noexcept {
    if(caller == args.back()) return next();
    else return args[caller->id+1]->front();
}

Text* Construct::textLeftOfSubphrase(const Subphrase* caller) const noexcept{
    if(caller->id) return args[caller->id-1]->back();
    else return prev();
}

Text* Construct::textUp(const Subphrase*, double) const noexcept{
    return prev();
}

Text* Construct::textDown(const Subphrase*, double) const noexcept{
    return next();
}

size_t Construct::serialChars() const noexcept{
    size_t sze = 2 + dims() + args.size();
    for(Subphrase* subphrase : args) sze += subphrase->serialChars();
    return sze;
}

size_t Construct::dims() const noexcept {
    return 0;
}

void Construct::writeString(std::string& out, size_t& curr) const noexcept {
    out[curr++] = OPEN;
    out[curr++] = constructCode();
    writeArgs(out, curr);
    for(Subphrase* subphrase : args){
        subphrase->writeString(out, curr);
        out[curr++] = CLOSE;
    }
}

void Construct::writeArgs(std::string&, size_t&) const noexcept {
    assert(dims()==0); //Do nothing assuming fixed arity. Method should be overridden if n-ary.
}

std::string Construct::toString() const{
    std::string str;
    str.resize(serialChars());
    size_t curr = 0;
    writeString(str, curr);

    return str;
}

#ifdef HOPE_SEMANTIC_DEBUGGING
std::string Construct::toStringWithSemanticTags() const{
    std::string out;
    out.resize(2 + dims());
    out[0] = OPEN;
    out[1] = constructCode();
    size_t curr = 2;
    writeArgs(out, curr);
    for(Subphrase* subphrase : args){
        out += subphrase->toStringWithSemanticTags();
        out += CLOSE;
    }

    return out;
}
#endif

#ifndef HOPE_TYPESET_HEADLESS
ParseNode Construct::parseNodeAt(double x, double y) const noexcept {
    if(Subphrase* s = argAt(x, y)){
        ParseNode pn_from_child = s->parseNodeAt(x, y);
        if(pn_from_child != NONE) return pn_from_child;
    }

    return pn;
}

bool Construct::contains(double x, double y) const noexcept{
    return (x >= this->x) & (x <= this->x + width) & (y >= this->y) & (y <= this->y + height());
}

Construct* Construct::constructAt(double x, double y) noexcept{
    if(Subphrase* s = argAt(x, y))
        if(Construct* c = s->constructAt(x, y))
            return c;

    return this;
}

Subphrase* Construct::argAt(double x, double y) const noexcept{
    for(Subphrase* s : args) if(s->contains(x,y)) return s;
    return nullptr;
}

uint8_t Construct::scriptDepth() const noexcept{
    return parent->script_level;
}

bool Construct::increasesScriptDepth(uint8_t) const noexcept{
    return false;
}

void Construct::updateSize() noexcept{
    for(Subphrase* s : args){
        s->script_level = parent->script_level + (increasesScriptDepth(static_cast<uint8_t>(s->id)) & (parent->script_level < 2));
        s->updateSize();
    }
    updateSizeFromChildSizes();
}

void Construct::updateLayout() noexcept{
    updateChildPositions();
    for(Subphrase* s : args)
        s->updateLayout();
}

void Construct::paint(Painter& painter) const{
    paintSpecific(painter);
    for(Subphrase* arg : args) arg->paint(painter);
}

double Construct::height() const noexcept{
    return above_center + under_center;
}

#ifndef NDEBUG
void Construct::invalidateWidth() noexcept{
    width = STALE;
    parent->invalidateWidth();
}

void Construct::invalidateDims() noexcept{
    width = above_center = under_center = STALE;
    parent->invalidateDims();
}
#endif

Construct::ContextAction::ContextAction(const std::string& name, void (*takeAction)(Construct*, Controller&, Subphrase*))
    : takeAction(takeAction), name(name) {}

const std::vector<Construct::ContextAction> Construct::no_actions {};

const std::vector<Construct::ContextAction>& Construct::getContextActions(Subphrase*) const noexcept {
    return no_actions;
}

void Construct::updateChildPositions() noexcept {
    // Default does nothing
    // Must override for constructs with children
    assert(numArgs() == 0);
}

void Construct::paintSpecific(Painter&) const {
    // Default does nothing
}

void Construct::invalidateX() noexcept{
    x = std::numeric_limits<double>::quiet_NaN();
}

void Construct::invalidateY() noexcept{
    y = std::numeric_limits<double>::quiet_NaN();
}

void Construct::invalidatePos() noexcept{
    invalidateX();
    invalidateY();
}
#endif

void Construct::setupUnaryArg(){
    args.push_back(new Subphrase);
    args[0]->setParent(this);
    args[0]->id = 0;
}

void Construct::setupBinaryArgs(){
    args.push_back(new Subphrase);
    args[0]->setParent(this);
    args[0]->id = 0;
    args.push_back(new Subphrase);
    args[1]->setParent(this);
    args[1]->id = 1;
}

void Construct::setupNAargs(uint16_t n){
    for(uint16_t i = 0; i < n; i++){
        args.push_back(new Subphrase);
        args[i]->setParent(this);
        args[i]->id = i;
    }
}

size_t Construct::numArgs() const noexcept{
    return args.size();
}

bool Construct::sameContent(const Construct* other) const noexcept{
    if((constructCode() != other->constructCode()) || (numArgs() != other->numArgs()))
        return false;
    for(size_t i = 0; i < numArgs(); i++)
        if(!arg(i)->sameContent(other->arg(i)))
            return false;
    return true;
}

void Construct::search(const std::string& target, std::vector<Selection>& hits, bool use_case, bool word) const{
    for(Subphrase* arg : args) arg->search(target, hits, use_case, word);
}

Subphrase* Construct::child() const noexcept{
    assert(args.size() == 1);
    return args[0];
}

Subphrase* Construct::first() const noexcept{
    assert(args.size() == 2);
    return args[0];
}

Subphrase* Construct::second() const noexcept{
    assert(args.size() == 2);
    return args[1];
}

bool Construct::hasSmallChild() const noexcept{
    assert(args.size() == 1);

    Text* t = child()->front();
    if( child()->hasConstructs() ){
        return false;
    }else if(t->numChars() == 1){
        switch (t->charAt(0)) {
            case 'a':
            case 'c':
            case 'e':
            case 'g':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
                return true;

            default: return false;
        }
    }else if(t->numChars() == 2){
        uint16_t first = static_cast<uint8_t>(t->charAt(0));
        uint16_t second = static_cast<uint8_t>(t->charAt(1));
        switch (first | (second << 8)) {
            case 45518:
            case 46030:
            case 46542:
            case 47054:
            case 47566:
            case 48334:
            case 48590:
            case 49102:
            case 32975:
            case 33231:
            case 33743:
            case 33999:
            case 34255:
            case 34767:
            case 35023:
            case 35279:
                return true;

            default: return false;
        }
    }else{
        return false;
    }
}

Subphrase* Construct::arg(size_t index) const noexcept{
    return args[index];
}

size_t Construct::getSubphraseIndex(const Subphrase* s) const noexcept{
    auto in_vec = std::find(args.begin(), args.end(), s);
    assert(in_vec != args.end());
    return in_vec - args.begin();
}

}

}
