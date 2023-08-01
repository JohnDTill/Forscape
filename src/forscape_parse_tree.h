#ifndef FORSCAPE_PARSE_TREE_H
#define FORSCAPE_PARSE_TREE_H

#include <code_ast_fields.h>
#include <code_parsenode_ops.h>
#include <cassert>
#include <limits>
#include <string>
#include <vector>

#ifndef NDEBUG
#include <unordered_set>
#endif

namespace Forscape {

namespace Typeset {
class Selection;
struct Marker;
class Model;
}

using Typeset::Selection; //EVENTUALLY: Selection may depend on environment, e.g. string_view

namespace Code {

struct Symbol;

class ParseTree {
public:
    FORSCAPE_AST_FIELD_CODEGEN_DECLARATIONS

    #ifndef NDEBUG
    FORSCAPE_UNORDERED_MAP<std::string, std::unordered_set<std::string>> aliases;
    #endif

    ParseTree() noexcept;
    void clear() noexcept;
    bool empty() const noexcept;
    const Typeset::Marker& getLeft(ParseNode pn) const noexcept;
    void setLeft(ParseNode pn, const Typeset::Marker& m) noexcept;
    const Typeset::Marker& getRight(ParseNode pn) const noexcept;
    void setRight(ParseNode pn, const Typeset::Marker& m) noexcept;
    ParseNode arg(ParseNode pn, size_t index) const noexcept;
    template<size_t index> ParseNode arg(ParseNode pn) const noexcept;
    void setArg(ParseNode pn, size_t index, ParseNode val) noexcept;
    template<size_t index> void setArg(ParseNode pn, ParseNode val) noexcept;
    void reduceNumArgs(ParseNode pn, size_t sze) noexcept;
    double getDouble(ParseNode pn) const noexcept;
    void setDouble(ParseNode pn, double val) noexcept;
    ParseNode lhs(ParseNode pn) const noexcept;
    ParseNode rhs(ParseNode pn) const noexcept;
    ParseNode child(ParseNode pn) const noexcept;
    ParseNode valCapList(ParseNode pn) const noexcept;
    ParseNode refCapList(ParseNode pn) const noexcept;
    void setSymId(ParseNode pn, size_t sym_id) noexcept;
    size_t getSymId(ParseNode pn) const noexcept;
    void setSymbol(ParseNode pn, const Symbol* const symbol) noexcept;
    Symbol* getSymbol(ParseNode pn) const noexcept;
    Typeset::Model* getModel(ParseNode pn) const noexcept;
    void setRefList(ParseNode fn, ParseNode list) noexcept;
    ParseNode paramList(ParseNode pn) const noexcept;
    ParseNode body(ParseNode pn) const noexcept;
    void setBody(ParseNode pn, ParseNode body) noexcept;
    ParseNode algName(ParseNode pn) const noexcept;
    size_t valListSize(ParseNode pn) const noexcept;
    ParseNode unitVectorElem(ParseNode pn) const noexcept;
    void setUnitVectorElem(ParseNode pn, ParseNode val) noexcept;
    ParseNode unitVectorRows(ParseNode pn) const noexcept;
    void setUnitVectorRows(ParseNode pn, ParseNode val) noexcept;
    ParseNode unitVectorCols(ParseNode pn) const noexcept;
    void setUnitVectorCols(ParseNode pn, ParseNode val) noexcept;
    std::string str(ParseNode pn) const alloc_except;
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
    ParseNode popNaryChild() noexcept;
    ParseNode finishNary(Op type, const Selection& sel) alloc_except;
    ParseNode finishNary(Op type) alloc_except;
    void cancelNary() noexcept;

    #ifndef NDEBUG
    bool isNode(ParseNode pn) const noexcept;
    bool isLastAllocatedNode(ParseNode pn) const noexcept;
    bool inFinalState() const noexcept;
    template<typename T> bool notInTree(const T& obj) const noexcept;
    std::string toGraphviz() const;
    std::string toGraphviz(ParseNode pn) const;
    #endif

    size_t root;

    void patchClones() noexcept;
    void patchClonedTypes() noexcept;

    size_t append(const ParseTree& other);
    void shift(ParseNode pn, size_t offset);
    size_t offset() const noexcept;
    bool hasChild(ParseNode pn, ParseNode child) const noexcept;

private:
    std::vector<size_t> data;
    std::vector<ParseNode> nary_construction_stack;
    std::vector<size_t> nary_start;

    #ifndef NDEBUG
    FORSCAPE_UNORDERED_SET<ParseNode> created;
    #endif

    static constexpr size_t LEFT_MARKER_OFFSET = SELECTION_OFFSET + 2;
    static constexpr size_t RIGHT_MARKER_OFFSET = SELECTION_OFFSET;

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

#endif // FORSCAPE_PARSE_TREE_H
