#ifndef HOPE_SYMBOL_BUILD_PASS_H
#define HOPE_SYMBOL_BUILD_PASS_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "typeset_selection.h"
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

static constexpr size_t NONE = std::numeric_limits<size_t>::max();

struct Symbol{
    struct Access{
        ParseNode pn;
        size_t closure_depth;

        Access(ParseNode pn, size_t closure_depth) :
            pn(pn), closure_depth(closure_depth) {}
    };

    std::vector<Typeset::Selection>* document_occurences;
    size_t declaration_lexical_depth;
    size_t declaration_closure_depth;
    size_t stack_index;
    bool is_const;
    bool is_used = false;
    bool is_reassigned = false; //Used to determine if parameters are constant
    bool is_closure_nested = false;
    bool is_prototype = false;
    bool is_ewise_index = false;
    bool is_captured = false;

    Symbol()
        : declaration_lexical_depth(0) {}

    Symbol(Typeset::Selection first_occurence,
               size_t lexical_depth,
               size_t closure_depth,
               bool is_const)
        : declaration_lexical_depth(lexical_depth),
          declaration_closure_depth(closure_depth),
          is_const(is_const) {
        document_occurences = new std::vector<Typeset::Selection>();
        document_occurences->push_back(first_occurence);
    }

    size_t closureIndex() const noexcept{
        assert(declaration_closure_depth != 0);
        return declaration_closure_depth - 1;
    }
};

struct Scope{
    enum UsageType{
        DECLARE,
        REASSIGN,
        READ,
    };

    struct Usage{
        size_t var_id;
        ParseNode pn;
        UsageType type;

        Usage()
            : var_id(NONE), type(DECLARE) {}

        Usage(size_t var_id, ParseNode pn, UsageType type)
            : var_id(var_id), pn(pn), type(type) {}
    };

    struct Subscope{
        std::vector<Usage> usages;
        size_t subscope_id = NONE;
    };

    ParseNode closing_function;
    size_t parent_id;
    std::vector<Subscope> subscopes;

    Scope(ParseNode closing_function, size_t parent_id)
        : closing_function(closing_function), parent_id(parent_id) {
        subscopes.push_back(Subscope());
    }
};

struct SymbolTable{
    std::vector<Symbol> symbols;
    std::vector<Scope> scopes;
};

class ParseTree;
typedef std::unordered_map<Typeset::Marker, std::vector<Typeset::Selection>*> IdMap;

class SymbolTableBuilder{
public:
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model);
    void resolveSymbols();
    IdMap doc_map; //Click a variable and see all references, accounting for scoping
    SymbolTable symbol_table;

private:
    static const std::unordered_map<std::string_view, Op> predef;
    std::vector<size_t> symbol_id_index;
    size_t active_scope_id;
    typedef std::vector<size_t> Id;
    typedef size_t IdIndex;
    std::vector<Id> ids;
    std::unordered_map<Typeset::Selection, IdIndex> map;
    std::vector<Error>& errors;
    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;
    size_t closure_depth = 0;
    ParseTree& parse_tree;

    void reset() noexcept;
    Scope& activeScope() noexcept;
    void addScope(ParseNode closing_function = NONE);
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
    void finalize(const Symbol& sym_info);
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
