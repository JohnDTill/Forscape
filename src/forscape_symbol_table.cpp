#include "forscape_symbol_table.h"

#include "forscape_dynamic_settings.h"
#include "forscape_parse_tree.h"
#include "forscape_program.h"
#include "forscape_scanner.h"
#include "forscape_symbol_lexical_pass.h"
#include <algorithm>
#include <cassert>
#include <unordered_set>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

#ifndef NDEBUG
std::unordered_set<const Symbol*> SymbolTable::all_symbols;

bool SymbolTable::isValidSymbolPtr(const Symbol* const ptr) noexcept {
    return all_symbols.find(ptr) != all_symbols.cend() || ptr == nullptr;
}

SymbolTable::~SymbolTable() noexcept {
    for(Symbol& sym : symbols) all_symbols.erase(&sym);
}
#endif

Symbol::Symbol() noexcept
    : declaration_lexical_depth(0) {}

Symbol::Symbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed_var, bool is_const) noexcept
    : index_of_shadowed_var(shadowed_var),
      declaration_lexical_depth(lexical_depth),
      declaration_closure_depth(closure_depth),
      use_level(Program::instance()->settings.warningLevel<WARN_UNUSED_VARIABLE>()),
      flag(pn),
      is_const(is_const) {}

size_t Symbol::closureIndex() const noexcept {
    assert(declaration_closure_depth != 0);
    return declaration_closure_depth - 1;
}

const Typeset::Selection& Symbol::sel(const ParseTree& parse_tree) const noexcept {
    return parse_tree.getSelection(flag);
}

const Typeset::Selection& Symbol::firstOccurence() const noexcept {
    SymbolUsage* usage = lastUsage();
    while(SymbolUsage* prev = usage->prevUsage()) usage = prev;
    return usage->sel;
}

const Typeset::Selection& Symbol::sel() const noexcept {
    return lastUsage()->sel;
}

std::string Symbol::str() const noexcept {
    return sel().str();
}

void Symbol::getLocalOccurences(std::vector<Typeset::Selection>& found) const {
    for(SymbolUsage* usage = lastUsage(); usage != nullptr; usage = usage->prevUsage())
        found.push_back(usage->sel);
}

void Symbol::getExternalOccurences(std::vector<Typeset::Selection>& found) const {
    for(SymbolUsage* usage = last_external_usage; usage != lastUsage(); usage = usage->prevUsage())
        found.push_back(usage->sel);
}

void Symbol::getAllOccurences(std::vector<Typeset::Selection>& found) const {
    for(SymbolUsage* usage = last_external_usage; usage != nullptr; usage = usage->prevUsage())
        found.push_back(usage->sel);
}

void Symbol::getModelOccurences(std::vector<Typeset::Selection>& found, Typeset::Model* model) const {
    for(SymbolUsage* usage = last_external_usage; usage != nullptr; usage = usage->prevUsage())
        if(usage->sel.getModel() == model)
            found.push_back(usage->sel);
}

SymbolUsage::SymbolUsage() noexcept
    : prev_usage_index(NONE), symbol_index(NONE) {}

SymbolUsage::SymbolUsage(
        SymbolUsageIndex prev_usage_index,
        SymbolIndex symbol_index,
        ParseNode pn,
        const Typeset::Selection& sel) noexcept
    : sel(sel), prev_usage_index(prev_usage_index), symbol_index(symbol_index), pn(pn) {}

ScopeSegment::ScopeSegment(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        size_t name_start, size_t name_size,
        #endif
        const Typeset::Marker& start, ParseNode closure, ScopeSegmentIndex parent, ScopeSegmentIndex prev, SymbolIndex sym_begin, size_t usage_begin) noexcept
    :
      #ifdef FORSCAPE_USE_SCOPE_NAME
      name_start(name_start), name_size(name_size),
      #endif
      start_of_selection(start), fn(closure), parent_lexical_segment_index(parent), prev_lexical_segment_index(prev), first_sym_index(sym_begin), usage_begin(usage_begin) {}

bool ScopeSegment::isStartOfScope() const noexcept {
    return prev_lexical_segment_index == NONE;
}

void SymbolTable::addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except {
    size_t comment = parse_tree.getFlag(pn);
    if(comment == 0) comment = NONE; //EVENTUALLY: this should be initialised to NONE
    parse_tree.setSymId(pn, symbols.size());
    symbols.push_back(Symbol(pn, lexical_depth, closure_depth, shadowed, is_const));
    symbols.back().comment = comment;
    symbols.back().last_usage_index = symbol_usages.size();

    symbol_usages.push_back(SymbolUsage(NONE, symbols.size()-1, pn, parse_tree.getSelection(pn)));
}

size_t SymbolTable::containingScope(const Typeset::Marker& m) const noexcept {
    auto search_fn = [](const ScopeSegment& scope, const Typeset::Marker& m){
        return scope.start_of_selection.precedesInclusive(m);
    };
    auto lookup = std::lower_bound(scope_segments.begin(), scope_segments.end(), m, search_fn);
    return lookup - scope_segments.begin() - (lookup != scope_segments.begin());
}

