#ifndef HOPE_TYPE_RESOLVER_H
#define HOPE_TYPE_RESOLVER_H

#include <algorithm>
#include <code_error_types.h>
#include <limits>
#include <set>
#include <stack>
#include <unordered_map>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
class SymbolTable;
typedef size_t Type;

class TypeResolver : private std::vector<size_t>{

//DO THIS: avoid collisions with logging macros
#undef VOID

public:
    typedef size_t ParseNode;
    typedef std::vector<size_t> DeclareSignature;
    typedef std::vector<size_t> CallSignature;
    static constexpr Type UNINITIALISED = std::numeric_limits<size_t>::max();
    static constexpr Type NUMERIC = UNINITIALISED-1;
    static constexpr Type STRING = UNINITIALISED-2;
    static constexpr Type BOOLEAN = UNINITIALISED-3;
    static constexpr Type VOID = UNINITIALISED-4;
    static constexpr Type RECURSIVE_CYCLE = UNINITIALISED-5;
    static constexpr Type FAILURE = UNINITIALISED-6;
    static constexpr bool isAbstractFunctionGroup(size_t type) noexcept;
    bool retry_at_recursion = false;
    bool first_attempt = true;
    const CallSignature* recursion_fallback = nullptr;

    Type declare(const DeclareSignature& fn);
    Type instantiate(const CallSignature& fn);

    std::string typeString(Type t) const;

    void reset() noexcept;
    Type makeFunctionSet(ParseNode fn) noexcept;
    Type functionSetUnion(Type a, Type b);
    size_t numElements(size_t index) const noexcept;
    ParseNode arg(size_t index, size_t n) const noexcept;

    struct vectorOfIntHash{
        std::size_t operator()(const std::vector<size_t>& vec) const noexcept {
            std::size_t seed = vec.size();
            for(auto& i : vec) seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    const DeclareSignature& declared(size_t index) const noexcept;

private:
    typedef size_t ParentType;
    static constexpr ParentType TYPE_FUNCTION_SET = 0;
    size_t v(size_t index) const noexcept;
    size_t first(size_t index) const noexcept;
    size_t last(size_t index) const noexcept;
    std::string abstractFunctionSetString(Type t) const;
    std::vector<DeclareSignature> declared_funcs;

    std::unordered_map<DeclareSignature, size_t, vectorOfIntHash> declared_func_map;
    std::unordered_map<CallSignature, size_t, vectorOfIntHash> called_func_map;

    std::string declFunctionString(size_t i) const;
    std::string instFunctionString(const CallSignature& sig) const;

    std::unordered_map<std::vector<ParseNode>, Type, vectorOfIntHash> memoized_abstract_function_groups;

    private:
        std::stack<Type> return_types;

    public:
        TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept;
        void resolve();

    private:
        void resolveStmt(size_t pn) noexcept;
        Type fillDefaultsAndInstantiate(ParseNode call_node, CallSignature sig);
        size_t resolveExpr(size_t pn) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn, size_t start = 0) noexcept;
        size_t instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig);
        size_t error(size_t pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;
        ParseNode getFuncFromCallSig(const CallSignature& sig) const noexcept;
        ParseNode getFuncFromDeclSig(const DeclareSignature& sig) const noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Code::Error>& errors;
        std::vector<Type> old_val_cap;
        std::vector<Type> old_ref_cap;
        std::vector<Type> old_args;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
