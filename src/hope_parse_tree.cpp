#include "hope_parse_tree.h"

#include <code_parsenodetype.h>
#include "typeset_selection.h"

#ifndef NDEBUG
#include <code_parsenodegraphviz.h>
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

const Typeset::Marker& ParseTree::getLeft(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index-LEFT_MARKER_OFFSET);
}

void ParseTree::setLeft(ParseNode index, const Typeset::Marker& m) noexcept{
    *reinterpret_cast<Typeset::Marker*>(data()+index-LEFT_MARKER_OFFSET) = m;
}

const Typeset::Marker& ParseTree::getRight(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index-RIGHT_MARKER_OFFSET);
}

void ParseTree::setRight(ParseNode index, const Typeset::Marker &m) noexcept{
    *reinterpret_cast<Typeset::Marker*>(data()+index-RIGHT_MARKER_OFFSET) = m;
}

ParseNode ParseTree::arg(ParseNode node, size_t index) const noexcept{
    assert(index < getNumArgs(node));
    return (*this)[node-FIXED_FIELDS-index];
}

void ParseTree::setArg(ParseNode node, size_t index, ParseNode val) noexcept{
    (*this)[node-FIXED_FIELDS-index] = val;
}

ParseNode ParseTree::lhs(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 2);
    return arg(node, 0);
}

ParseNode ParseTree::rhs(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 2);
    return arg(node, 1);
}

ParseNode ParseTree::child(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 1);
    return arg(node, 0);
}

std::string ParseTree::str(ParseNode node) const{
    return getSelection(node).str();
}

size_t ParseTree::addTerminal(size_t type, const Typeset::Selection& c){
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 0);
    setSelection(pn, c);
    setType(pn, type);

    return pn;
}

size_t ParseTree::addUnary(size_t type, const Typeset::Selection& c, size_t child){
    push_back(child);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 1);
    setSelection(pn, c);
    setType(pn, type);

    return pn;
}

size_t ParseTree::addUnary(size_t type, size_t child){
    push_back(child);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 1);
    setSelection(pn, getSelection(child));
    setType(pn, type);

    return pn;
}

size_t ParseTree::addLeftUnary(size_t type, const Typeset::Marker& left, size_t child){
    push_back(child);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 1);
    setLeft(pn, left);
    setRight(pn, getRight(child));
    setType(pn, type);

    return pn;
}

size_t ParseTree::addRightUnary(size_t type, const Typeset::Marker& right, size_t child){
    push_back(child);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 1);
    setLeft(pn, getLeft(child));
    setRight(pn, right);
    setType(pn, type);

    return pn;
}

size_t ParseTree::addBinary(size_t type, const Typeset::Selection& c, size_t lhs, size_t rhs){
    push_back(rhs);
    push_back(lhs);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 2);
    setSelection(pn, c);
    setType(pn, type);

    return pn;
}

size_t ParseTree::addBinary(size_t type, size_t lhs, size_t rhs){
    push_back(rhs);
    push_back(lhs);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 2);
    setLeft(pn, getLeft(lhs));
    setRight(pn, getRight(rhs));
    setType(pn, type);

    return pn;
}

size_t ParseTree::addTernary(size_t type, const Typeset::Selection& c, size_t A, size_t B, size_t C){
    push_back(C);
    push_back(B);
    push_back(A);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 3);
    setSelection(pn, c);
    setType(pn, type);

    return pn;
}

ParseNode ParseTree::addTernary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C){
    push_back(C);
    push_back(B);
    push_back(A);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 3);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(C));
    setType(pn, type);

    return pn;
}

ParseNode ParseTree::addQuadary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 4);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(D));
    setType(pn, type);

    return pn;
}

ParseNode ParseTree::addQuadary(ParseNodeType type, const Typeset::Selection &c, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 4);
    setSelection(pn, c);
    setType(pn, type);

    return pn;
}

ParseNode ParseTree::addPentary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D, ParseNode E){
    push_back(E);
    push_back(D);
    push_back(C);
    push_back(B);
    push_back(A);
    resize(size() + FIXED_FIELDS);
    ParseNode pn = size()-1;
    setNumArgs(pn, 5);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(E));
    setType(pn, type);

    return pn;
}

ParseTree::NaryBuilder ParseTree::naryBuilder(size_t type){
    return NaryBuilder(*this, type);
}

#ifndef NDEBUG
std::string ParseTree::toGraphviz(ParseNode root) const{
    std::string src = "digraph {\n\trankdir=TB\n\n";
    size_t sze = 0;
    graphvizHelper(src, root, sze);
    src += "}\n";

    return src;
}

void ParseTree::graphvizHelper(std::string& src, ParseNode n, size_t& size) const{
    std::string id = std::to_string(size++);
    src += "\tn" + id + " [label=";
    writeType(src, n);
    src += "]\n";
    for(size_t i = 0; i < getNumArgs(n); i++){
        size_t child = arg(n, i);
        if(child != EMPTY){
            std::string child_id = std::to_string(size);
            graphvizHelper(src, child, size);
            src += "\tn" + id + "->n" + child_id + "\n";
        }
    }
}
#endif

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
    tree.resize(tree.size() + FIXED_FIELDS);
    ParseNode pn = tree.size()-1;
    tree.setNumArgs(pn, children.size());
    tree.setLeft(pn, tree.getLeft(children.front()));
    tree.setRight(pn, tree.getRight(children.back()));
    tree.setType(pn, type);

    return pn;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Marker& right){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    tree.insert(tree.end(), children.rbegin(), children.rend());
    tree.resize(tree.size() + FIXED_FIELDS);
    ParseNode pn = tree.size()-1;
    tree.setNumArgs(pn, children.size());
    tree.setLeft(pn, tree.getLeft(children.front()));
    tree.setRight(pn, right);
    tree.setType(pn, type);

    return pn;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Selection& c){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    tree.insert(tree.end(), children.rbegin(), children.rend());
    tree.resize(tree.size() + FIXED_FIELDS);
    ParseNode pn = tree.size()-1;
    tree.setNumArgs(pn, children.size());
    tree.setSelection(pn, c);
    tree.setType(pn, type);

    return pn;
}

}

}
