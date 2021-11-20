//This pass is responsible to group related identifiers into symbols,
//and gather any data the GUI needs for usages, rename symbol, context-aware autocomplete, etc.
//This pass should NOT handle any details of how variables will be resolved at runtime.
//   The AST is subject to a great deal of transformation, so gathering data now would be premature.

#ifndef HOPE_SYMBOL_BUILD_PASS_H
#define HOPE_SYMBOL_BUILD_PASS_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "typeset_selection.h"
#include <unordered_map>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

class ParseTree;
typedef std::unordered_map<Typeset::Marker, std::vector<Typeset::Selection>*> IdMap;

class SymbolTableBuilder{
public:
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model);
    void resolveSymbols();
    IdMap doc_map; //Click a variable and see all references, accounting for scoping

private:
    void link(size_t scope_index = 0);
    std::vector<std::unordered_map<size_t, size_t>> closures;
    std::vector<size_t> closure_size;
    std::vector<size_t> stack_frame;
    size_t stack_size;

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

    void reset() noexcept;

    static const std::unordered_map<std::string_view, ParseNodeType> predef;

    std::vector<Symbol> symbols;
    std::vector<size_t> symbol_id_index;

    static constexpr size_t NONE = std::numeric_limits<size_t>::max();

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

    std::vector<Scope> scopes;

    size_t active_scope_id;

    Scope& activeScope() noexcept{
        return scopes[active_scope_id];
    }

    void addScope(ParseNode closing_function = NONE){
        activeScope().subscopes.back().subscope_id = scopes.size();
        scopes.push_back(Scope(closing_function, active_scope_id));
        active_scope_id = scopes.size()-1;
        activeScope().subscopes.push_back(Scope::Subscope());
    }

    void closeScope() noexcept{
        active_scope_id = activeScope().parent_id;
        if(active_scope_id != NONE)
            activeScope().subscopes.push_back(Scope::Subscope());
    }

    typedef std::vector<size_t> Id;
    typedef size_t IdIndex;
    std::vector<Id> ids;
    std::unordered_map<Typeset::Selection, IdIndex> map;
    std::vector<Error>& errors;

    #ifndef NDEBUG
    void invariants(){
        //A map result always has an active symbol
        for(const auto& entry : map) assert(!ids[entry.second].empty());
    }
    #endif

    Symbol& lastSymbolOfId(const Id& identifier) noexcept{
        return symbols[identifier.back()];
    }

    Symbol& lastSymbolOfId(IdIndex index) noexcept{
        return lastSymbolOfId(ids[index]);
    }

    size_t lastSymbolIndexOfId(IdIndex index) const noexcept{
        return ids[index].back();
    }

    Symbol& lastDefinedSymbol() noexcept{
        return symbols.back();
    }

    Symbol* symbolFromSelection(const Typeset::Selection& sel) noexcept{
        auto lookup = map.find(sel);
        return lookup == map.end() ? nullptr : &lastSymbolOfId(lookup->second);
    }

    size_t symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept{
        auto lookup = map.find(sel);
        return lookup == map.end() ? NONE : lastSymbolIndexOfId(lookup->second);
    }

    void increaseLexicalDepth();
    void decreaseLexicalDepth();
    void increaseClosureDepth(ParseNode pn);
    void decreaseClosureDepth();

    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;

    struct Closure {
        ParseNode fn;
        std::unordered_map<IdIndex, size_t> upvalue_indices;
        size_t num_upvalues = 0;
        std::vector<std::pair<IdIndex, bool> > upvalues;
        std::vector<size_t> captured;

        Closure(){}
        Closure(ParseNode fn) : fn(fn) {}
    };

    size_t getUpvalueIndex(IdIndex value, size_t closure_index);

    size_t closure_depth = 0;

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

    ParseTree& parse_tree;
};

}

}

#endif // HOPE_SYMBOL_BUILD_PASS_H
