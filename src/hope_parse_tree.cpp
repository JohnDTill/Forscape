#include "hope_parse_tree.h"

#include <code_parsenode_ops.h>
#include <hope_common.h>
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
    string_lits.clear();
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

double ParseTree::getDouble(ParseNode pn) const noexcept{
    const Value& v = getValue(pn);
    assert(v.index() == double_index);
    return std::get<double>(v);
}

void ParseTree::setDouble(ParseNode pn, double val) noexcept{
    setValue(pn, val);
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
    return node == NONE ? 0 : getNumArgs(node);
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

template<typename T> ParseNode ParseTree::addNode(Op type, const Typeset::Selection& sel, const T& children) alloc_except {
    ParseNode pn = size();

    resize(size() + FIXED_FIELDS);
    setOp(pn, type);
    setSelection(pn, sel);
    setNumArgs(pn, children.size());

    //memcpy(data()+pn+FIXED_FIELDS, &children, children.size()*sizeof(ParseNode));
    insert(end(), children.begin(), children.end());

    return pn;
}

template<typename T> ParseNode ParseTree::addNode(Op type, const T& children) alloc_except {
    return addNode<T>(type, Typeset::Selection(getLeft(children[0]), getRight(children.back())), children);
}

template<size_t N>
ParseNode ParseTree::addNode(Op type, const Typeset::Selection& sel, const std::array<ParseNode, N>& children) alloc_except {
    return addNode<std::array<ParseNode, N>>(type, sel, children);
}

template<size_t N>
ParseNode ParseTree::addNode(Op type, const std::array<ParseNode, N>& children) alloc_except {
    return addNode<std::array<ParseNode, N>>(type, children);
}

ParseNode ParseTree::addTerminal(size_t type, const Typeset::Selection& c) alloc_except {
    return addNode<0>(type, c, {});
}

ParseNode ParseTree::addUnary(size_t type, const Typeset::Selection& c, ParseNode child) alloc_except {
    return addNode<1>(type, c, {child});
}

ParseNode ParseTree::addUnary(size_t type, size_t child) alloc_except {
    return addNode<1>(type, {child});
}

ParseNode ParseTree::addLeftUnary(size_t type, const Typeset::Marker& left, ParseNode child) alloc_except {
    return addNode<1>(type, Typeset::Selection(left, getRight(child)), {child});
}

ParseNode ParseTree::addRightUnary(size_t type, const Typeset::Marker& right, ParseNode child) alloc_except {
    return addNode<1>(type, Typeset::Selection(getLeft(child), right), {child});
}

ParseNode ParseTree::clone(ParseNode pn) alloc_except {
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
        if(a == NONE){
            setArg(cloned, i, NONE);
        }else{
            setArg(cloned, i, clone(a));
        }
    }

    return cloned;
}

ParseNode ParseTree::getZero(const Typeset::Selection& sel) alloc_except {
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setFlag(pn, 0.0);
    setScalar(pn);

    return pn;
}

ParseNode ParseTree::getOne(const Typeset::Selection& sel) alloc_except {
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setDouble(pn, 1.0);
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

void ParseTree::setString(ParseNode pn, const std::string& str) alloc_except{
    assert(getOp(pn) == OP_STRING);
    setFlag(pn, string_lits.size());
    string_lits.push_back(str);
}

const std::string& ParseTree::getString(ParseNode pn) const noexcept{
    assert(getOp(pn) == OP_STRING);
    return string_lits[getFlag(pn)];
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
        if(child != NONE){
            std::string child_id = std::to_string(size);
            graphvizHelper(src, child, size);
            src += "\tn" + id + "->n" + child_id + "\n";
        }
    }
}
#endif

template ParseNode ParseTree::arg<0>(ParseNode) const noexcept;
template ParseNode ParseTree::arg<1>(ParseNode) const noexcept;
template ParseNode ParseTree::arg<2>(ParseNode) const noexcept;
template ParseNode ParseTree::arg<3>(ParseNode) const noexcept;
template ParseNode ParseTree::arg<4>(ParseNode) const noexcept;
template void ParseTree::setArg<0>(ParseNode, ParseNode) noexcept;
template void ParseTree::setArg<1>(ParseNode, ParseNode) noexcept;
template void ParseTree::setArg<2>(ParseNode, ParseNode) noexcept;
template void ParseTree::setArg<3>(ParseNode, ParseNode) noexcept;
template void ParseTree::setArg<4>(ParseNode, ParseNode) noexcept;
template ParseNode ParseTree::addNode(Op, const Typeset::Selection&, const std::vector<ParseNode>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::vector<ParseNode>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Typeset::Selection&, const std::array<ParseNode, 2>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Typeset::Selection&, const std::array<ParseNode, 3>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Typeset::Selection&, const std::array<ParseNode, 4>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Typeset::Selection&, const std::array<ParseNode, 5>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 2>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 3>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 5>&) alloc_except;

}

}
