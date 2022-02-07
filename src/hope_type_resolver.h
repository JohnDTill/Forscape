#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

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
    static constexpr size_t TYPE_UNKNOWN = std::numeric_limits<size_t>::max();
    static constexpr size_t TYPE_NUMERIC = std::numeric_limits<size_t>::max()-1;
    static constexpr size_t TYPE_STRING = std::numeric_limits<size_t>::max()-2;
    static constexpr size_t TYPE_BOOLEAN = std::numeric_limits<size_t>::max()-3;
    static constexpr size_t TYPE_FAILURE = std::numeric_limits<size_t>::max()-4;
    static constexpr size_t TYPE_VOID = std::numeric_limits<size_t>::max()-5;
    static constexpr bool isAbstractFunctionGroup(size_t type) noexcept {
        return type < TYPE_VOID;
    }

    typedef std::set<size_t> AbstractFunctionPool;

    std::stack<size_t> return_types;

    static std::vector<size_t> function_type_pool; //DO THIS - is single threaded acceptable?
    //DO THIS - is memoizing even worth it? (yes, it's needed to nest complicated types)


    struct FuncSignature{
        size_t args_begin;
        size_t args_end;
        size_t n_default = 0;

        size_t numTypes() const noexcept{
            return args_end - args_begin;
        }

        size_t numParams() const noexcept{
            return numTypes()-1;
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

        size_t paramType() const noexcept{
            assert(numParams() == 1);
            return function_type_pool[args_begin+1];
        }

        size_t paramType(size_t index) const noexcept{
            assert(index < numParams());
            return function_type_pool[args_begin+1+index];
        }

        bool operator==(const FuncSignature& other) const noexcept {
            return std::equal(begin(), end(), other.begin(), other.end());
        }
    };

    static std::vector<FuncSignature> function_sig_pool;

    struct FuncSignatureHash{
        size_t operator()(size_t index) const noexcept {
            const FuncSignature& sig = function_sig_pool[index];
            std::size_t seed = sig.numTypes();
            for(auto type : sig) seed ^= type + 0x9e3779b9 + (seed << 12) + (seed >> 4);
            return seed;
        }
    };

    struct FuncSignatureEqual {
        bool operator()(size_t a, size_t b) const {
            return function_sig_pool[a] == function_sig_pool[b];
        }
    };

    static std::unordered_set<size_t, FuncSignatureHash, FuncSignatureEqual> memoized_signatures;

    static size_t getMemoizedType(const FuncSignature& sig){
        size_t index = function_sig_pool.size();
        function_sig_pool.push_back(sig);

        auto lookup = memoized_signatures.insert(index);
        if(lookup.second){
            std::cout << "Memoized, returning " << index << std::endl;
            return index;
        }

        std::cout << "Found, returning " << (*lookup.first) << std::endl;

        function_type_pool.resize(function_type_pool.size() - sig.numTypes());
        return *lookup.first;
    }

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void reset() noexcept;
        void resolveStmt(size_t pn) noexcept;
        size_t instantiateFunc(size_t body, size_t params, bool is_lambda) noexcept;
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
