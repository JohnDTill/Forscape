#include "hope_parse_tree.h"

#include <code_parsenodetype.h>
#include "typeset_selection.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

ParseNode ParseTree::back() const noexcept{
    return size()-1;
}

bool ParseTree::empty() const noexcept{
    return std::vector<size_t>::empty();
}

ParseNodeType ParseTree::getEnum(ParseNode node) const noexcept{
    return (*this)[node-ENUM_OFFSET];
}

void ParseTree::setEnum(ParseNode node, ParseNodeType type) noexcept{
    (*this)[node-ENUM_OFFSET] = type;
}

size_t ParseTree::numArgs(ParseNode node) const noexcept{
    return (*this)[node-SIZE_OFFSET];
}

void ParseTree::setStackOffset(ParseNode node, size_t val) noexcept{
    assert(getEnum(node) == PN_IDENTIFIER);
    (*this)[node-SIZE_OFFSET] = val;
}

size_t ParseTree::stackOffset(ParseNode node) const noexcept{
    return (*this)[node-SIZE_OFFSET];
}

void ParseTree::setUpvalue(ParseNode node, size_t offset) noexcept{
    setStackOffset(node, offset);
}

size_t ParseTree::upvalueOffset(ParseNode node) const noexcept{
    return (*this)[node-SIZE_OFFSET];
}

const Typeset::Selection& ParseTree::getSelection(size_t index) const noexcept{
    return *reinterpret_cast<const Typeset::Selection*>(data()+index-RIGHT_MARKER_OFFSET);
}

void ParseTree::setSelection(ParseNode index, const Typeset::Selection& sel) noexcept{
    setLeft(index, sel.left);
    setRight(index, sel.right);
}

const Typeset::Marker& ParseTree::getLeft(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index-LEFT_MARKER_OFFSET);
}

void ParseTree::setLeft(ParseNode index, const Typeset::Marker& m) noexcept{
    (*this)[index-RIGHT_MARKER_OFFSET] = reinterpret_cast<size_t>(m.text);
    (*this)[index-RIGHT_MARKER_OFFSET+1] = m.index;
}

const Typeset::Marker& ParseTree::getRight(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index-RIGHT_MARKER_OFFSET);
}

void ParseTree::setRight(ParseNode index, const Typeset::Marker &m) noexcept{
    (*this)[index-LEFT_MARKER_OFFSET] = reinterpret_cast<size_t>(m.text);
    (*this)[index-LEFT_MARKER_OFFSET+1] = m.index;
}

ParseNode ParseTree::arg(ParseNode node, size_t index) const noexcept{
    assert(index < numArgs(node));
    return (*this)[node-FIXED_FIELDS-index];
}

void ParseTree::setArg(ParseNode node, size_t index, ParseNode val) noexcept{
    (*this)[node-FIXED_FIELDS-index] = val;
}

ParseNode ParseTree::lhs(ParseNode node) const noexcept{
    assert(numArgs(node) == 2);
    return arg(node, 0);
}

ParseNode ParseTree::rhs(ParseNode node) const noexcept{
    assert(numArgs(node) == 2);
    return arg(node, 1);
}

ParseNode ParseTree::child(ParseNode node) const noexcept{
    assert(numArgs(node) == 1);
    return arg(node, 0);
}

std::string ParseTree::str(ParseNode node) const{
    return getSelection(node).str();
}

void ParseTree::setRightMarker(size_t index, const Typeset::Marker& m) noexcept{
    *reinterpret_cast<Typeset::Marker*>(data()+index-RIGHT_MARKER_OFFSET) = m;
}

void ParseTree::addIdentifier(const Typeset::Selection& c){
    push_back(PN_IDENTIFIER);
    pushSelection(c);
    push_back(UNITIALIZED);
}

