#include "hope_parse_tree.h"

#include <code_parsenode_ops.h>
#include <hope_static_pass.h>
#include "typeset_selection.h"

#ifndef NDEBUG
#include <code_parsenodegraphviz.h>
#include <iostream>
#endif

namespace Hope {

namespace Code {

static constexpr size_t UNKNOWN_SIZE = 0;

void ParseTree::clear() noexcept {
    std::vector<size_t>::clear();
    cloned_vars.clear();
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

template<size_t index>
ParseNode ParseTree::arg(ParseNode node) const noexcept{
    assert(index < getNumArgs(node));
    return (*this)[node+FIXED_FIELDS+index];
}

void ParseTree::setArg(ParseNode node, size_t index, ParseNode val) noexcept{
    (*this)[node+FIXED_FIELDS+index] = val;
}

double ParseTree::getFlagAsDouble(ParseNode pn) const noexcept{
    size_t flag = getFlag(pn);
    return *reinterpret_cast<double*>(&flag);
}

void ParseTree::setFlag(ParseNode pn, double val) noexcept{
    setFlag(pn, *reinterpret_cast<size_t*>(&val));
}

template<size_t index>
void ParseTree::setArg(ParseNode node, ParseNode val) noexcept{
    (*this)[node+FIXED_FIELDS+index] = val;
}

ParseNode ParseTree::lhs(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 2);
    return arg<0>(node);
}

ParseNode ParseTree::rhs(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 2);
    return arg<1>(node);
}

ParseNode ParseTree::child(ParseNode node) const noexcept{
    assert(getNumArgs(node) == 1);
    return arg<0>(node);
}

ParseNode ParseTree::valCapList(ParseNode node) const noexcept{
    assert(getOp(node) == OP_ALGORITHM || getOp(node) == OP_LAMBDA);
    return arg<0>(node);
}

ParseNode ParseTree::refCapList(ParseNode node) const noexcept{
    assert(getOp(node) == OP_ALGORITHM || getOp(node) == OP_LAMBDA);
    return arg<1>(node);
}

void ParseTree::setRefList(ParseNode fn, ParseNode list) noexcept{
    assert(getOp(fn) == OP_ALGORITHM || getOp(fn) == OP_LAMBDA);
    setArg<1>(fn, list);
}

ParseNode ParseTree::paramList(ParseNode node) const noexcept{
    assert(getOp(node) == OP_ALGORITHM || getOp(node) == OP_LAMBDA);
    return arg<2>(node);
}

ParseNode ParseTree::body(ParseNode node) const noexcept{
    assert(getOp(node) == OP_ALGORITHM || getOp(node) == OP_LAMBDA);
    return arg<3>(node);
}

void ParseTree::setBody(ParseNode node, ParseNode body) noexcept{
    assert(getOp(node) == OP_ALGORITHM || getOp(node) == OP_LAMBDA);
    setArg<3>(node, body);
}

ParseNode ParseTree::algName(ParseNode node) const noexcept{
    assert(getOp(node) == OP_ALGORITHM);
    return arg<4>(node);
}

size_t ParseTree::valListSize(ParseNode node) const noexcept{
    return node == EMPTY ? 0 : getNumArgs(node);
}

ParseNode ParseTree::unitVectorElem(ParseNode node) const noexcept{
    return arg<0>(node);
}

void ParseTree::setUnitVectorElem(ParseNode node, ParseNode val) noexcept{
    setArg<0>(node, val);
}

ParseNode ParseTree::unitVectorRows(ParseNode node) const noexcept{
    return arg<1>(node);
}

void ParseTree::setUnitVectorRows(ParseNode node, ParseNode val) noexcept{
    setArg<1>(node, val);
}

ParseNode ParseTree::unitVectorCols(ParseNode node) const noexcept{
    return arg<2>(node);
}

void ParseTree::setUnitVectorCols(ParseNode node, ParseNode val) noexcept{
    setArg<2>(node, val);
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
    setArg<0>(pn, child);

    return pn;
}

size_t ParseTree::addUnary(size_t type, size_t child){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 1);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, getSelection(child));
    setNumArgs(pn, 1);
    setArg<0>(pn, child);

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
    setArg<0>(pn, child);

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
    setArg<0>(pn, child);

    return pn;
}

size_t ParseTree::addBinary(size_t type, const Typeset::Selection& c, size_t lhs, size_t rhs){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 2);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 2);
    setArg<0>(pn, lhs);
    setArg<1>(pn, rhs);

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
    setArg<0>(pn, lhs);
    setArg<1>(pn, rhs);

    return pn;
}

size_t ParseTree::addTernary(size_t type, const Typeset::Selection& c, size_t A, size_t B, size_t C){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 3);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 3);
    setArg<0>(pn, A);
    setArg<1>(pn, B);
    setArg<2>(pn, C);

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
    setArg<0>(pn, A);
    setArg<1>(pn, B);
    setArg<2>(pn, C);

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
    setArg<0>(pn, A);
    setArg<1>(pn, B);
    setArg<2>(pn, C);
    setArg<3>(pn, D);

    return pn;
}

