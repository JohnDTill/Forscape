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

    void clear() noexcept { std::vector<size_t>::clear(); }
    ParseNode back() const noexcept;
    bool empty() const noexcept;
    const Typeset::Marker& getLeft(ParseNode index) const noexcept;
    void setLeft(ParseNode index, const Typeset::Marker& m) noexcept;
    const Typeset::Marker& getRight(ParseNode index) const noexcept;
    void setRight(ParseNode index, const Typeset::Marker& m) noexcept;
    ParseNode arg(ParseNode node, size_t index) const noexcept;
    void setArg(ParseNode node, size_t index, ParseNode val) noexcept;
    ParseNode lhs(ParseNode node) const noexcept;
    ParseNode rhs(ParseNode node) const noexcept;
    ParseNode child(ParseNode node) const noexcept;
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

    #ifndef NDEBUG
    std::string toGraphviz(ParseNode root) const;
    #endif

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

private:
    static constexpr size_t UNITIALIZED = std::numeric_limits<size_t>::max();
    static constexpr size_t RIGHT_MARKER_OFFSET = SELECTION_OFFSET;
    static constexpr size_t LEFT_MARKER_OFFSET = SELECTION_OFFSET - 2;
    size_t fields(size_t node) const noexcept{ return getNumArgs(node)+FIXED_FIELDS; }

    #ifndef NDEBUG
    void graphvizHelper(std::string& src, ParseNode n, size_t& size) const;
    void writeType(std::string& src, ParseNode n) const;
    #endif
};

}

}

#endif // HOPE_PARSE_TREE_H
