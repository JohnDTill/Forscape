#include "forscape_symbol_table.h"

#include "forscape_parse_tree.h"
#include "forscape_scanner.h"
#include "forscape_symbol_lexical_pass.h"
#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace Forscape {

namespace Code {

Symbol::Symbol() noexcept
    : declaration_lexical_depth(0) {}

Symbol::Symbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed_var, bool is_const) noexcept
    : declaration_lexical_depth(lexical_depth),
      declaration_closure_depth(closure_depth),
      flag(pn),
      index_of_shadowed_var(shadowed_var),
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
        #ifdef FORSCAPE_USE_SCOPE_NAME
        size_t name_start, size_t name_size,
        #endif
        const Typeset::Marker& start, ParseNode closure, ScopeSegmentIndex parent, ScopeSegmentIndex prev, SymbolIndex sym_begin, size_t usage_begin) noexcept
    :
      #ifdef FORSCAPE_USE_SCOPE_NAME
      name_start(name_start), name_size(name_size),
      #endif
      start(start), fn(closure), parent_lexical_segment_index(parent), prev_lexical_segment_index(prev), first_sym_index(sym_begin), usage_begin(usage_begin) {}

bool ScopeSegment::isStartOfScope() const noexcept{
    return prev_lexical_segment_index == NONE;
}

void SymbolTable::addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except {
    size_t comment = parse_tree.getFlag(pn);
    if(comment == 0) comment = NONE; //EVENTUALLY: this should be initialised to NONE
    parse_tree.setSymId(pn, symbols.size());
    symbols.push_back(Symbol(pn, lexical_depth, closure_depth, shadowed, is_const));
    symbols.back().comment = comment;
}

size_t SymbolTable::containingScope(const Typeset::Marker& m) const noexcept{
    auto search_fn = [](const ScopeSegment& scope, const Typeset::Marker& m){
        return scope.start.precedesInclusive(m);
    };
    auto lookup = std::lower_bound(scope_segments.begin(), scope_segments.end(), m, search_fn);
    return lookup - scope_segments.begin() - (lookup != scope_segments.begin());
}

std::vector<std::string> SymbolTable::getSuggestions(const Typeset::Marker& loc) const{
    FORSCAPE_UNORDERED_SET<std::string> suggestions;

    Typeset::Marker left = loc;
    while(left.atTextStart()){
        if(left.atFirstTextInPhrase()) return std::vector<std::string>();
        left.text = left.text->prevTextAsserted();
        left.index = left.text->numChars();
    }
    left.decrementToPrevWord();
    Typeset::Selection typed(left, loc);

    //Add matching words in scope
    for(size_t i = containingScope(loc); i != NONE; i = scope_segments[i].parent_lexical_segment_index){
        for(size_t j = i; j != NONE; j = scope_segments[j].prev_lexical_segment_index){
            const ScopeSegment& seg = scope_segments[j];
            const size_t end = (&seg == &scope_segments.back()) ? symbols.size() : scope_segments[j+1].first_sym_index;
            for(size_t k = seg.first_sym_index; k < end; k++){
                const Typeset::Selection& candidate = parse_tree.getSelection(symbols[k].flag);
                if(candidate.startsWith(typed) && candidate.right.precedesInclusive(loc) && candidate.right != loc)
                    suggestions.insert(candidate.str());

                //EVENTUALLY: filter suggestions based on type so suggestions are always context appropriate
                //            don't use a trie or more specialised data structure unless necessary
            }
        }
    }

    //Add predefined variables
    if(typed.isTextSelection()){
        const std::string_view typed_view = typed.strView();
        for(const auto& entry : SymbolLexicalPass::predef){
            const auto& keyword = entry.first;
            if(keyword.size() >= typed_view.size() && std::string_view(keyword.data(), typed_view.size()) == typed_view)
                suggestions.insert(std::string(keyword));
        }
    }

    std::vector<std::string> sorted;
    sorted.insert(sorted.end(), suggestions.cbegin(), suggestions.cend());
    std::sort(sorted.begin(), sorted.end());

    //Add matching keywords
    if(typed.isTextSelection()){
        const size_t sorted_size_prior = sorted.size();
        const std::string_view typed_view = typed.strView();
        for(const auto& entry : Scanner::keywords){
            const auto& keyword = entry.first;
            if(keyword.size() >= typed_view.size() && std::string_view(keyword.data(), typed_view.size()) == typed_view)
                sorted.push_back(std::string(keyword));
        }
        std::sort(sorted.begin()+sorted_size_prior, sorted.end());
    }

    return sorted;
}

