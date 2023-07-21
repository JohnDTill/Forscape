#ifndef FORSCAPE_SYMBOL_LEXICAL_PASS_H
#define FORSCAPE_SYMBOL_LEXICAL_PASS_H

#include "forscape_common.h"
#include "forscape_error.h"
#include "forscape_parse_tree.h"
#include "forscape_symbol_table.h"
#include <vector>

namespace Forscape {

namespace Code {

class ParseTree;

class SymbolLexicalPass {
private:
    std::vector<Error>& errors;
    std::vector<Error>& warnings;
    ParseTree& parse_tree;
    Typeset::Model* model;

public:
    SymbolLexicalPass(ParseTree& parse_tree, Typeset::Model* model) noexcept;
    void resolveSymbols() alloc_except;
    SymbolTable symbol_table;
    static FORSCAPE_STATIC_MAP<std::string_view, Op> predef;

private:
    std::vector<Symbol>& symbols;
    FORSCAPE_UNORDERED_MAP<Typeset::Selection, SymbolIndex>& lexical_map;
    std::vector<ScopeSegment>& scope_segments;
    std::vector<SymbolUsage>& symbol_usages;
    size_t active_scope_id;
    static constexpr size_t GLOBAL_DEPTH = 0;
    uint16_t lexical_depth = GLOBAL_DEPTH;
    uint8_t closure_depth = 0;
    size_t cutoff = std::numeric_limits<size_t>::max();

    std::vector<SymbolIndex> refs;
    std::vector<size_t> ref_frames;
    std::vector<ParseNode> processed_refs;

    std::vector<ParseNode> potential_loop_vars;

    void reset() noexcept;
    ScopeSegment& activeScope() noexcept;
    void addScope(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode closure = NONE) alloc_except;
    void closeScope(const Typeset::Marker& end) noexcept;
    Symbol& lastDefinedSymbol() noexcept;
    size_t symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept;
    void increaseLexicalDepth(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin) alloc_except;
    void decreaseLexicalDepth(const Typeset::Marker& end) alloc_except;
    void increaseClosureDepth(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode pn) alloc_except;
    void decreaseClosureDepth(const Typeset::Marker& end) alloc_except;
    void makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable) alloc_except;
    void appendEntry(ParseNode pn, size_t& old_entry, bool immutable, bool warn_on_shadow = true) alloc_except;
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
    void resolveConditional1(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except;
    void resolveConditional2(ParseNode pn) alloc_except;
    void resolveFor(ParseNode pn) alloc_except;
    void resolveRangedFor(ParseNode pn) alloc_except;
    void resolveSettingsUpdate(ParseNode pn) alloc_except;
    void resolveSwitch(ParseNode pn) alloc_except;
    void resolveBody(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except;
    void resolveBlock(ParseNode pn) alloc_except;
    void resolveLexicalScope(ParseNode pn) alloc_except;
    void resolveDefault(ParseNode pn) alloc_except;
    void resolveLambda(ParseNode pn) alloc_except;
    void resolveAlgorithm(ParseNode pn) alloc_except;
    void resolvePrototype(ParseNode pn) alloc_except;
    void resolveClass(ParseNode pn) alloc_except;
    void resolveSubscript(ParseNode pn) alloc_except;
    void resolveBig(ParseNode pn) alloc_except;
    void resolveDerivative(ParseNode pn) alloc_except;
    void resolveLimit(ParseNode pn) alloc_except;
    void resolveIndefiniteIntegral(ParseNode pn) alloc_except;
    void resolveDefiniteIntegral(ParseNode pn) alloc_except;
    void resolveSetBuilder(ParseNode pn) alloc_except;
    void resolveUnknownDeclaration(ParseNode pn) alloc_except;
    void resolveImport(ParseNode pn) alloc_except;
    void resolveFromImport(ParseNode pn) alloc_except;
    void resolveNamespace(ParseNode pn) alloc_except;
    void loadScope(ParseNode pn, size_t sym_id) alloc_except;
    void unloadScope(ParseNode body, size_t sym_id) alloc_except;
    void resolveScopeAccess(ParseNode pn) alloc_except;
    bool defineLocalScope(ParseNode pn, bool immutable = true, bool warn_on_shadow = true) alloc_except;
    bool declared(ParseNode pn) const noexcept;
    size_t symIndex(ParseNode pn) const noexcept;
    Settings& settings() const noexcept;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    template<bool first = true> void fixSubIdDocMap(ParseNode pn) const alloc_except;
    #endif
};

}

}

#endif // FORSCAPE_SYMBOL_LEXICAL_PASS_H
