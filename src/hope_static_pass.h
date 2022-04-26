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

typedef size_t ParseNode;
struct PairHash{
    size_t operator()(const std::pair<ParseNode, ParseNode>& in) const noexcept {
        return std::hash<size_t>{}(in.first ^ (in.second << 32));
    }
};
typedef std::unordered_map<std::pair<ParseNode, ParseNode>, ParseNode, PairHash> InstantiationLookup;

struct Error;
class ParseTree;
class SymbolTable;
struct Symbol;
typedef size_t Type;

class StaticPass : private std::vector<size_t>{

public:
    InstantiationLookup instantiation_lookup;
    typedef std::vector<size_t> DeclareSignature;
    typedef std::vector<size_t> CallSignature;
    static constexpr Type UNINITIALISED = std::numeric_limits<size_t>::max();
    static constexpr Type NUMERIC = UNINITIALISED-1;
    static constexpr Type STRING = UNINITIALISED-2;
    static constexpr Type BOOLEAN = UNINITIALISED-3;
    static constexpr Type VOID_TYPE = UNINITIALISED-4;
    static constexpr Type RECURSIVE_CYCLE = UNINITIALISED-5;
    static constexpr Type FAILURE = UNINITIALISED-6;
    static constexpr bool isAbstractFunctionGroup(size_t type) noexcept;
    bool retry_at_recursion = false;
    bool first_attempt = true;
    const CallSignature* recursion_fallback = nullptr;
    bool encountered_autosize = false;
    bool second_dim_attempt = false;

    Type declare(const DeclareSignature& fn);
    Type instantiate(ParseNode call_node,const CallSignature& fn);

    std::string typeString(Type t) const;
    std::string typeString(const Symbol& sym) const;

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
    static std::string numTypeString(size_t rows, size_t cols);

    struct CallResult{
        Type type;
        ParseNode instantiated;

        CallResult(){}
        CallResult(Type type, ParseNode instantiated)
            : type(type), instantiated(instantiated) {}
    };

    std::unordered_map<DeclareSignature, size_t, vectorOfIntHash> declared_func_map;
    std::unordered_map<CallSignature, CallResult, vectorOfIntHash> called_func_map;

    std::string declFunctionString(size_t i) const;
    std::string instFunctionString(const CallSignature& sig) const;

    std::unordered_map<std::vector<ParseNode>, Type, vectorOfIntHash> memoized_abstract_function_groups;

    private:
        std::stack<Type> return_types;

    public:
        StaticPass(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors, std::vector<Code::Error>& warnings) noexcept;
        void resolve();

    private:
        ParseNode resolveStmt(size_t pn) noexcept;
        Type fillDefaultsAndInstantiate(ParseNode call_node, CallSignature sig);
        ParseNode resolveExprTop(size_t pn, size_t rows_expected = 0, size_t cols_expected = 0);
        ParseNode resolveExpr(size_t pn, size_t rows_expected = 0, size_t cols_expected = 0) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn, size_t start = 0) noexcept;
        Type instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig);
        size_t error(ParseNode pn, ParseNode sel, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;
        size_t errorType(ParseNode pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;
        ParseNode getFuncFromCallSig(const CallSignature& sig) const noexcept;
        ParseNode getFuncFromDeclSig(const DeclareSignature& sig) const noexcept;
        ParseNode resolveAlg(ParseNode pn);
        ParseNode resolveDeriv(ParseNode pn);
        ParseNode resolveIdentity(ParseNode pn);
        ParseNode resolveInverse(ParseNode pn);
        ParseNode resolveLambda(ParseNode pn);
        ParseNode resolveMatrix(ParseNode pn);
        ParseNode resolveMult(ParseNode pn, size_t rows_expected = 0, size_t cols_expected = 0);
        ParseNode resolveOnesMatrix(ParseNode pn);
        ParseNode resolvePower(ParseNode pn);
        ParseNode resolveUnaryMinus(ParseNode pn);
        ParseNode resolveUnitVector(ParseNode pn);
        ParseNode resolveZeroMatrix(ParseNode pn);
        ParseNode copyChildProperties(ParseNode pn) noexcept;
        ParseNode enforceScalar(ParseNode pn);
        ParseNode enforceZero(ParseNode pn);
        ParseNode enforceNaturalNumber(ParseNode pn);
        ParseNode enforcePositiveInt(ParseNode pn);
        static bool dimsDisagree(size_t a, size_t b) noexcept;

        ParseTree& parse_tree;
        SymbolTable& symbol_table;
        std::vector<Error>& errors;
        std::vector<Error>& warnings;

        struct CachedInfo{
            Type type;
            size_t rows;
            size_t cols;

            CachedInfo() noexcept {}
            CachedInfo(const Symbol& sym) noexcept;
            void restore(Symbol& sym) const noexcept;
        };

        std::vector<CachedInfo> old_val_cap;
        std::vector<CachedInfo> old_ref_cap;
        std::vector<CachedInfo> old_args;
};

}

}

#endif // HOPE_TYPE_RESOLVER_H
