#ifndef HOPE_SYMBOL_BUILD_PASS_H
#define HOPE_SYMBOL_BUILD_PASS_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hope {

namespace Code {

class ParseTree;

class SymbolTableBuilder{
private:
    std::vector<Error>& errors;
    std::vector<Error>& warnings;
    ParseTree& parse_tree;

public:
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model) noexcept;
    void resolveSymbols() alloc_except;
    SymbolTable symbol_table;

private:
    static const std::unordered_map<std::string_view, Op> predef;
    size_t active_scope_id;
    std::unordered_map<Typeset::Selection, SymbolId> map;
    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;
    size_t closure_depth = 0;
    size_t cutoff = std::numeric_limits<size_t>::max();

    //EVENTUALLY: redesign nesting allocation
    //This should probably be some kind of map to intrusive linked list, like for symbols in general
    std::vector<std::unordered_set<size_t>> ref_list_sets;

    void reset() noexcept;
    ScopeSegment& activeScope() noexcept;
    void addScope(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode closure = NONE) alloc_except;
    void closeScope(const Typeset::Marker& end) noexcept;
    Symbol& lastDefinedSymbol() noexcept;
    size_t symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept;
    void increaseLexicalDepth(const Typeset::Selection& name, const Typeset::Marker& begin) alloc_except;
    void decreaseLexicalDepth(const Typeset::Marker& end) alloc_except;
    void increaseClosureDepth(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode pn) alloc_except;
    void decreaseClosureDepth(const Typeset::Marker& end) alloc_except;
    void makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable) alloc_except;
    void appendEntry(size_t index, ParseNode pn, size_t prev, bool immutable) alloc_except;
    void resolveStmt(ParseNode pn) alloc_except;
    void resolveExpr(ParseNode pn) alloc_except;
    void resolveEquality(ParseNode pn) alloc_except;
    void resolveAssignment(ParseNode pn) alloc_except;
    void resolveAssignmentId(ParseNode pn) alloc_except;
    void resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs) alloc_except;
    bool resolvePotentialIdSub(ParseNode pn) alloc_except;
    template <bool allow_imp_mult = false> void resolveReference(ParseNode pn) alloc_except;
    void resolveReference(ParseNode pn, const Typeset::Selection& c, size_t sym_id) alloc_except;
    void resolveIdMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except;
    void resolveScriptMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except;
    void resolveConditional1(const Typeset::Selection& name, ParseNode pn) alloc_except;
    void resolveConditional2(ParseNode pn) alloc_except;
    void resolveFor(ParseNode pn) alloc_except;
    void resolveBody(const Typeset::Selection& name, ParseNode pn) alloc_except;
    void resolveBlock(ParseNode pn) alloc_except;
    void resolveDefault(ParseNode pn) alloc_except;
    void resolveLambda(ParseNode pn) alloc_except;
    void resolveAlgorithm(ParseNode pn) alloc_except;
    void resolvePrototype(ParseNode pn) alloc_except;
    void resolveSubscript(ParseNode pn) alloc_except;
    void resolveBig(ParseNode pn) alloc_except;
    void resolveDerivative(ParseNode pn) alloc_except;
    bool defineLocalScope(ParseNode pn, bool immutable = true) alloc_except;
    bool declared(ParseNode pn) const noexcept;
    size_t symIndex(ParseNode pn) const noexcept;

    struct Closure {
        ParseNode fn;
        std::unordered_map<size_t, size_t> upvalue_indices;
        size_t num_upvalues = 0;
        std::vector<std::pair<size_t, bool> > upvalues;
        std::vector<size_t> captured;

        Closure() noexcept {}
        Closure(ParseNode fn) noexcept : fn(fn) {}
    };
};

}

}

#endif // HOPE_SYMBOL_BUILD_PASS_H
