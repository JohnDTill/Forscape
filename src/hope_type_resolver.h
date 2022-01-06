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

    struct FunctionSignature{
        size_t pn;
        std::vector<size_t> type_fields; //Could use a small array

        bool operator==(const FunctionSignature& other) const noexcept {
            return pn == other.pn && std::equal(type_fields.begin(), type_fields.end(), other.type_fields.begin());
        }
    };

    struct FunctionSignatureHash{
        size_t operator()(const FunctionSignature& fs) const noexcept {
            std::size_t seed = fs.pn;
            for(const auto& i : fs.type_fields) seed ^= i + 0x9e3779b9 + (seed << 12) + (seed >> 4);
            return seed;
        }
    };

    std::unordered_map<FunctionSignature, size_t, FunctionSignatureHash> concrete_functions;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void resolveStmt(size_t pn) noexcept;
        size_t traverseNode(size_t pn) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn) noexcept;
        size_t instantiate(const FunctionSignature& sig) noexcept;
        size_t error(size_t pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        bool had_change;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
