#ifndef HOPE_PARSE_TREE_H
#define HOPE_PARSE_TREE_H

#include "hope_parsenodetype.h"
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
    void clear() noexcept { std::vector<size_t>::clear(); }
    ParseNode back() const noexcept;
    bool empty() const noexcept;
    ParseNodeType getEnum(ParseNode node) const noexcept;
    void setEnum(ParseNode node, ParseNodeType type) noexcept;
    size_t numArgs(ParseNode node) const noexcept;
    void setStackOffset(ParseNode node, size_t val) noexcept;
    size_t stackOffset(ParseNode node) const noexcept;
    void setUpvalue(ParseNode node, size_t offset) noexcept;
    size_t upvalueOffset(ParseNode node) const noexcept;
    const Typeset::Selection& getSelection(ParseNode index) const noexcept;
    void setSelection(ParseNode index, const Typeset::Selection& sel) noexcept;
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
    void setRightMarker(size_t index, const Typeset::Marker& m) noexcept;
    void addIdentifier(const Typeset::Selection& c);
    ParseNode addTerminal(ParseNodeType type, const Typeset::Selection& c);
    ParseNode addUnary(ParseNodeType type, const Typeset::Selection& c, ParseNode child);
    ParseNode addUnary(ParseNodeType type, ParseNode child);
    ParseNode addLeftUnary(ParseNodeType type, const Typeset::Marker& left, ParseNode child);
    ParseNode addRightUnary(ParseNodeType type, const Typeset::Marker& right, ParseNode child);
    ParseNode addBinary(ParseNodeType type, const Typeset::Selection& c, ParseNode lhs, ParseNode rhs);
    ParseNode addBinary(ParseNodeType type, ParseNode lhs, ParseNode rhs);
    ParseNode addTernary(ParseNodeType type, const Typeset::Selection& c, ParseNode A, ParseNode B, ParseNode C);
    ParseNode addTernary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C);
    ParseNode addQuadary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D);
    ParseNode addQuadary(ParseNodeType type, const Typeset::Selection& c, ParseNode A, ParseNode B, ParseNode C, ParseNode D);
    ParseNode addPentary(ParseNodeType type, ParseNode A, ParseNode B, ParseNode C, ParseNode D, ParseNode E);

    static constexpr ParseNode EMPTY = std::numeric_limits<size_t>::max();

    class NaryBuilder{
    public:
        NaryBuilder(ParseTree& tree, ParseNodeType type);
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

    NaryBuilder naryBuilder(ParseNodeType type);

private:
    static constexpr size_t UNITIALIZED = std::numeric_limits<size_t>::max();
    static constexpr size_t FIXED_FIELDS = 6;
    static constexpr size_t RIGHT_MARKER_OFFSET = 5;
    static constexpr size_t LEFT_MARKER_OFFSET = 3;
    static constexpr size_t SIZE_OFFSET = 1;
    static constexpr size_t ENUM_OFFSET = 0;
    size_t fields(size_t node) const noexcept{ return numArgs(node)+FIXED_FIELDS; }
    void pushSelection(const Typeset::Selection& c);
    void pushMarker(const Typeset::Marker& m);
    void pushSelection(ParseNode src);
    void pushLeftMarker(ParseNode src);
    void pushRightMarker(ParseNode src);
};

}

}

#endif // HOPE_PARSE_TREE_H