ParseNode ParseTree::addQuadary(Op type, const Typeset::Selection &c, ParseNode A, ParseNode B, ParseNode C, ParseNode D){
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS + 4);
    setOp(pn, type);
    setFlag(pn, EMPTY);
    setSelection(pn, c);
    setNumArgs(pn, 4);
    setArg<0>(pn, A);
    setArg<1>(pn, B);
    setArg<2>(pn, C);
    setArg<3>(pn, D);

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
    setArg<0>(pn, A);
    setArg<1>(pn, B);
    setArg<2>(pn, C);
    setArg<3>(pn, D);
    setArg<4>(pn, E);

    return pn;
}

ParseNode ParseTree::clone(ParseNode pn){
    ParseNode cloned = size();

    switch (getOp(pn)) {
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:
        case OP_ALGORITHM:
        case OP_PROTOTYPE_ALG:
            cloned_vars.push_back(std::make_pair(pn, cloned));
    }

    insert(end(), data()+pn, data()+pn+FIXED_FIELDS);
    size_t nargs = getNumArgs(pn);
    resize(size() + nargs);
    for(size_t i = 0; i < nargs; i++){
        ParseNode a = arg(pn, i);
        if(a == EMPTY){
            setArg(cloned, i, EMPTY);
        }else if(getOp(a) == OP_CALL){
            setArg(cloned, i, a);
        }else{
            setArg(cloned, i, clone(a));
        }
    }

    return cloned;
}

ParseNode ParseTree::getZero(const Typeset::Selection& sel){
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setFlag(pn, 0.0);
    setScalar(pn);

    return pn;
}

ParseNode ParseTree::getOne(const Typeset::Selection& sel){
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setFlag(pn, 1.0);
    setScalar(pn);

    return pn;
}

bool ParseTree::definitelyScalar(ParseNode pn) const noexcept{
    return getRows(pn) == 1 && getCols(pn) == 1;
}

bool ParseTree::definitelyNotScalar(ParseNode pn) const noexcept{
    return getRows(pn) > 1 || getCols(pn) > 1;
}

bool ParseTree::definitelyMatrix(ParseNode pn) const noexcept{
    return getRows(pn) > 1 && getCols(pn) > 1;
}

bool ParseTree::definitelyR3(ParseNode pn) const noexcept{
    return getRows(pn) == 3 && getCols(pn) == 1;
}

bool ParseTree::nonSquare(ParseNode pn) const noexcept{
    size_t rows = getRows(pn);
    size_t cols = getCols(pn);
    return rows != UNKNOWN_SIZE && cols != UNKNOWN_SIZE && rows != cols;
}

bool ParseTree::maybeR3(ParseNode pn) const noexcept{
    size_t rows = getRows(pn);
    size_t cols = getCols(pn);
    return (rows == 3 || rows == UNKNOWN_SIZE) && (cols == 1 || cols == UNKNOWN_SIZE);
}

void ParseTree::setScalar(ParseNode pn) noexcept{
    setType(pn, StaticPass::NUMERIC);
    setRows(pn, 1);
    setCols(pn, 1);
}

void ParseTree::setR3(ParseNode pn) noexcept{
    setRows(pn, 3);
    setCols(pn, 1);
}

void ParseTree::copyDims(ParseNode dest, ParseNode src) noexcept{
    setRows(dest, getRows(src));
    setCols(dest, getCols(src));
}

void ParseTree::transposeDims(ParseNode dest, ParseNode src) noexcept{
    setRows(dest, getCols(src));
    setCols(dest, getRows(src));
}

ParseTree::NaryBuilder ParseTree::naryBuilder(size_t type){
    return NaryBuilder(*this, type);
}

void ParseTree::patchClones() noexcept{
    for(const auto& entry : cloned_vars){
        ParseNode orig = entry.first;
        ParseNode clone = entry.second;

        setOp(clone, getOp(orig));
        setFlag(clone, getFlag(orig));
    }
}

void ParseTree::patchClonedTypes() noexcept{
    for(const auto& entry : cloned_vars){
        ParseNode orig = entry.first;
        ParseNode clone = entry.second;

        setType(orig, getType(clone));
        setRows(orig, getRows(clone));
        setCols(orig, getCols(clone));
    }
}

#ifndef NDEBUG
std::string ParseTree::toGraphviz() const{
    std::string src = "digraph {\n\trankdir=TB\n\n";
    size_t sze = 0;
    graphvizHelper(src, root, sze);
    src += "}\n";

    return src;
}

std::string ParseTree::toGraphviz(ParseNode pn) const{
    std::string src = "digraph {\n\trankdir=TB\n\n";
    size_t sze = 0;
    graphvizHelper(src, pn, sze);
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
