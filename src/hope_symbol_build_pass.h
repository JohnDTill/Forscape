#ifndef HOPE_SYMBOL_BUILD_PASS_H
#define HOPE_SYMBOL_BUILD_PASS_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"
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
    static static_map<std::string_view, Op> predef;
    size_t active_scope_id;
    unordered_map<Typeset::Selection, SymbolId> map;
    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;
    size_t closure_depth = 0;
    size_t cutoff = std::numeric_limits<size_t>::max();

    std::vector<size_t> refs;
    std::vector<size_t> ref_frames;
    unordered_map<ParseNode, size_t> node_to_capture;

    std::vector<ParseNode> potential_loop_vars;

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
    void appendEntry(ParseNode pn, size_t prev, bool immutable) alloc_except;
    void resolveStmt(ParseNode pn) alloc_except;
    void resolveExpr(ParseNode pn) alloc_except;
    void resolveEquality(ParseNode pn) alloc_except;
    void resolveAssignment(ParseNode pn) alloc_except;
    void resolveAssignmentId(ParseNode pn) alloc_except;
    void resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs) alloc_except;
    bool resolvePotentialIdSub(ParseNode pn) alloc_except;
    template <bool allow_imp_mult = false> void resolveReference(ParseNode pn) alloc_except;
    void resolveReference(ParseNode pn, size_t sym_id) alloc_except;
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
};

}

}

#endif // HOPE_SYMBOL_BUILD_PASS_H
