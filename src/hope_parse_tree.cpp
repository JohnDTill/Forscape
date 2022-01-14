#include "hope_parse_tree.h"

#include <code_parsenode_ops.h>
#include "typeset_selection.h"

#ifndef NDEBUG
#include <code_parsenodegraphviz.h>
#include <iostream>
#endif

namespace Hope {

namespace Code {

void ParseTree::clear() noexcept {
    std::vector<size_t>::clear();
}

bool ParseTree::empty() const noexcept{
    return std::vector<size_t>::empty();
}

const Typeset::Marker& ParseTree::getLeft(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index+LEFT_MARKER_OFFSET);
}

void ParseTree::setLeft(ParseNode index, const Typeset::Marker& m) noexcept{
    *reinterpret_cast<Typeset::Marker*>(data()+index+LEFT_MARKER_OFFSET) = m;
}

const Typeset::Marker& ParseTree::getRight(ParseNode index) const noexcept{
    return *reinterpret_cast<const Typeset::Marker*>(data()+index+RIGHT_MARKER_OFFSET);
}

void ParseTree::setRight(ParseNode index, const Typeset::Marker& m) noexcept{
    *reinterpret_cast<Typeset::Marker*>(data()+index+RIGHT_MARKER_OFFSET) = m;
}

ParseNode ParseTree::arg(ParseNode node, size_t index) const noexcept{
    assert(index < getNumArgs(node));
    return (*this)[node+FIXED_FIELDS+index];
}

void ParseTree::setArg(ParseNode node, size_t index, ParseNode val) noexcept{
    (*this)[node+FIXED_FIELDS+index] = val;
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
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 0);

    return pn;
}

size_t ParseTree::addUnary(size_t type, const Typeset::Selection& c, size_t child){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 1);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 1);
    setArg(pn, 0, child);

    return pn;
}

size_t ParseTree::addUnary(size_t type, size_t child){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 1);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, getSelection(child));
    setNumArgs(pn, 1);
    setArg(pn, 0, child);

    return pn;
}

size_t ParseTree::addLeftUnary(size_t type, const Typeset::Marker& left, size_t child){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 1);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, left);
    setRight(pn, getRight(child));
    setNumArgs(pn, 1);
    setArg(pn, 0, child);

    return pn;
}

size_t ParseTree::addRightUnary(size_t type, const Typeset::Marker& right, size_t child){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 1);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, getLeft(child));
    setRight(pn, right);
    setNumArgs(pn, 1);
    setArg(pn, 0, child);

    return pn;
}

size_t ParseTree::addBinary(size_t type, const Typeset::Selection& c, size_t lhs, size_t rhs){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 2);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 2);
    setArg(pn, 0, lhs);
    setArg(pn, 1, rhs);

    return pn;
}

size_t ParseTree::addBinary(size_t type, size_t lhs, size_t rhs){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 2);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, getLeft(lhs));
    setRight(pn, getRight(rhs));
    setNumArgs(pn, 2);
    setArg(pn, 0, lhs);
    setArg(pn, 1, rhs);

    return pn;
}

size_t ParseTree::addTernary(size_t type, const Typeset::Selection& c, size_t A, size_t B, size_t C){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 3);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 3);
    setArg(pn, 0, A);
    setArg(pn, 1, B);
    setArg(pn, 2, C);

    return pn;
}

ParseNode ParseTree::addTernary(Op type, ParseNode A, ParseNode B, ParseNode C){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 3);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(C));
    setNumArgs(pn, 3);
    setArg(pn, 0, A);
    setArg(pn, 1, B);
    setArg(pn, 2, C);

    return pn;
}

ParseNode ParseTree::addQuadary(Op type, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 4);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(D));
    setNumArgs(pn, 4);
    setArg(pn, 0, A);
    setArg(pn, 1, B);
    setArg(pn, 2, C);
    setArg(pn, 3, D);

    return pn;
}

ParseNode ParseTree::addQuadary(Op type, const Typeset::Selection &c, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 4);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 4);
    setArg(pn, 0, A);
    setArg(pn, 1, B);
    setArg(pn, 2, C);
    setArg(pn, 3, D);

    return pn;
}

ParseNode ParseTree::addPentary(Op type, ParseNode A, ParseNode B, ParseNode C, ParseNode D, ParseNode E){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 5);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setLeft(pn, getLeft(A));
    setRight(pn, getRight(E));
    setNumArgs(pn, 5);
    setArg(pn, 0, A);
    setArg(pn, 1, B);
    setArg(pn, 2, C);
    setArg(pn, 3, D);
    setArg(pn, 4, E);

    return pn;
}

ParseTree::NaryBuilder ParseTree::naryBuilder(size_t type){
    return NaryBuilder(*this, type);
}

#ifndef NDEBUG
std::string ParseTree::toGraphviz() const{
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

    ParseNode pn = tree.size();

    tree.resize(tree.size() + FIXED_FIELDS);
    tree.setOp(pn, type);
    tree.setFlag(pn, EMPTY);
    tree.setLeft(pn, tree.getLeft(children.front()));
    tree.setRight(pn, tree.getRight(children.back()));
    tree.setNumArgs(pn, children.size());
    tree.insert(tree.end(), children.begin(), children.end());

    return pn;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Marker& right){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    ParseNode pn = tree.size();

    tree.resize(tree.size() + FIXED_FIELDS);
    tree.setOp(pn, type);
    tree.setFlag(pn, EMPTY);
    tree.setLeft(pn, tree.getLeft(children.front()));
    tree.setRight(pn, right);
    tree.setNumArgs(pn, children.size());
    tree.insert(tree.end(), children.begin(), children.end());

    return pn;
}

size_t ParseTree::NaryBuilder::finalize(const Typeset::Selection& c){
    #ifndef NDEBUG
    assert(!built);
    built = true;
    #endif

    ParseNode pn = tree.size();

    tree.resize(tree.size() + FIXED_FIELDS);
    tree.setOp(pn, type);
    tree.setFlag(pn, EMPTY);
    tree.setSelection(pn, c);
    tree.setNumArgs(pn, children.size());
    tree.insert(tree.end(), children.begin(), children.end());

    return pn;
}

}

}
