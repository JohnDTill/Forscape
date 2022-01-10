#ifndef HOPE_SCOPE_TREE_H
#define HOPE_SCOPE_TREE_H

#include "typeset_selection.h"
#include "typeset_text.h"
#include <limits>
#include <vector>

namespace Hope {

namespace Code {

static constexpr size_t NONE = std::numeric_limits<size_t>::max();
typedef size_t ParseNode;
typedef size_t ScopeId;
typedef size_t SymbolId;

struct Symbol{
    std::vector<Typeset::Selection> document_occurences; //DO THIS - eliminate nested vector
    size_t declaration_lexical_depth;
    size_t declaration_closure_depth;
    size_t flag;
    bool is_const;
    bool is_used = false;
    bool is_reassigned = false; //Used to determine if parameters are constant
    bool is_closure_nested = false;
    bool is_prototype = false;
    bool is_ewise_index = false;
    bool is_captured = false;

    Symbol()
        : declaration_lexical_depth(0) {}

    Symbol(size_t pn,
           Typeset::Selection first_occurence,
           size_t lexical_depth,
           size_t closure_depth,
           bool is_const)
        : declaration_lexical_depth(lexical_depth),
          declaration_closure_depth(closure_depth),
          flag(pn),
          is_const(is_const) {
        document_occurences.push_back(first_occurence);
    }

    size_t closureIndex() const noexcept{
        assert(declaration_closure_depth != 0);
        return declaration_closure_depth - 1;
    }
};

struct Scope{
    Typeset::Selection name;
    Typeset::Marker start;
    ParseNode closure;
    ScopeId parent;
    ScopeId prev;
    SymbolId sym_begin;
    ScopeId next = NONE;
    SymbolId sym_end = NONE;

    Scope(
        const Typeset::Selection& name,
        const Typeset::Marker& doc_start,
        ParseNode closure,
        ScopeId parent,
        ScopeId prev,
        SymbolId sym_begin
        )
        : name(name), start(doc_start), closure(closure), parent(parent), prev(prev), sym_begin(sym_begin) {}
};

class SymbolTable{
public:
    Typeset::Text global_name;
    Typeset::Text lambda;

public:
    std::vector<Scope> scopes;
    std::vector<Symbol> symbols;
    std::unordered_map<Typeset::Marker, SymbolId> occurence_to_symbol_map;

    SymbolTable(){
        global_name.str = "Global";
        global_name.str = "lambda";
    }

    void reset(const Typeset::Marker& doc_start) noexcept{
        scopes.clear();
        symbols.clear();
        occurence_to_symbol_map.clear();

        Typeset::Selection sel(&global_name, 0, 6);
        scopes.push_back(Scope(sel, doc_start, NONE, NONE, NONE, symbols.size()));
    }

    void addScope(const Typeset::Selection& name, const Typeset::Marker& start, ParseNode closure = NONE){
        Scope& active_scope = scopes.back();
        active_scope.sym_end = symbols.size();
        scopes.push_back(Scope(name, start, closure, scopes.size()-1, NONE, symbols.size()));
    }

    void closeScope(const Typeset::Marker& stop){
        Scope& closed_scope = scopes.back();
        closed_scope.sym_end = symbols.size();
        closed_scope.next = NONE;

        size_t prev_index = closed_scope.parent;
        Scope& prev_scope = scopes[prev_index];
        prev_scope.next = scopes.size();

        scopes.push_back(Scope(prev_scope.name, stop, prev_scope.closure, prev_scope.parent, prev_index, symbols.size()));
    }
};

}

}

#endif // HOPE_SCOPE_TREE_H
