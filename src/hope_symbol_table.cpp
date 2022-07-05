#include "hope_symbol_table.h"

#include "hope_parse_tree.h"
#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace Hope {

namespace Code {

Symbol::Symbol() noexcept
    : declaration_lexical_depth(0) {}

Symbol::Symbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed_var, bool is_const) noexcept
    : declaration_lexical_depth(lexical_depth),
      declaration_closure_depth(closure_depth),
      flag(pn),
      shadowed_var(shadowed_var),
      is_const(is_const) {}

size_t Symbol::closureIndex() const noexcept{
    assert(declaration_closure_depth != 0);
    return declaration_closure_depth - 1;
}

const Typeset::Selection& Symbol::sel(const ParseTree& parse_tree) const noexcept{
    return parse_tree.getSelection(flag);
}

Usage::Usage() noexcept
    : var_id(NONE), type(DECLARE) {}

Usage::Usage(size_t var_id, ParseNode pn, UsageType type) noexcept
    : var_id(var_id), pn(pn), type(type) {}

ScopeSegment::ScopeSegment(
        #ifdef HOPE_USE_SCOPE_NAME
        size_t name_start, size_t name_size,
        #endif
        const Typeset::Marker &start, ParseNode closure, ScopeId parent, ScopeId prev, SymbolId sym_begin, size_t usage_begin) noexcept
    :
      #ifdef HOPE_USE_SCOPE_NAME
      name_start(name_start), name_size(name_size),
      #endif
      start(start), fn(closure), parent(parent), prev(prev), sym_begin(sym_begin), usage_begin(usage_begin) {}

bool ScopeSegment::isStartOfScope() const noexcept{
    return prev == NONE;
}

bool ScopeSegment::isEndOfScope() const noexcept{
    return next == NONE;
}

void SymbolTable::addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except {
    occurence_to_symbol_map[parse_tree.getLeft(pn)] = symbols.size();
    size_t comment = parse_tree.getFlag(pn);
    parse_tree.setFlag(pn, symbols.size());
    symbols.push_back(Symbol(pn, lexical_depth, closure_depth, shadowed, is_const));
    symbols.back().comment = comment;
}

void SymbolTable::addOccurence(const Typeset::Marker& left, size_t sym_index) alloc_except {
    occurence_to_symbol_map[left] = sym_index;
}

size_t SymbolTable::containingScope(const Typeset::Marker& m) const noexcept{
    auto search_fn = [](const ScopeSegment& scope, const Typeset::Marker& m){
        return scope.start.precedesInclusive(m);
    };
    auto lookup = std::lower_bound(scopes.begin(), scopes.end(), m, search_fn);
    return lookup - scopes.begin() - (lookup != scopes.begin());
}

std::vector<Typeset::Selection> SymbolTable::getSuggestions(const Typeset::Marker& loc) const{
    HOPE_UNORDERED_SET<Typeset::Selection> suggestions;

    Typeset::Marker left = loc;
    while(left.atTextStart()){
        if(left.atFirstTextInPhrase()) return std::vector<Typeset::Selection>();
        left.text = left.text->prevTextAsserted();
        left.index = left.text->numChars();
    }
    left.decrementToPrevWord();
    Typeset::Selection typed(left, loc);

    for(size_t i = containingScope(loc); i != NONE; i = scopes[i].parent){
        for(size_t j = i; j != NONE; j = scopes[j].prev){
            const ScopeSegment& seg = scopes[j];
            for(size_t k = seg.sym_begin; k < seg.sym_end; k++){
                const Typeset::Selection& candidate = parse_tree.getSelection(symbols[k].flag);
                if(candidate.startsWith(typed) && candidate.right.precedesInclusive(loc) && candidate.right != loc)
                    suggestions.insert(candidate);

                //EVENTUALLY: filter suggestions based on type so suggestions are always context appropriate
                //            don't use a trie or more specialised data structure unless necessary
            }
        }
    }

    std::vector<Typeset::Selection> sorted;
    sorted.insert(sorted.end(), suggestions.begin(), suggestions.end());
    auto comp = [](const Typeset::Selection& a, const Typeset::Selection& b){
        return a.str() < b.str();
    };
    std::sort(sorted.begin(), sorted.end(), comp);

    return sorted;
}

const Typeset::Selection& SymbolTable::getSel(size_t sym_index) const noexcept{
    return symbols[sym_index].sel(parse_tree);
}

void SymbolTable::getSymbolOccurences(const Typeset::Marker& loc, std::vector<Typeset::Selection>& found) const {
    found.clear();
    auto lookup = occurence_to_symbol_map.find(loc);
    if(lookup == occurence_to_symbol_map.end()) return;

    size_t sym_id = lookup->second;
    getSymbolOccurences(sym_id, found);
}

void SymbolTable::getSymbolOccurences(size_t sym_id, std::vector<Typeset::Selection>& found) const {
    for(const Usage& usage : usages)
        if(usage.var_id == sym_id)
            found.push_back(parse_tree.getSelection(usage.pn));
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

    #if !defined(NDEBUG) && !defined(HOPE_TYPESET_HEADLESS)
    scope_names = "Global";
    #endif

    scopes.push_back(ScopeSegment(SCOPE_NAME(0) SCOPE_NAME(scope_names.size()) doc_start, NONE, NONE, NONE, symbols.size(), usages.size()));
}

void SymbolTable::addScope(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& start, ParseNode closure) alloc_except {
    ScopeSegment& active_scope = scopes.back();
    active_scope.sym_end = symbols.size();
    active_scope.usage_end = usages.size();
    scopes.push_back(ScopeSegment(SCOPE_NAME(scope_names.size()) SCOPE_NAME(name.size()) start, closure, scopes.size()-1, NONE, symbols.size(), usages.size()));
    #ifdef HOPE_USE_SCOPE_NAME
    scope_names += name;
    #endif
}

void SymbolTable::closeScope(const Typeset::Marker& stop) alloc_except {
    ScopeSegment& closed_scope = scopes.back();
    closed_scope.sym_end = symbols.size();
    closed_scope.usage_end = usages.size();
    closed_scope.next = NONE;

    size_t prev_index = closed_scope.parent;
    ScopeSegment& prev_scope = scopes[prev_index];
    prev_scope.next = scopes.size();

    scopes.push_back(ScopeSegment(SCOPE_NAME(prev_scope.name_start) SCOPE_NAME(prev_scope.name_size) stop, prev_scope.fn, prev_scope.parent, prev_index, symbols.size(), usages.size()));
}

void SymbolTable::finalize() noexcept {
    ScopeSegment& scope = scopes.back();
    scope.next = NONE;
    scope.sym_end = symbols.size();
    scope.usage_end = usages.size();
}

}

}
