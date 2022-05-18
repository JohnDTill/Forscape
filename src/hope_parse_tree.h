#ifndef HOPE_PARSE_TREE_H
#define HOPE_PARSE_TREE_H

#include <code_ast_fields.h>
#include <code_parsenode_ops.h>
#include <cassert>
#include <limits>
#include <string>
#include <vector>

namespace Hope {

namespace Typeset {
class Selection;
struct Marker;
}

using Typeset::Selection; //EVENTUALLY: Selection may depend on environment, e.g. string_view

namespace Code {

//DO THIS - think about templating the payload and generalising to "AppendOnlyTree"
class ParseTree : private std::vector<size_t> {
public:
    HOPE_AST_FIELD_CODEGEN_DECLARATIONS

    void clear() noexcept;
    bool empty() const noexcept;
    const Typeset::Marker& getLeft(ParseNode index) const noexcept;
    void setLeft(ParseNode index, const Typeset::Marker& m) noexcept;
    const Typeset::Marker& getRight(ParseNode index) const noexcept;
    void setRight(ParseNode index, const Typeset::Marker& m) noexcept;
    ParseNode arg(ParseNode node, size_t index) const noexcept;
    template<size_t index> ParseNode arg(ParseNode node) const noexcept;
    void setArg(ParseNode node, size_t index, ParseNode val) noexcept;
    template<size_t index> void setArg(ParseNode node, ParseNode val) noexcept;
    double getDouble(ParseNode pn) const noexcept;
    void setDouble(ParseNode pn, double val) noexcept;
    ParseNode lhs(ParseNode node) const noexcept;
    ParseNode rhs(ParseNode node) const noexcept;
    ParseNode child(ParseNode node) const noexcept;
    ParseNode valCapList(ParseNode node) const noexcept;
    ParseNode refCapList(ParseNode node) const noexcept;
    void setRefList(ParseNode fn, ParseNode list) noexcept;
    ParseNode paramList(ParseNode node) const noexcept;
    ParseNode body(ParseNode node) const noexcept;
    void setBody(ParseNode node, ParseNode body) noexcept;
    ParseNode algName(ParseNode node) const noexcept;
    size_t valListSize(ParseNode node) const noexcept;
    ParseNode unitVectorElem(ParseNode node) const noexcept;
    void setUnitVectorElem(ParseNode node, ParseNode val) noexcept;
    ParseNode unitVectorRows(ParseNode node) const noexcept;
    void setUnitVectorRows(ParseNode node, ParseNode val) noexcept;
    ParseNode unitVectorCols(ParseNode node) const noexcept;
    void setUnitVectorCols(ParseNode node, ParseNode val) noexcept;
    std::string str(ParseNode node) const;
    template<typename T> ParseNode addNode(Op type, const Selection& sel, const T& children) alloc_except;
    template<typename T> ParseNode addNode(Op type, const T& children) alloc_except;
    template<size_t N> ParseNode addNode(Op type, const Selection& sel, const std::array<ParseNode, N>& children) alloc_except;
    template<size_t N> ParseNode addNode(Op type, const std::array<ParseNode, N>& children) alloc_except;
    ParseNode addTerminal(Op type, const Selection& c) alloc_except;
    ParseNode addUnary(Op type, const Selection& c, ParseNode child) alloc_except;
    ParseNode addUnary(Op type, ParseNode child) alloc_except;
    ParseNode addLeftUnary(Op type, const Typeset::Marker& left, ParseNode child) alloc_except;
    ParseNode addRightUnary(Op type, const Typeset::Marker& right, ParseNode child) alloc_except;
    ParseNode clone(ParseNode pn) alloc_except;
    ParseNode getZero(const Selection& sel) alloc_except;
    ParseNode getOne(const Selection& sel) alloc_except;
    bool definitelyScalar(ParseNode pn) const noexcept;
    bool definitelyNotScalar(ParseNode pn) const noexcept;
    bool definitelyMatrix(ParseNode pn) const noexcept;
    bool definitelyR3(ParseNode pn) const noexcept;
    bool nonSquare(ParseNode pn) const noexcept;
    bool maybeR3(ParseNode pn) const noexcept;
    void setScalar(ParseNode pn) noexcept;
    void setR3(ParseNode pn) noexcept;
    void copyDims(ParseNode dest, ParseNode src) noexcept;
    void transposeDims(ParseNode dest, ParseNode src) noexcept;

    void prepareNary() alloc_except;
    void addNaryChild(ParseNode pn) alloc_except;
    ParseNode finishNary(Op type, const Selection& sel) alloc_except;
    ParseNode finishNary(Op type) alloc_except;

    #ifndef NDEBUG
    bool inFinalState() const noexcept;
    std::string toGraphviz() const;
    std::string toGraphviz(ParseNode pn) const;
    #endif

    size_t root;

    void patchClones() noexcept;
    void patchClonedTypes() noexcept;

private:
    std::vector<ParseNode> nary_construction_stack;
    std::vector<size_t> nary_start;

    static constexpr size_t UNITIALIZED = std::numeric_limits<size_t>::max();
    static constexpr size_t LEFT_MARKER_OFFSET = SELECTION_OFFSET + 2;
    static constexpr size_t RIGHT_MARKER_OFFSET = SELECTION_OFFSET;
    size_t fields(size_t node) const noexcept{ return getNumArgs(node)+FIXED_FIELDS; }

    #ifndef NDEBUG
    void graphvizHelper(std::string& src, ParseNode n, size_t& size) const;
    void writeType(std::string& src, ParseNode n) const;
    #endif

    std::vector<std::pair<ParseNode, ParseNode>> cloned_vars;
};

extern template ParseNode ParseTree::arg<0>(ParseNode) const noexcept;
extern template ParseNode ParseTree::arg<1>(ParseNode) const noexcept;
extern template ParseNode ParseTree::arg<2>(ParseNode) const noexcept;
extern template ParseNode ParseTree::arg<3>(ParseNode) const noexcept;
extern template ParseNode ParseTree::arg<4>(ParseNode) const noexcept;
extern template void ParseTree::setArg<0>(ParseNode, ParseNode) noexcept;
extern template void ParseTree::setArg<1>(ParseNode, ParseNode) noexcept;
extern template void ParseTree::setArg<2>(ParseNode, ParseNode) noexcept;
extern template void ParseTree::setArg<3>(ParseNode, ParseNode) noexcept;
extern template void ParseTree::setArg<4>(ParseNode, ParseNode) noexcept;
extern template ParseNode ParseTree::addNode(Op, const Selection&, const std::vector<ParseNode>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const std::vector<ParseNode>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 2>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 3>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 4>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const Selection&, const std::array<ParseNode, 5>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 2>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 3>&) alloc_except;
extern template ParseNode ParseTree::addNode(Op, const std::array<ParseNode, 5>&) alloc_except;

}

}

#endif // HOPE_PARSE_TREE_H
