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
    data.clear();
    nary_construction_stack.clear();
    nary_start.clear();
    cloned_vars.clear();

    #ifndef NDEBUG
    created.clear();
    #endif
}

bool ParseTree::empty() const noexcept {
    return data.empty();
}

const Typeset::Marker& ParseTree::getLeft(ParseNode pn) const noexcept {
    assert(isNode(pn));
    return *reinterpret_cast<const Typeset::Marker*>(data.data()+pn+LEFT_MARKER_OFFSET);
}

void ParseTree::setLeft(ParseNode pn, const Typeset::Marker& m) noexcept {
    assert(isNode(pn));
    *reinterpret_cast<Typeset::Marker*>(data.data()+pn+LEFT_MARKER_OFFSET) = m;
}

const Typeset::Marker& ParseTree::getRight(ParseNode pn) const noexcept {
    assert(isNode(pn));
    return *reinterpret_cast<const Typeset::Marker*>(data.data()+pn+RIGHT_MARKER_OFFSET);
}

void ParseTree::setRight(ParseNode pn, const Typeset::Marker& m) noexcept {
    assert(isNode(pn));
    *reinterpret_cast<Typeset::Marker*>(data.data()+pn+RIGHT_MARKER_OFFSET) = m;
}

ParseNode ParseTree::arg(ParseNode pn, size_t index) const noexcept {
    assert(index < getNumArgs(pn));
    return data[pn+FIXED_FIELDS+index];
}

template<size_t index>
ParseNode ParseTree::arg(ParseNode pn) const noexcept {
    assert(index < getNumArgs(pn));
    return data[pn+FIXED_FIELDS+index];
}

void ParseTree::setArg(ParseNode pn, size_t index, ParseNode val) noexcept {
    assert(index < getNumArgs(pn));
    data[pn+FIXED_FIELDS+index] = val;
}

void ParseTree::reduceNumArgs(ParseNode pn, size_t sze) noexcept {
    assert(getNumArgs(pn) >= sze);
    setNumArgs(pn, sze);
}

template<size_t index> void ParseTree::setArg(ParseNode pn, ParseNode val) noexcept {
    assert(index < getNumArgs(pn));
    data[pn+FIXED_FIELDS+index] = val;
}

double ParseTree::getDouble(ParseNode pn) const noexcept {
    assert(isNode(pn));
    const Value& v = getValue(pn);
    assert(v.index() == double_index);
    return std::get<double>(v);
}

void ParseTree::setDouble(ParseNode pn, double val) noexcept {
    assert(isNode(pn));
    setValue(pn, val);
}

ParseNode ParseTree::lhs(ParseNode pn) const noexcept {
    assert(getNumArgs(pn) == 2);
    return arg<0>(pn);
}

ParseNode ParseTree::rhs(ParseNode pn) const noexcept {
    assert(getNumArgs(pn) == 2);
    return arg<1>(pn);
}

ParseNode ParseTree::child(ParseNode pn) const noexcept {
    assert(getNumArgs(pn) == 1);
    return arg<0>(pn);
}

ParseNode ParseTree::valCapList(ParseNode pn) const noexcept {
    assert(getOp(pn) == OP_ALGORITHM || getOp(pn) == OP_LAMBDA);
    return arg<0>(pn);
}

ParseNode ParseTree::refCapList(ParseNode pn) const noexcept {
    assert(getOp(pn) == OP_ALGORITHM || getOp(pn) == OP_LAMBDA);
    return arg<1>(pn);
}

void ParseTree::setSymId(ParseNode pn, size_t sym_id) noexcept {
    assert(isNode(pn));
    //assert(getOp(pn) == OP_IDENTIFIER); //DO THIS
    setFlag(pn, sym_id);
}

size_t ParseTree::getSymId(ParseNode pn) const noexcept{
    assert(isNode(pn));
    assert(getOp(pn) == OP_IDENTIFIER);
    return getFlag(pn);
}

void ParseTree::setRefList(ParseNode fn, ParseNode list) noexcept {
    assert(getOp(fn) == OP_ALGORITHM || getOp(fn) == OP_LAMBDA);
    setArg<1>(fn, list);
}

ParseNode ParseTree::paramList(ParseNode pn) const noexcept {
    assert(getOp(pn) == OP_ALGORITHM || getOp(pn) == OP_LAMBDA);
    return arg<2>(pn);
}

ParseNode ParseTree::body(ParseNode pn) const noexcept {
    assert(getOp(pn) == OP_ALGORITHM || getOp(pn) == OP_LAMBDA);
    return arg<3>(pn);
}

