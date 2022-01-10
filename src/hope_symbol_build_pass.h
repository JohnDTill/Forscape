#ifndef HOPE_SYMBOL_BUILD_PASS_H
#define HOPE_SYMBOL_BUILD_PASS_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_scope_tree.h"
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

class ParseTree;

class SymbolTableBuilder{
public:
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model);
    void resolveSymbols();

private:
    static const std::unordered_map<std::string_view, Op> predef;
    std::vector<size_t> symbol_id_index;
    size_t active_scope_id;
    typedef std::vector<size_t> Id;
    typedef size_t IdIndex;
    std::vector<Id> ids; //DO THIS - eliminate nested vector
    std::unordered_map<Typeset::Selection, IdIndex> map;
    std::vector<Error>& errors;
    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;
    size_t closure_depth = 0;
    ParseTree& parse_tree;
    SymbolTable& symbol_table;

    void reset() noexcept;
    Scope& activeScope() noexcept;
    void addScope();
    void closeScope() noexcept;
    Symbol& lastSymbolOfId(const Id& identifier) noexcept;
    Symbol& lastSymbolOfId(IdIndex index) noexcept;
    size_t lastSymbolIndexOfId(IdIndex index) const noexcept;
    Symbol& lastDefinedSymbol() noexcept;
    Symbol* symbolFromSelection(const Typeset::Selection& sel) noexcept;
    size_t symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept;
    void increaseLexicalDepth();
    void decreaseLexicalDepth();
    void increaseClosureDepth(ParseNode pn);
    void decreaseClosureDepth();
    size_t getUpvalueIndex(IdIndex value, size_t closure_index);
    void finalize(size_t sym_id);
    void makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable);
    void appendEntry(size_t index, const Typeset::Selection& c, ParseNode pn, bool immutable);
    void resolveStmt(ParseNode pn);
    void resolveExpr(ParseNode pn);
    void resolveEquality(ParseNode pn);
    void resolveAssignment(ParseNode pn);
    void resolveAssignmentId(ParseNode pn);
    void resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs);
    bool resolvePotentialIdSub(ParseNode pn);
    void resolveReference(ParseNode pn);
    void resolveReference(ParseNode pn, const Typeset::Selection& c, size_t sym_id);
    void resolveConditional1(ParseNode pn);
    void resolveConditional2(ParseNode pn);
    void resolveFor(ParseNode pn);
    void resolveBody(ParseNode pn);
    void resolveBlock(ParseNode pn);
    void resolveDefault(ParseNode pn);
    void resolveLambda(ParseNode pn);
    void resolveAlgorithm(ParseNode pn);
    void resolvePrototype(ParseNode pn);
    void resolveSubscript(ParseNode pn);
    void resolveBig(ParseNode pn);
    bool defineLocalScope(ParseNode pn, bool immutable = true);

    struct Closure {
        ParseNode fn;
        std::unordered_map<IdIndex, size_t> upvalue_indices;
        size_t num_upvalues = 0;
        std::vector<std::pair<IdIndex, bool> > upvalues;
        std::vector<size_t> captured;

        Closure(){}
        Closure(ParseNode fn) : fn(fn) {}
    };
};

}

}

#endif // HOPE_SYMBOL_BUILD_PASS_H