size_t ParseTree::addTerminal(size_t type, const Typeset::Selection& c){
    pushSelection(c);
    push_back(0);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addUnary(size_t type, const Typeset::Selection& c, size_t child){
    push_back(child);
    pushSelection(c);
    push_back(1);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addUnary(size_t type, size_t child){
    push_back(child);
    pushSelection(child);
    push_back(1);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addLeftUnary(size_t type, const Typeset::Marker& left, size_t child){
    push_back(child);
    pushRightMarker(child);
    pushMarker(left);
    push_back(1);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addRightUnary(size_t type, const Typeset::Marker& right, size_t child){
    push_back(child);
    pushMarker(right);
    pushLeftMarker(child);
    push_back(1);
    push_back(type);


    return size()-1;
}

size_t ParseTree::addBinary(size_t type, const Typeset::Selection& c, size_t lhs, size_t rhs){
    push_back(rhs);
    push_back(lhs);
    pushSelection(c);
    push_back(2);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addBinary(size_t type, size_t lhs, size_t rhs){
    push_back(rhs);
    push_back(lhs);
    pushRightMarker(rhs);
    pushLeftMarker(lhs);
    push_back(2);
    push_back(type);

    return size()-1;
}

size_t ParseTree::addTernary(size_t type, const Typeset::Selection& c, size_t A, size_t B, size_t C){
    push_back(C);
    push_back(B);
    push_back(A);
    pushSelection(c);
    push_back(3);
    push_back(type);

    return size()-1;
}

ParseNode ParseTree::addTernary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C){
    push_back(C);
    push_back(B);
    push_back(A);
    pushRightMarker(C);
    pushLeftMarker(A);
    push_back(3);
    push_back(type);

    return size()-1;
}

ParseNode ParseTree::addQuadary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    pushRightMarker(D);
    pushLeftMarker(A);
    push_back(4);
    push_back(type);

    return size()-1;
}

ParseNode ParseTree::addQuadary(ParseNodeType type, const Typeset::Selection &c, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    pushSelection(c);
    push_back(4);
    push_back(type);

    return size()-1;
}

ParseNode ParseTree::addPentary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D, ParseNode E){
    push_back(E);
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    pushRightMarker(E);
    pushLeftMarker(A);
    push_back(5);
    push_back(type);

    return size()-1;
}

ParseTree::NaryBuilder ParseTree::naryBuilder(size_t type){
    return NaryBuilder(*this, type);
}

void ParseTree::pushSelection(const Typeset::Selection& c){
    pushMarker(c.right);
    pushMarker(c.left);
}

void ParseTree::pushMarker(const Typeset::Marker& m){
    push_back( reinterpret_cast<size_t>(m.text) );
    push_back( m.index );
}

void ParseTree::pushSelection(size_t src){
    insert(end(), begin()+src-RIGHT_MARKER_OFFSET, begin()+src-RIGHT_MARKER_OFFSET+4);
}

void ParseTree::pushLeftMarker(size_t src){
    insert(end(), begin()+src-LEFT_MARKER_OFFSET, begin()+src-LEFT_MARKER_OFFSET+2);
}

void ParseTree::pushRightMarker(size_t src){
    insert(end(), begin()+src-RIGHT_MARKER_OFFSET, begin()+src-RIGHT_MARKER_OFFSET+2);
}

ParseTree::NaryBuilder::NaryBuilder(ParseTree& tree, size_t type)
    : tree(tree), type(type) {}

void ParseTree::NaryBuilder::addNaryChild(size_t index){
    children.push_back(index);
}

size_t ParseTree::NaryBuilder::finalize(){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    tree.insert(tree.end(), children.rbegin(), children.rend());
    tree.pushLeftMarker( children.front() );
    tree.pushRightMarker( children.back() );
    tree.push_back( children.size() );
    tree.push_back(type);

    return tree.size()-1;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Marker& right){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    tree.insert(tree.end(), children.rbegin(), children.rend());
    tree.pushMarker( right );
    tree.pushLeftMarker( children.front() );
    tree.push_back( children.size() );
    tree.push_back(type);

    return tree.size()-1;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Selection& c){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    tree.insert(tree.end(), children.rbegin(), children.rend());
    tree.pushSelection(c);
    tree.push_back( children.size() );
    tree.push_back(type);

    return tree.size()-1;
}

}

}
