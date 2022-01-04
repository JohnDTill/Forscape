#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

#include <limits>
#include <vector>

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
struct SymbolTable;

class TypeResolver{
    static constexpr size_t TYPE_ERROR = 0;
    static constexpr size_t TYPE_ANY = std::numeric_limits<size_t>::max();
    static constexpr size_t TYPE_NUMERIC = 1;
    static constexpr size_t TYPE_STRING = 1 << 1;
    static constexpr size_t TYPE_ALG = 1 << 2;
    static constexpr size_t TYPE_BINARY = 1 << 3;
    static constexpr size_t TYPE_UNCHECKED = 1 << 4;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve(size_t root);

    private:
        void performPass() noexcept;
        void traverseNode(size_t pn) noexcept;
        void compareWithChildren(size_t pn) noexcept;
        void enforceSameAsChildren(size_t pn) noexcept;
        void enforceType(size_t pn, size_t type) noexcept;
        void enforceNumeric(size_t pn) noexcept;
        void enforceBinary(size_t pn) noexcept;
        void enforceString(size_t pn) noexcept;
        void enforceSame(size_t a, size_t b) noexcept;
        void reportErrors(size_t pn) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        size_t root;
        bool had_change;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
