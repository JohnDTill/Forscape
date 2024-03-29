#ifndef FORSCAPE_SCOPE_TREE_H
#define FORSCAPE_SCOPE_TREE_H

#include <forscape_common.h>
#include "typeset_selection.h"
#include "typeset_text.h"
#include <limits>
#include <vector>

#if !defined(NDEBUG) && !defined(FORSCAPE_TYPESET_HEADLESS)
#define SCOPE_NAME(x) x,
#define FORSCAPE_USE_SCOPE_NAME
#else
#define SCOPE_NAME(x)
#endif

namespace Forscape {

namespace Code {

class ParseTree;

typedef size_t ScopeSegmentIndex;
typedef size_t SymbolIndex;
typedef size_t SymbolUsageIndex;
class SymbolLexicalPass;
class SymbolTable;
struct SymbolUsage;

struct Symbol {
private: friend SymbolLexicalPass; friend SymbolTable; //Fields for the lexical pass
    SymbolIndex index_of_shadowed_var  DEBUG_INIT_UNITIALISED(SymbolIndex);
    SymbolUsageIndex last_usage_index  DEBUG_INIT_UNITIALISED(SymbolUsageIndex);
    uint16_t declaration_lexical_depth  DEBUG_INIT_UNITIALISED(uint16_t);
    size_t previous_namespace_index = NONE;
    const Typeset::Selection& sel(const ParseTree& parse_tree) const noexcept;

public:
    uint8_t declaration_closure_depth  DEBUG_INIT_UNITIALISED(uint8_t);
    uint8_t use_level; //EVENTUALLY: could consolidate with is_used?
    size_t flag  DEBUG_INIT_UNITIALISED(size_t);
    size_t type = NONE;
    size_t rows = UNKNOWN_SIZE;
    size_t cols = UNKNOWN_SIZE;
    ParseNode comment  DEBUG_INIT_UNITIALISED(ParseNode);
    Symbol* aliased_var = nullptr;
    bool is_const;
    bool is_used = false;
    bool is_reassigned = false; //Used to determine if parameters are constant
    bool is_closure_nested = false;
    bool is_ewise_index = false;
    bool is_captured_by_value = false;
    bool tied_to_file = false;

    Symbol() noexcept;
    Symbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed_var, bool is_const) noexcept;
    size_t closureIndex() const noexcept;
    const Typeset::Selection& firstOccurence() const noexcept;
    const Typeset::Selection& sel() const noexcept;
    std::string str() const noexcept;
    SymbolUsage* lastUsage() const noexcept { return reinterpret_cast<SymbolUsage*>(last_usage_index); }
    SymbolUsage* last_external_usage;
    Symbol* shadowedVar() const noexcept { return reinterpret_cast<Symbol*>(index_of_shadowed_var); }
    void setShadowedVar(Symbol* sym) noexcept { index_of_shadowed_var = reinterpret_cast<size_t>(sym); }
    void getLocalOccurences(std::vector<Typeset::Selection>& found) const;
    void getExternalOccurences(std::vector<Typeset::Selection>& found) const;
    void getAllOccurences(std::vector<Typeset::Selection>& found) const;
    void getModelOccurences(std::vector<Typeset::Selection>& found, Typeset::Model* model) const;
};

struct ScopeSegment {
    SymbolIndex first_sym_index  DEBUG_INIT_UNITIALISED(SymbolIndex);
    ScopeSegmentIndex parent_lexical_segment_index  DEBUG_INIT_UNITIALISED(ScopeSegmentIndex);
    ScopeSegmentIndex prev_lexical_segment_index  DEBUG_INIT_UNITIALISED(ScopeSegmentIndex);
    bool is_end_of_scope = false;
    Typeset::Marker start_of_selection;
    ParseNode fn  DEBUG_INIT_UNITIALISED(ParseNode);
    size_t usage_begin  DEBUG_INIT_UNITIALISED(size_t);
    ScopeSegmentIndex prev_namespace_segment  DEBUG_INIT_UNITIALISED(ScopeSegmentIndex);
    size_t usage_end = NONE;

