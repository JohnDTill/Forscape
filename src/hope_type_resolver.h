#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

#include "hope_type_system.h"
#include <algorithm>
#include <code_error_types.h>
#include <limits>
#include <set>
#include <stack>
#include <unordered_set>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

//DO THIS - get the type system working when each function signature must be inferred at declaration
//DO THIS - get the type system working when functions may be generic, and multiple instantiations are allowed

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
class SymbolTable;

class TypeResolver{
    public:
        TypeSystem ts;

    private:
        std::stack<Type> return_types;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void reset() noexcept;
        void resolveStmt(size_t pn) noexcept;
        size_t instantiateFunc(size_t body, size_t params, bool is_lambda) noexcept;
        size_t resolveExpr(size_t pn) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn, size_t start = 0) noexcept;
        size_t error(size_t pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        bool had_change;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
