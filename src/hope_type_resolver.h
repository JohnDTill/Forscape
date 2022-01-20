#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

#include <algorithm>
#include <code_error_types.h>
#include <limits>
#include <unordered_map>
#include <vector>

//DO THIS - get the type system working when each function signature must be inferred at declaration
//DO THIS - get the type system working when functions may be generic, and multiple instantiations are allowed

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
class SymbolTable;

class TypeResolver{
    static constexpr size_t TYPE_UNKNOWN = std::numeric_limits<size_t>::max();
    static constexpr size_t TYPE_NUMERIC = std::numeric_limits<size_t>::max()-1;
    static constexpr size_t TYPE_STRING = std::numeric_limits<size_t>::max()-2;
    static constexpr size_t TYPE_BOOLEAN = std::numeric_limits<size_t>::max()-3;
    static constexpr size_t TYPE_VOID = std::numeric_limits<size_t>::max()-4;
    static constexpr bool isFunction(size_t type) noexcept {
        return type < TYPE_VOID;
    }

    static std::vector<size_t> function_type_pool; //DO THIS - is single threaded acceptable?

    struct FuncSignature{
        size_t name;
        size_t args_begin;
        size_t args_end;
        size_t n_default;

        size_t numArgs() const noexcept{
            return args_end - args_begin;
        }

        auto begin() const noexcept{
            return function_type_pool.data() + args_begin;
        }

        auto end() const noexcept{
            return function_type_pool.data() + args_end;
        }

        size_t returnType() const noexcept{
            return function_type_pool[args_begin];
        }

        bool operator==(const FuncSignature& other) const noexcept {
            return name == other.name && std::equal(begin(), end(), other.begin(), other.end());
        }
    };

    struct FuncSignatureHash{
        size_t operator()(const FuncSignature& sig) const noexcept {
            std::size_t seed = sig.name;
            for(auto type : sig) seed ^= type + 0x9e3779b9 + (seed << 12) + (seed >> 4);
            return seed;
        }
    };

    static std::unordered_map<FuncSignature, size_t, FuncSignatureHash> memoized_signatures;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void reset() noexcept;
        void resolveStmt(size_t pn) noexcept;
        size_t resolveFunction(size_t body, size_t params) noexcept;
        size_t resolveExpr(size_t pn, size_t expected) noexcept;
        size_t callSite(size_t pn, size_t expected) noexcept;
        size_t implicitMult(size_t pn, size_t expected, size_t start = 0) noexcept;
        size_t error(size_t pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        bool had_change;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