void ParseTree::setBody(ParseNode pn, ParseNode body) noexcept {
    assert(getOp(pn) == OP_ALGORITHM || getOp(pn) == OP_LAMBDA);
    setArg<3>(pn, body);
}

ParseNode ParseTree::algName(ParseNode pn) const noexcept {
    assert(getOp(pn) == OP_ALGORITHM);
    return arg<4>(pn);
}

size_t ParseTree::valListSize(ParseNode pn) const noexcept {
    return pn == NONE ? 0 : getNumArgs(pn);
}

ParseNode ParseTree::unitVectorElem(ParseNode pn) const noexcept {
    return arg<0>(pn);
}

void ParseTree::setUnitVectorElem(ParseNode pn, ParseNode val) noexcept {
    setArg<0>(pn, val);
}

ParseNode ParseTree::unitVectorRows(ParseNode pn) const noexcept {
    return arg<1>(pn);
}

void ParseTree::setUnitVectorRows(ParseNode pn, ParseNode val) noexcept {
    setArg<1>(pn, val);
}

ParseNode ParseTree::unitVectorCols(ParseNode pn) const noexcept {
    return arg<2>(pn);
}

void ParseTree::setUnitVectorCols(ParseNode pn, ParseNode val) noexcept {
    setArg<2>(pn, val);
}

std::string ParseTree::str(ParseNode pn) const alloc_except {
    return getSelection(pn).str();
}

template<typename T> ParseNode ParseTree::addNode(Op type, const Selection& sel, const T& children) alloc_except {
    assert(notInTree(sel));
    assert(notInTree(children));

    ParseNode pn = data.size();
    #ifndef NDEBUG
    for(ParseNode child : children) assert(child == NONE || isNode(child));
    created.insert(pn);
    #endif
    data.resize(data.size() + FIXED_FIELDS);
    setOp(pn, type);
    setSelection(pn, sel);
    setNumArgs(pn, children.size());

    data.insert(data.end(), children.begin(), children.end());

    return pn;
}

template<typename T> ParseNode ParseTree::addNode(Op type, const T& children) alloc_except {
    return addNode<T>(type, Selection(getLeft(children[0]), getRight(children.back())), children);
}

template<size_t N>
ParseNode ParseTree::addNode(Op type, const Selection& sel, const std::array<ParseNode, N>& children) alloc_except {
    return addNode<std::array<ParseNode, N>>(type, sel, children);
}

template<size_t N> ParseNode ParseTree::addNode(Op type, const std::array<ParseNode, N>& children) alloc_except {
    return addNode<std::array<ParseNode, N>>(type, children);
}

ParseNode ParseTree::addTerminal(size_t type, const Selection& c) alloc_except {
    return addNode<0>(type, c, {});
}

ParseNode ParseTree::addUnary(size_t type, const Selection& c, ParseNode child) alloc_except {
    return addNode<1>(type, c, {child});
}

ParseNode ParseTree::addUnary(size_t type, size_t child) alloc_except {
    return addNode<1>(type, {child});
}

ParseNode ParseTree::addLeftUnary(size_t type, const Typeset::Marker& left, ParseNode child) alloc_except {
    return addNode<1>(type, Selection(left, getRight(child)), {child});
}

ParseNode ParseTree::addRightUnary(size_t type, const Typeset::Marker& right, ParseNode child) alloc_except {
    return addNode<1>(type, Selection(getLeft(child), right), {child});
}

ParseNode ParseTree::clone(ParseNode pn) alloc_except {
    ParseNode cloned = data.size();
    #ifndef NDEBUG
    created.insert(cloned);
    #endif
    switch (getOp(pn)) {
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:
        case OP_ALGORITHM:
        case OP_PROTOTYPE_ALG:
            cloned_vars.push_back(std::make_pair(pn, cloned));
    }

    data.insert(data.end(), data.data()+pn, data.data()+pn+FIXED_FIELDS);
    size_t nargs = getNumArgs(pn);
    data.resize(data.size() + nargs);
    for(size_t i = 0; i < nargs; i++){
        ParseNode a = arg(pn, i);
        setArg(cloned, i, (a==NONE) ? NONE : clone(a));
    }

    return cloned;
}

ParseNode ParseTree::getZero(const Selection& sel) alloc_except {
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setDouble(pn, 0.0);
    setScalar(pn);

    return pn;
}

ParseNode ParseTree::getOne(const Selection& sel) alloc_except {
    ParseNode pn = addTerminal(OP_INTEGER_LITERAL, sel);
    setDouble(pn, 1.0);
    setScalar(pn);

    return pn;
}

