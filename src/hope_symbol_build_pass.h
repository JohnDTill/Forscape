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
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model);
    void resolveSymbols();
    SymbolTable symbol_table;

private:
    static const std::unordered_map<std::string_view, Op> predef;
    size_t active_scope_id;
    std::unordered_map<Typeset::Selection, SymbolId> map;
    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;
    size_t closure_depth = 0;

    //EVENTUALLY: redesign nesting allocation
    //This should probably be some kind of map to intrusive linked list, like for symbols in general
    std::vector<std::unordered_set<size_t>> ref_list_sets;

    void reset() noexcept;
    ScopeSegment& activeScope() noexcept;
    void addScope(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode closure = NONE);
    void closeScope(const Typeset::Marker& end) noexcept;
    Symbol& lastDefinedSymbol() noexcept;
    size_t symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept;
    void increaseLexicalDepth(const Typeset::Selection& name, const Typeset::Marker& begin);
    void decreaseLexicalDepth(const Typeset::Marker& end);
    void increaseClosureDepth(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode pn);
    void decreaseClosureDepth(const Typeset::Marker& end);
    void makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable);
    void appendEntry(size_t index, ParseNode pn, size_t prev, bool immutable);
    void resolveStmt(ParseNode pn);
    void resolveExpr(ParseNode pn);
    void resolveEquality(ParseNode pn);
    void resolveAssignment(ParseNode pn);
    void resolveAssignmentId(ParseNode pn);
    void resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs);
    bool resolvePotentialIdSub(ParseNode pn);
    template <bool allow_imp_mult = false> void resolveReference(ParseNode pn);
    void resolveReference(ParseNode pn, const Typeset::Selection& c, size_t sym_id);
    void resolveIdMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right);
    void resolveScriptMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right);
    void resolveConditional1(const Typeset::Selection& name, ParseNode pn);
    void resolveConditional2(ParseNode pn);
    void resolveFor(ParseNode pn);
    void resolveBody(const Typeset::Selection& name, ParseNode pn);
    void resolveBlock(ParseNode pn);
    void resolveDefault(ParseNode pn);
    void resolveLambda(ParseNode pn);
    void resolveAlgorithm(ParseNode pn);
    void resolvePrototype(ParseNode pn);
    void resolveSubscript(ParseNode pn);
    void resolveBig(ParseNode pn);
    bool defineLocalScope(ParseNode pn, bool immutable = true);
    bool declared(ParseNode pn) const noexcept;

    struct Closure {
        ParseNode fn;
        std::unordered_map<size_t, size_t> upvalue_indices;
        size_t num_upvalues = 0;
        std::vector<std::pair<size_t, bool> > upvalues;
        std::vector<size_t> captured;

        Closure(){}
        Closure(ParseNode fn) : fn(fn) {}
    };
};

}

}

#endif // HOPE_SYMBOL_BUILD_PASS_H