const Typeset::Selection& SymbolTable::getSel(size_t sym_index) const noexcept{
    return symbols[sym_index].sel(parse_tree);
}

void SymbolTable::getSymbolOccurences(size_t sym_id, std::vector<Typeset::Selection>& found) const {
    for(const Usage& usage : usages)
        if(usage.var_id == sym_id)
            found.push_back(parse_tree.getSelection(usage.pn));
}

ScopeSegmentIndex SymbolTable::head(ScopeSegmentIndex index) const noexcept{
    size_t prev = scope_segments[index].prev_lexical_segment_index;
    while(prev != NONE){
        index = prev;
        prev = scope_segments[index].prev_lexical_segment_index;
    }
    return index;
}

void SymbolTable::reset(const Typeset::Marker& doc_start) noexcept{
    scope_segments.clear();
    symbols.clear();
    usages.clear();
    scoped_vars.clear();

    #if !defined(NDEBUG) && !defined(FORSCAPE_TYPESET_HEADLESS)
    scope_names = "Global";
    #endif

    scope_segments.push_back(ScopeSegment(SCOPE_NAME(0) SCOPE_NAME(scope_names.size()) doc_start, NONE, NONE, NONE, symbols.size(), usages.size()));
}

void SymbolTable::addScope(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& start, ParseNode closure) alloc_except {
    ScopeSegment& active_scope = scope_segments.back();
    active_scope.usage_end = usages.size();
    scope_segments.push_back(ScopeSegment(SCOPE_NAME(scope_names.size()) SCOPE_NAME(name.size()) start, closure, scope_segments.size()-1, NONE, symbols.size(), usages.size()));
    #ifdef FORSCAPE_USE_SCOPE_NAME
    scope_names += name;
    #endif
}

void SymbolTable::closeScope(const Typeset::Marker& stop) alloc_except {
    ScopeSegment& closed_scope = scope_segments.back();
    closed_scope.usage_end = usages.size();
    closed_scope.is_end_of_scope = true;

    size_t prev_index = closed_scope.parent_lexical_segment_index;
    ScopeSegment& prev_scope = scope_segments[prev_index];

    scope_segments.push_back(ScopeSegment(SCOPE_NAME(prev_scope.name_start) SCOPE_NAME(prev_scope.name_size) stop, prev_scope.fn, prev_scope.parent_lexical_segment_index, prev_index, symbols.size(), usages.size()));
}

void SymbolTable::finalize() noexcept {
    ScopeSegment& scope = scope_segments.back();
    scope.is_end_of_scope = true;
    scope.usage_end = usages.size();
}

#ifndef NDEBUG
void SymbolTable::verifyIdentifier(ParseNode pn) const noexcept {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    size_t sym_id = parse_tree.getSymId(pn);
    assert(sym_id < symbols.size());
    const Symbol& sym = symbols[sym_id];
    assert(parse_tree.getSelection(pn) == sym.sel(parse_tree));
}
#endif

void SymbolTable::resolveReference(ParseNode pn, size_t sym_id, size_t closure_depth) noexcept {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    Symbol& sym = symbols[sym_id];
    parse_tree.setSymId(pn, sym_id);
    sym.is_used = true;

    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    usages.push_back(Usage(sym_id, pn, READ));
}

}

}