    #ifdef FORSCAPE_USE_SCOPE_NAME
    size_t name_start;
    size_t name_size;
    #endif

    ScopeSegment(
        #if !defined(NDEBUG) && !defined(FORSCAPE_TYPESET_HEADLESS)
        size_t name_start,
        size_t name_size,
        #endif
        const Typeset::Marker& start,
        ParseNode closure,
        ScopeSegmentIndex parent,
        ScopeSegmentIndex prev,
        SymbolIndex sym_begin,
        size_t usage_begin
        ) noexcept;

    bool isStartOfScope() const noexcept;
};

struct SymbolUsage {
    Typeset::Selection sel;
    SymbolUsageIndex prev_usage_index  DEBUG_INIT_UNITIALISED(SymbolUsageIndex);
    SymbolIndex symbol_index  DEBUG_INIT_UNITIALISED(SymbolIndex);
    ParseNode pn  DEBUG_INIT_UNITIALISED(ParseNode);

    SymbolUsage() noexcept;
    SymbolUsage(SymbolUsageIndex prev_usage_index, SymbolIndex symbol_index, ParseNode pn, const Typeset::Selection& sel) noexcept;
    SymbolUsage* prevUsage() const noexcept { return reinterpret_cast<SymbolUsage*>(prev_usage_index); }
    Symbol* symbol() const noexcept { return reinterpret_cast<Symbol*>(symbol_index); }
    bool isDeclaration() const noexcept { return prevUsage() == nullptr; }
};

class SymbolTable{
public:
    std::vector<Symbol> symbols;
    FORSCAPE_UNORDERED_MAP<Typeset::Selection, SymbolIndex> lexical_map;
    std::vector<ScopeSegment> scope_segments;
    std::vector<SymbolUsage> symbol_usages;
    ParseTree& parse_tree;

    #ifndef NDEBUG
    static std::unordered_set<const Symbol*> all_symbols;
    static bool isValidSymbolPtr(const Symbol* const ptr) noexcept;
    ~SymbolTable() noexcept;
    #endif

    #ifdef FORSCAPE_USE_SCOPE_NAME
    std::string scope_names;
    std::string_view getName(const ScopeSegment& scope) const noexcept {
        return std::string_view(scope_names.data()+scope.name_start, scope.name_size);
    }
    #endif

    SymbolTable(ParseTree& parse_tree) noexcept
        : parse_tree(parse_tree) {}

    void addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except;
    size_t containingScope(const Typeset::Marker& m) const noexcept;
    void getSuggestions(const Typeset::Marker& loc, std::vector<std::string>& suggestions) const;
    const Typeset::Selection& getSel(size_t sym_index) const noexcept;

    void reset(const Typeset::Marker& doc_start) noexcept;
    void addScope(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& start, ParseNode closure = NONE) alloc_except;
    void closeScope(const Typeset::Marker& stop) alloc_except;
    void finalize() noexcept;

    #ifndef NDEBUG
    void verifyIdentifier(ParseNode pn) const noexcept;
    #endif

    struct ScopedVarKey {
        SymbolIndex symbol_index;
        Typeset::Selection sel;
        ScopedVarKey(ParseNode symbol_root, const Typeset::Selection& sel) noexcept : symbol_index(symbol_root), sel(sel) {}
        bool operator==(const ScopedVarKey& other) const noexcept {
            return symbol_index == other.symbol_index && sel == other.sel; }
    };
    struct HashScopedVarKey {
        std::size_t operator()(const ScopedVarKey& s) const {
            return s.symbol_index ^ std::hash<Typeset::Selection>()(s.sel);
        }
    };
    FORSCAPE_UNORDERED_MAP<ScopedVarKey, SymbolIndex, HashScopedVarKey> scoped_vars;

    void resolveReference(ParseNode pn, size_t sym_id, size_t closure_depth) alloc_except;
};

}

}

#endif // FORSCAPE_SCOPE_TREE_H
