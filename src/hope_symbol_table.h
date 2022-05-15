#ifndef HOPE_SCOPE_TREE_H
#define HOPE_SCOPE_TREE_H

#include <hope_common.h>
#include "typeset_selection.h"
#include "typeset_text.h"
#include <limits>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

class ParseTree;

typedef size_t ScopeId;
typedef size_t SymbolId;
static constexpr size_t UNKNOWN_SIZE = 0;

struct Symbol{
    size_t declaration_lexical_depth;
    size_t declaration_closure_depth;
    size_t flag;
    size_t type;
    size_t rows = UNKNOWN_SIZE;
    size_t cols = UNKNOWN_SIZE;
    size_t shadowed_var;
    size_t comment;
    bool is_const;
    bool is_used = false;
    bool is_reassigned = false; //Used to determine if parameters are constant
    bool is_closure_nested = false;
    bool is_prototype = false;
    bool is_ewise_index = false;
    bool is_captured_by_value = false;

    Symbol() noexcept;

    Symbol(size_t pn,
           size_t lexical_depth,
           size_t closure_depth,
           size_t shadowed_var,
           bool is_const) noexcept;

    size_t closureIndex() const noexcept;

    const Typeset::Selection& sel(const ParseTree& parse_tree) const noexcept;
};

enum UsageType{
    DECLARE,
    REASSIGN,
    READ,
};

struct Usage{
    size_t var_id;
    ParseNode pn;
    UsageType type;

    Usage() noexcept;

    Usage(size_t var_id, ParseNode pn, UsageType type) noexcept;
};

struct ScopeSegment{
    Typeset::Selection name;
    Typeset::Marker start;
    ParseNode fn;
    ScopeId parent;
    ScopeId prev;
    SymbolId sym_begin;
    size_t usage_begin;
    ScopeId next = NONE;
    SymbolId sym_end = NONE;
    size_t usage_end = NONE;

    ScopeSegment(
        const Typeset::Selection& name,
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
    std::unordered_map<Typeset::Marker, SymbolId> occurence_to_symbol_map;
    std::vector<Usage> usages;
    ParseTree& parse_tree;

    //EVENTUALLY: fix this hack
    Typeset::Text global_name;
    Typeset::Text lambda_name;
    Typeset::Text elementwise_asgn;
    Typeset::Text if_name;
    Typeset::Text else_name;
    Typeset::Text while_name;
    Typeset::Text for_name;
    Typeset::Text big_name;
    Typeset::Text deriv_name;

    SymbolTable(ParseTree& parse_tree)
        : parse_tree(parse_tree) {
        global_name.str = "Global";
        lambda_name.str = "lambda";
        elementwise_asgn.str = "element-wise assignment";
        if_name.str = "if";
        else_name.str = "else";
        while_name.str = "while";
        for_name.str = "for";
        big_name.str = "big symbol";
        deriv_name.str = "derivative";
    }

    void addSymbol(size_t pn, size_t lexical_depth, size_t closure_depth, size_t shadowed, bool is_const) alloc_except;
    void addOccurence(const Typeset::Marker& left, size_t sym_index) alloc_except;
    size_t containingScope(const Typeset::Marker& m) const noexcept;
    std::vector<Typeset::Selection> getSuggestions(const Typeset::Marker& loc) const;
    const Typeset::Selection& getSel(size_t sym_index) const noexcept;
    void getSymbolOccurences(const Typeset::Marker& loc, std::vector<Typeset::Selection>& found) const;

    Typeset::Selection global() noexcept{
        return Typeset::Selection(&global_name, 0, 6);
    }

    Typeset::Selection lambda() noexcept{
        return Typeset::Selection(&lambda_name, 0, 6);
    }

    Typeset::Selection ewise() noexcept{
        return Typeset::Selection(&elementwise_asgn, 0, elementwise_asgn.size());
    }

    Typeset::Selection ifSel() noexcept{
        return Typeset::Selection(&if_name, 0, 2);
    }

    Typeset::Selection elseSel() noexcept{
        return Typeset::Selection(&else_name, 0, 4);
    }

    Typeset::Selection whileSel() noexcept{
        return Typeset::Selection(&while_name, 0, 5);
    }

    Typeset::Selection forSel() noexcept{
        return Typeset::Selection(&for_name, 0, 3);
    }

    Typeset::Selection big() noexcept{
        return Typeset::Selection(&big_name, 0, big_name.size());
    }

    Typeset::Selection deriv() noexcept{
        return Typeset::Selection(&deriv_name, 0, deriv_name.size());
    }

    ScopeId head(ScopeId index) const noexcept;
    void reset(const Typeset::Marker& doc_start) noexcept;
    void addScope(const Typeset::Selection& name, const Typeset::Marker& start, ParseNode closure = NONE) alloc_except;
    void closeScope(const Typeset::Marker& stop) alloc_except;
    void finalize() noexcept;
};

}

}

#endif // HOPE_SCOPE_TREE_H