bool ParseTree::definitelyScalar(ParseNode pn) const noexcept {
    return getRows(pn) == 1 && getCols(pn) == 1;
}

bool ParseTree::definitelyNotScalar(ParseNode pn) const noexcept {
    return getRows(pn) > 1 || getCols(pn) > 1;
}

bool ParseTree::definitelyMatrix(ParseNode pn) const noexcept {
    return getRows(pn) > 1 && getCols(pn) > 1;
}

bool ParseTree::definitelyR3(ParseNode pn) const noexcept {
    return getRows(pn) == 3 && getCols(pn) == 1;
}

bool ParseTree::nonSquare(ParseNode pn) const noexcept {
    size_t rows = getRows(pn);
    size_t cols = getCols(pn);
    return rows != UNKNOWN_SIZE && cols != UNKNOWN_SIZE && rows != cols;
}

bool ParseTree::maybeR3(ParseNode pn) const noexcept {
    size_t rows = getRows(pn);
    size_t cols = getCols(pn);
    return (rows == 3 || rows == UNKNOWN_SIZE) && (cols == 1 || cols == UNKNOWN_SIZE);
}

void ParseTree::setScalar(ParseNode pn) noexcept {
    setType(pn, StaticPass::NUMERIC);
    setRows(pn, 1);
    setCols(pn, 1);
}

void ParseTree::setR3(ParseNode pn) noexcept {
    setRows(pn, 3);
    setCols(pn, 1);
}

void ParseTree::copyDims(ParseNode dest, ParseNode src) noexcept {
    setRows(dest, getRows(src));
    setCols(dest, getCols(src));
}

void ParseTree::transposeDims(ParseNode dest, ParseNode src) noexcept {
    setRows(dest, getCols(src));
    setCols(dest, getRows(src));
}

void ParseTree::prepareNary() alloc_except {
    nary_start.push_back(nary_construction_stack.size());
}

void ParseTree::addNaryChild(ParseNode pn) alloc_except {
    assert(isNode(pn));
    nary_construction_stack.push_back(pn);
}

ParseNode ParseTree::popNaryChild() noexcept{
    ParseNode pn = nary_construction_stack.back();
    nary_construction_stack.pop_back();
    return pn;
}

ParseNode ParseTree::finishNary(Op type, const Selection& sel) alloc_except {
    assert(!nary_start.empty());
    assert(notInTree(sel));
    size_t N = nary_construction_stack.size()-nary_start.back();

    ParseNode pn = data.size();
    #ifndef NDEBUG
    created.insert(pn);
    #endif
    data.resize(data.size() + FIXED_FIELDS);
    setOp(pn, type);
    setSelection(pn, sel);
    setNumArgs(pn, N);
    data.insert(data.end(), nary_construction_stack.end()-N, nary_construction_stack.end());

    nary_construction_stack.resize(nary_start.back());
    nary_start.pop_back();

    return pn;
}

ParseNode ParseTree::finishNary(Op type) alloc_except {
    assert(!nary_start.empty());
    return finishNary(type, Selection(
                          getLeft(nary_construction_stack[nary_start.back()]), getRight(nary_construction_stack.back())));
}

void ParseTree::patchClones() noexcept {
    for(const auto& entry : cloned_vars){
        ParseNode orig = entry.first;
        ParseNode clone = entry.second;

        setOp(clone, getOp(orig));
        setFlag(clone, getFlag(orig));
    }
}

void ParseTree::patchClonedTypes() noexcept {
    for(const auto& entry : cloned_vars){
        ParseNode orig = entry.first;
        ParseNode clone = entry.second;

        setType(orig, getType(clone));
        setRows(orig, getRows(clone));
        setCols(orig, getCols(clone));
    }
}

#ifndef NDEBUG
bool ParseTree::isNode(ParseNode pn) const noexcept {
    return created.find(pn) != created.end();
}

bool ParseTree::isLastAllocatedNode(ParseNode pn) const noexcept {
    return pn + FIXED_FIELDS + getNumArgs(pn) == data.size();
}

bool ParseTree::inFinalState() const noexcept {
    return nary_construction_stack.empty() && nary_start.empty();
}

template<typename T> bool ParseTree::notInTree(const T& obj) const noexcept {
    if(data.empty()) return true;

    auto potential_index = reinterpret_cast<const size_t*>(&obj) - &data[0];
    return potential_index < 0 || static_cast<size_t>(potential_index) >= data.size();
}

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
template ParseNode ParseTree::addNode(Op, const Selection&, const std::vector<ParseNode>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::vector<ParseNode>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 2>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 3>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 4>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 5>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 2>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 3>&) alloc_except;
template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 5>&) alloc_except;

}

}
