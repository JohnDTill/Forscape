#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

#include <algorithm>
#include <code_error_types.h>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
struct SymbolTable;

class TypeResolver{
    enum Type {
        TYPE_ERROR,
        TYPE_NUMERIC,
        TYPE_STRING,
        TYPE_ALG,
        TYPE_BOOLEAN,
        TYPE_CALLABLE,
        TYPE_UNCHECKED,
    };

    struct FuncSignature{
        size_t nargs;
        std::vector<size_t> default_arg_types;
    };

    std::vector<FuncSignature> function_signatures;
    size_t active_signature;

    struct CallSignature{
        size_t pn;
        std::vector<size_t> arg_types; //Could use a small array

        bool operator==(const CallSignature& other) const noexcept {
            return pn == other.pn && std::equal(arg_types.begin(), arg_types.end(), other.arg_types.begin());
        }
    };

    struct CallSignatureHash{
        size_t operator()(const CallSignature& cs) const noexcept {
            std::size_t seed = cs.pn;
            for(const auto& i : cs.arg_types) seed ^= i + 0x9e3779b9 + (seed << 12) + (seed >> 4);
            return seed;
        }
    };

    std::unordered_map<CallSignature, size_t, CallSignatureHash> instantiated_functions;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void reset() noexcept;
        void resolveStmt(size_t pn) noexcept;
        void resolveParams(size_t pn, size_t params) noexcept;
        size_t resolveExpr(size_t pn) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn) noexcept;
        size_t instantiate(const CallSignature& sig) noexcept;
        size_t error(size_t pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        bool had_change;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
