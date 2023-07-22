#ifndef FORSCAPE_TYPE_RESOLVER_H
#define FORSCAPE_TYPE_RESOLVER_H

#include <algorithm>
#include <code_error_types.h>
#include <forscape_common.h>
#include <forscape_dynamic_settings.h>
#include <limits>
#include <set>
#include <stack>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

typedef size_t ParseNode;
struct PairHash{
    size_t operator()(const std::pair<size_t, size_t>& in) const noexcept {
        return std::hash<size_t>{}(in.first ^ (in.second << 4*sizeof(size_t)));
    }
};
typedef FORSCAPE_UNORDERED_MAP<std::pair<ParseNode, ParseNode>, ParseNode, PairHash> InstantiationLookup;

//EVENTUALLY: should generalise these to any supported Value type
typedef FORSCAPE_UNORDERED_MAP<std::pair<ParseNode, double>, ParseNode> NumericSwitchMap;
typedef FORSCAPE_UNORDERED_MAP<std::pair<ParseNode, std::string>, ParseNode> StringSwitchMap;

struct Error;
class ParseTree;
class SymbolTable;
struct Symbol;
typedef size_t Type;

class StaticPass : private std::vector<size_t>{

public:
    InstantiationLookup instantiation_lookup;
    NumericSwitchMap number_switch;
    StringSwitchMap string_switch;
    typedef std::vector<size_t> DeclareSignature;
    typedef std::vector<size_t> CallSignature;
    static constexpr Type UNINITIALISED = std::numeric_limits<size_t>::max();
    static constexpr Type NUMERIC = UNINITIALISED-1;
    static constexpr Type STRING = UNINITIALISED-2;
    static constexpr Type BOOLEAN = UNINITIALISED-3;
    static constexpr Type VOID_TYPE = UNINITIALISED-4;
    static constexpr Type RECURSIVE_CYCLE = UNINITIALISED-5;
    static constexpr Type FAILURE = UNINITIALISED-6;
    static constexpr Type NAMESPACE = UNINITIALISED-7;
    static constexpr Type MODULE = UNINITIALISED-8;
    static constexpr Type ALIAS = UNINITIALISED-9;
    static constexpr bool isAbstractFunctionGroup(size_t type) noexcept;
    bool retry_at_recursion = false;
    bool first_attempt = true;
    const CallSignature* recursion_fallback = nullptr;
    bool encountered_autosize = false;
    bool second_dim_attempt = false;

    Type declare(const DeclareSignature& fn);
    Type instantiate(ParseNode call_node, const CallSignature& fn);

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
        size_t rows;
        size_t cols;
        ParseNode instantiated;

        CallResult(){}
        CallResult(Type type, size_t rows, size_t cols, ParseNode instantiated)
            : type(type), rows(rows), cols(cols), instantiated(instantiated) {}
    };

    FORSCAPE_UNORDERED_MAP<DeclareSignature, size_t, vectorOfIntHash> declared_func_map;
    FORSCAPE_UNORDERED_MAP<CallSignature, CallResult, vectorOfIntHash> called_func_map;

    std::vector<std::pair<ParseNode, CallSignature>> all_calls;

    std::string declFunctionString(size_t i) const;
    std::string instFunctionString(const CallSignature& sig) const;

    FORSCAPE_UNORDERED_MAP<std::vector<ParseNode>, Type, vectorOfIntHash> memoized_abstract_function_groups;

    private:
        struct ReturnType{
            Type type;
            size_t rows = 0;
            size_t cols = 0;

            ReturnType() noexcept {}
            ReturnType(Type type) noexcept
                : type(type) {}
        };
        std::stack<ReturnType> return_types;

    public:
        StaticPass(
            ParseTree& parse_tree,
            std::vector<Code::Error>& errors,
            std::vector<Code::Error>& warnings) noexcept;
        void resolve(Typeset::Model* entry_point);

    private:
        ParseNode resolveStmt(ParseNode pn) noexcept;
        ParseNode resolveLValue(ParseNode pn, bool write = false) noexcept;
        Type fillDefaultsAndInstantiate(ParseNode call_node, CallSignature sig);
        ParseNode resolveExprTop(size_t pn, size_t rows_expected = 0, size_t cols_expected = 0);
        ParseNode resolveExpr(size_t pn, size_t rows_expected = 0, size_t cols_expected = 0) noexcept;
        ParseNode patchSingleCharMult(ParseNode parent, ParseNode mult) noexcept;
        size_t callSite(size_t pn) noexcept;
        size_t implicitMult(size_t pn, size_t start = 0) noexcept;
        Type instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig);
        size_t error(ParseNode pn, ParseNode sel, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;
        size_t errorType(ParseNode pn, ErrorCode code = ErrorCode::TYPE_ERROR) noexcept;
        ParseNode getFuncFromCallSig(const CallSignature& sig) const noexcept;
        ParseNode getFuncFromDeclSig(const DeclareSignature& sig) const noexcept;
        ParseNode resolveAlg(ParseNode pn);
        ParseNode resolveSwitch(ParseNode pn);
        ParseNode resolveSwitchNumeric(ParseNode pn, ParseNode switch_key);
        ParseNode resolveSwitchString(ParseNode pn, ParseNode switch_key);
        ParseNode resolveDeriv(ParseNode pn);
        ParseNode resolveIdentity(ParseNode pn);
        ParseNode resolveInverse(ParseNode pn);
        ParseNode resolveLambda(ParseNode pn);
        ParseNode resolveMatrix(ParseNode pn);
        ParseNode resolveTranspose(ParseNode pn, size_t rows_expected = 0, size_t cols_expected = 0);
        ParseNode resolveMult(ParseNode pn, size_t rows_expected = 0, size_t cols_expected = 0);
        ParseNode resolveOnesMatrix(ParseNode pn);
        ParseNode resolvePower(ParseNode pn);
        ParseNode resolveUnaryMinus(ParseNode pn);
        ParseNode resolveUnitVector(ParseNode pn);
        ParseNode resolveZeroMatrix(ParseNode pn);
        ParseNode resolveLimit(ParseNode pn);
        ParseNode resolveDefiniteIntegral(ParseNode pn);
        ParseNode resolveScopeAccess(ParseNode pn, bool write = false);
        ParseNode resolveBlock(ParseNode pn);
        ParseNode copyChildProperties(ParseNode pn) noexcept;
        ParseNode enforceScalar(ParseNode pn);
        ParseNode enforceZero(ParseNode pn);
        ParseNode enforceNaturalNumber(ParseNode pn);
        ParseNode enforceSemiPositiveInt(ParseNode pn);
        static bool dimsDisagree(size_t a, size_t b) noexcept;
        Settings& settings() const noexcept;
        SymbolTable& symbolTable() const noexcept;
        void finaliseSymbolTable(Typeset::Model* model) const noexcept;

        ParseTree& parse_tree;
        std::vector<Error>& errors;
        std::vector<Error>& warnings;
        Typeset::Model* active_model  DEBUG_INIT_NULLPTR;
        std::vector<Typeset::Model*> imported_models;

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

        size_t closureDepth() const noexcept { return return_types.size(); }
};

}

}

#endif // FORSCAPE_TYPE_RESOLVER_H
