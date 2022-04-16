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

namespace Code {

typedef size_t ParseNode;

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
    double getFlagAsDouble(ParseNode pn) const noexcept;
    void setFlag(ParseNode pn, double val) noexcept;
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
    std::string str(ParseNode node) const;
    ParseNode addTerminal(Op type, const Typeset::Selection& c);
    ParseNode addUnary(Op type, const Typeset::Selection& c, ParseNode child);
    ParseNode addUnary(Op type, ParseNode child);
    ParseNode addLeftUnary(Op type, const Typeset::Marker& left, ParseNode child);
    ParseNode addRightUnary(Op type, const Typeset::Marker& right, ParseNode child);
    ParseNode addBinary(Op type, const Typeset::Selection& c, ParseNode lhs, ParseNode rhs);
    ParseNode addBinary(Op type, ParseNode lhs, ParseNode rhs);
    ParseNode addTernary(Op type, const Typeset::Selection& c, ParseNode A, ParseNode B, ParseNode C);
    ParseNode addTernary(Op type, ParseNode A, ParseNode B, ParseNode C);
    ParseNode addQuadary(Op type, ParseNode A, ParseNode B, ParseNode C, ParseNode D);
    ParseNode addQuadary(Op type, const Typeset::Selection& c, ParseNode A, ParseNode B, ParseNode C, ParseNode D);
    ParseNode addPentary(Op type, ParseNode A, ParseNode B, ParseNode C, ParseNode D, ParseNode E);
    ParseNode clone(ParseNode pn);
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

    #ifndef NDEBUG
    std::string toGraphviz() const;
    std::string toGraphviz(ParseNode pn) const;
    #endif

    size_t root;

    static constexpr ParseNode EMPTY = std::numeric_limits<size_t>::max();

    class NaryBuilder{
    public:
        NaryBuilder(ParseTree& tree, Op type);
        void addNaryChild(ParseNode index);
        ParseNode finalize();
        ParseNode finalize(const Typeset::Marker& right);
        ParseNode finalize(const Typeset::Selection& c);

    private:
        ParseTree& tree;
        size_t type;
        std::vector<ParseNode> children;

        #ifndef NDEBUG
        bool built = false;
        public: ~NaryBuilder(){ assert(built); }
        #endif
    };

    NaryBuilder naryBuilder(Op type);

    void patchClones() noexcept;

private:
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

}

}

#endif // HOPE_PARSE_TREE_H