void SymbolTable::getSuggestions(const Typeset::Marker& loc, std::vector<std::string>& suggestions) const {
    FORSCAPE_UNORDERED_SET<std::string> suggestion_set;

    Typeset::Marker left = loc;
    while(left.atTextStart()){
        if(left.atFirstTextInPhrase()) return;
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
                const Typeset::Selection& candidate = symbols[k].sel();
                if(candidate.startsWith(typed) && candidate.right.precedesInclusive(loc) && candidate.right != loc)
                    suggestion_set.insert(candidate.str());

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
                suggestion_set.insert(std::string(keyword));
        }
    }

    suggestions.insert(suggestions.end(), suggestion_set.cbegin(), suggestion_set.cend());
    std::sort(suggestions.begin(), suggestions.end());

    //Add matching keywords
    if(typed.isTextSelection()){
        const size_t sorted_size_prior = suggestions.size();
        const std::string_view typed_view = typed.strView();
        for(const auto& entry : Scanner::keywords){
            const auto& keyword = entry.first;
            if(keyword.size() >= typed_view.size() && std::string_view(keyword.data(), typed_view.size()) == typed_view)
                suggestions.push_back(std::string(keyword));
        }
        std::sort(suggestions.begin()+sorted_size_prior, suggestions.end());
    }
}

const Typeset::Selection& SymbolTable::getSel(size_t sym_index) const noexcept {
    return symbols[sym_index].sel(parse_tree);
}

void SymbolTable::reset(const Typeset::Marker& doc_start) noexcept {
    #ifndef NDEBUG
    for(Symbol& sym : symbols) all_symbols.erase(&sym);
    #endif

    scope_segments.clear();
    symbols.clear();
    symbol_usages.clear();
    scoped_vars.clear();

    #if !defined(NDEBUG) && !defined(FORSCAPE_TYPESET_HEADLESS)
    scope_names = "Global";
    #endif

    scope_segments.push_back(ScopeSegment(SCOPE_NAME(0) SCOPE_NAME(scope_names.size()) doc_start, NONE, NONE, NONE, symbols.size(), symbol_usages.size()));
}

void SymbolTable::addScope(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& start, ParseNode closure) alloc_except {
    ScopeSegment& active_scope = scope_segments.back();
    active_scope.usage_end = symbol_usages.size();
    scope_segments.push_back(ScopeSegment(SCOPE_NAME(scope_names.size()) SCOPE_NAME(name.size()) start, closure, scope_segments.size()-1, NONE, symbols.size(), symbol_usages.size()));
    #ifdef FORSCAPE_USE_SCOPE_NAME
    scope_names += name;
    #endif
}

void SymbolTable::closeScope(const Typeset::Marker& stop) alloc_except {
    ScopeSegment& closed_scope = scope_segments.back();
    closed_scope.usage_end = symbol_usages.size();
    closed_scope.is_end_of_scope = true;

    size_t prev_index = closed_scope.parent_lexical_segment_index;
    ScopeSegment& prev_scope = scope_segments[prev_index];

    scope_segments.push_back(ScopeSegment(SCOPE_NAME(prev_scope.name_start) SCOPE_NAME(prev_scope.name_size) stop, prev_scope.fn, prev_scope.parent_lexical_segment_index, prev_index, symbols.size(), symbol_usages.size()));
}

void SymbolTable::finalize() noexcept {
    ScopeSegment& scope = scope_segments.back();
    scope.is_end_of_scope = true;
    scope.usage_end = symbol_usages.size();

    #ifndef NDEBUG
    for(Symbol& sym : symbols) all_symbols.insert(&sym);
    #endif

    const Symbol* const symbol_offset = symbols.data();
    const SymbolUsage* const usage_offset = symbol_usages.data();
    for(Symbol& sym : symbols){
        sym.last_usage_index = reinterpret_cast<SymbolUsageIndex>(usage_offset + sym.last_usage_index);
        sym.index_of_shadowed_var = reinterpret_cast<SymbolIndex>(symbol_offset + sym.index_of_shadowed_var);
        sym.last_external_usage = sym.lastUsage();
    }
    for(SymbolUsage& usage : symbol_usages){
        if(usage.prev_usage_index == NONE) usage.prev_usage_index = reinterpret_cast<SymbolUsageIndex>(nullptr);
        else usage.prev_usage_index = reinterpret_cast<SymbolUsageIndex>(usage_offset + usage.prev_usage_index);

        //EVENTUALLY: possibility of nullptr is a concession to current namespace scheme
        if(usage.symbol_index == NONE) usage.symbol_index = reinterpret_cast<SymbolIndex>(nullptr);
        else usage.symbol_index = reinterpret_cast<SymbolIndex>(symbol_offset + usage.symbol_index);

        if(usage.symbol())
            parse_tree.setSymbol(usage.pn, usage.symbol());
        else
            parse_tree.setFlag(usage.pn, reinterpret_cast<size_t>(&usage));
    }
    for(auto& entry : lexical_map) entry.second = reinterpret_cast<SymbolIndex>(symbol_offset + entry.second);

    //EVENTUALLY: this is nested alloc, but the current namespace scheme can probably be erased
    //            Likely make this decision when implementing OOP
    FORSCAPE_UNORDERED_MAP<ScopedVarKey, SymbolIndex, HashScopedVarKey> scoped_vars;
    for(const auto& entry : this->scoped_vars){
        auto key = entry.first;
        key.symbol_index = reinterpret_cast<SymbolIndex>(symbol_offset + key.symbol_index);
        scoped_vars[key] = reinterpret_cast<SymbolIndex>(symbol_offset + entry.second);
    }
    this->scoped_vars = scoped_vars;
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

void SymbolTable::resolveReference(ParseNode pn, size_t sym_id, size_t closure_depth) alloc_except {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    Symbol& sym = symbols[sym_id];
    parse_tree.setSymId(pn, sym_id);
    sym.is_used = true;

    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    symbol_usages.push_back(SymbolUsage(sym.last_usage_index, sym_id, pn, parse_tree.getSelection(pn)));
    sym.last_usage_index = symbol_usages.size()-1;
}

}

}
