#ifndef HOPE_SCOPE_TREE_H
#define HOPE_SCOPE_TREE_H

#include <hope_common.h>
#include "typeset_selection.h"
#include "typeset_text.h"
#include <limits>
#include <vector>

#if !defined(NDEBUG) && !defined(HOPE_TYPESET_HEADLESS)
#define SCOPE_NAME(x) x,
#define HOPE_USE_SCOPE_NAME
#else
#define SCOPE_NAME(x)
#endif

namespace Hope {

namespace Code {

class ParseTree;

typedef size_t ScopeId;
typedef size_t SymbolId;

struct Symbol {
    size_t declaration_lexical_depth  DEBUG_INIT_UNITIALISED;
    size_t declaration_closure_depth  DEBUG_INIT_UNITIALISED;
    size_t flag  DEBUG_INIT_UNITIALISED;
    size_t type = NONE;
    size_t rows = UNKNOWN_SIZE;
    size_t cols = UNKNOWN_SIZE;
    size_t shadowed_var  DEBUG_INIT_UNITIALISED;
    size_t comment  DEBUG_INIT_UNITIALISED;
    size_t scope_tail = NONE;
    bool is_const;
    bool is_used = false;
    bool is_reassigned = false; //Used to determine if parameters are constant
    bool is_closure_nested = false;
    bool is_prototype = false;
    bool is_ewise_index = false;
    bool is_captured_by_value = false;

    Symbol() noexcept;
    Symbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed_var, bool is_const) noexcept;
    size_t closureIndex() const noexcept;
    const Typeset::Selection& sel(const ParseTree& parse_tree) const noexcept;
};

enum UsageType{
    DECLARE,
    READ,
};

struct Usage{
    size_t var_id  DEBUG_INIT_UNITIALISED;
    ParseNode pn  DEBUG_INIT_UNITIALISED;
    UsageType type;

    Usage() noexcept;
    Usage(size_t var_id, ParseNode pn, UsageType type) noexcept;
};

struct ScopeSegment{
    #ifdef HOPE_USE_SCOPE_NAME
    size_t name_start;
    size_t name_size;
    #endif
    Typeset::Marker start;
    ParseNode fn  DEBUG_INIT_UNITIALISED;
    ScopeId parent  DEBUG_INIT_UNITIALISED;
    ScopeId prev_lexical_segment  DEBUG_INIT_UNITIALISED;
    SymbolId sym_begin  DEBUG_INIT_UNITIALISED;
    size_t usage_begin  DEBUG_INIT_UNITIALISED;
    ScopeId next_lexical_segment = NONE;
    ScopeId prev_namespace_segment  DEBUG_INIT_UNITIALISED;
    SymbolId sym_end = NONE;
    size_t usage_end = NONE;

    ScopeSegment(
        #if !defined(NDEBUG) && !defined(HOPE_TYPESET_HEADLESS)
        size_t name_start,
        size_t name_size,
        #endif
        const Typeset::Marker& start,
        ParseNode closure,
        ScopeId parent,
        ScopeId prev,
        SymbolId sym_begin,
        size_t usage_begin
        ) noexcept;

    bool isStartOfScope() const noexcept;
    bool isEndOfScope() const noexcept;
};

class SymbolTable{
public:
    std::vector<ScopeSegment> scopes;
    std::vector<Symbol> symbols;
    std::vector<Usage> usages;
    ParseTree& parse_tree;

    #ifdef HOPE_USE_SCOPE_NAME
    std::string scope_names;
    std::string_view getName(const ScopeSegment& scope) const noexcept {
        return std::string_view(scope_names.data()+scope.name_start, scope.name_size);
    }
    #endif

    SymbolTable(ParseTree& parse_tree) noexcept
        : parse_tree(parse_tree) {}

    void addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except;
    size_t containingScope(const Typeset::Marker& m) const noexcept;
    std::vector<Typeset::Selection> getSuggestions(const Typeset::Marker& loc) const;
    const Typeset::Selection& getSel(size_t sym_index) const noexcept;
    void getSymbolOccurences(size_t sym_id, std::vector<Typeset::Selection>& found) const;

    ScopeId head(ScopeId index) const noexcept;
    void reset(const Typeset::Marker& doc_start) noexcept;
    void addScope(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& start, ParseNode closure = NONE) alloc_except;
    void closeScope(const Typeset::Marker& stop) alloc_except;
    void finalize() noexcept;

    #ifndef NDEBUG
    void verifyIdentifier(ParseNode pn) const noexcept;
    #endif

    struct StoredScopeKey {
        ParseNode symbol_root;
        Typeset::Selection sel;
        StoredScopeKey(ParseNode symbol_root, const Typeset::Selection& sel) noexcept : symbol_root(symbol_root), sel(sel) {}
        bool operator==(const StoredScopeKey& other) const noexcept {
            return symbol_root == other.symbol_root && sel == other.sel; }
    };
    struct HashStoredScopeKey {
        std::size_t operator()(const StoredScopeKey& s) const {
            return s.symbol_root ^ std::hash<Typeset::Selection>()(s.sel);
        }
    };
    HOPE_UNORDERED_MAP<StoredScopeKey, size_t, HashStoredScopeKey> stored_scopes;

    void resolveReference(ParseNode pn, size_t sym_id, size_t closure_depth) alloc_except;
};

}

}

#endif // HOPE_SCOPE_TREE_H
