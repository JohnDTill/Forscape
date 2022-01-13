#include "hope_scope_tree.h"

#include <cassert>

namespace Hope {

namespace Code {

Symbol::Symbol()
    : declaration_lexical_depth(0) {}

Symbol::Symbol(size_t pn, Typeset::Selection first_occurence, size_t lexical_depth, size_t closure_depth, bool is_const)
    : declaration_lexical_depth(lexical_depth),
      declaration_closure_depth(closure_depth),
      flag(pn),
      is_const(is_const) {
    document_occurences.push_back(first_occurence);
}

size_t Symbol::closureIndex() const noexcept{
    assert(declaration_closure_depth != 0);
    return declaration_closure_depth - 1;
}

Usage::Usage()
    : var_id(NONE), type(DECLARE) {}

Usage::Usage(size_t var_id, ParseNode pn, UsageType type)
    : var_id(var_id), pn(pn), type(type) {}

ScopeSegment::ScopeSegment(const Typeset::Selection &name, const Typeset::Marker &start, ParseNode closure, ScopeId parent, ScopeId prev, SymbolId sym_begin, size_t usage_begin)
    : name(name), start(start), closure(closure), parent(parent), prev(prev), sym_begin(sym_begin), usage_begin(usage_begin) {}

bool ScopeSegment::isStartOfScope() const noexcept{
    return prev == NONE;
}

bool ScopeSegment::isEndOfScope() const noexcept{
    return next == NONE;
}

ScopeId SymbolTable::head(ScopeId index) const noexcept{
    size_t prev = scopes[index].prev;
    while(prev != NONE){
        index = prev;
        prev = scopes[index].prev;
    }
    return index;
}

void SymbolTable::reset(const Typeset::Marker &doc_start) noexcept{
    scopes.clear();
    symbols.clear();
    occurence_to_symbol_map.clear();
    usages.clear();

    Typeset::Selection sel(&global_name, 0, 6);
    scopes.push_back(ScopeSegment(sel, doc_start, NONE, NONE, NONE, symbols.size(), usages.size()));
}

void SymbolTable::addScope(const Typeset::Selection &name, const Typeset::Marker &start, ParseNode closure){
    ScopeSegment& active_scope = scopes.back();
    active_scope.sym_end = symbols.size();
    active_scope.usage_end = usages.size();
    scopes.push_back(ScopeSegment(name, start, closure, scopes.size()-1, NONE, symbols.size(), usages.size()));
}

void SymbolTable::closeScope(const Typeset::Marker &stop){
    ScopeSegment& closed_scope = scopes.back();
    closed_scope.sym_end = symbols.size();
    closed_scope.usage_end = usages.size();
    closed_scope.next = NONE;

    size_t prev_index = closed_scope.parent;
    ScopeSegment& prev_scope = scopes[prev_index];
    prev_scope.next = scopes.size();

    scopes.push_back(ScopeSegment(prev_scope.name, stop, prev_scope.closure, prev_scope.parent, prev_index, symbols.size(), usages.size()));
}

void SymbolTable::finalize(){
    ScopeSegment& scope = scopes.back();
    scope.next = NONE;
    scope.sym_end = symbols.size();
    scope.usage_end = usages.size();
}

}

}
