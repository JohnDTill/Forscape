#include "hope_type_resolver.h"

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"

namespace Hope {

namespace Code {

TypeResolver::TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors) {}

void TypeResolver::resolve(){
    reset();
    if(!errors.empty()) return;
    resolveStmt(parse_tree.root);

    for(const auto& entry : called_func_map)
        if(entry.second == RECURSIVE_CYCLE)
            error(getFuncFromCallSig(entry.first), RECURSIVE_TYPE);
    if(!errors.empty()) return;

    //Not sure about the soundness of the recursion handling strategy, so let's double check
    //EVENTUALLY: remove this check
    const auto call_map_backup = called_func_map;
    for(const auto& entry : call_map_backup){
        called_func_map.erase(entry.first);
        assert(instantiate(entry.first) == entry.second);
    }
}

void TypeResolver::reset() noexcept{
    clear();
    memoized_abstract_function_groups.clear();
    declared_funcs.clear();
    declared_func_map.clear();
    called_func_map.clear();
    assert(return_types.empty());
    assert(retry_at_recursion == false);
    assert(first_attempt == true);
    assert(recursion_fallback == nullptr);

    for(Symbol& sym : symbol_table.symbols)
        sym.type = UNINITIALISED;
}

void TypeResolver::resolveStmt(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) != expected){ \
            error(node); \
            return; \
        }

    switch (parse_tree.getOp(pn)) {
        case OP_ASSIGN:
        case OP_EQUAL:{
            size_t sym_id = parse_tree.getFlag(parse_tree.lhs(pn));
            Symbol& sym = symbol_table.symbols[sym_id];
            sym.type = resolveExpr(parse_tree.rhs(pn));
            break;
        }
        case OP_REASSIGN:{
            ParseNode lhs = parse_tree.lhs(pn);
            if(parse_tree.getOp(lhs) == OP_SUBSCRIPT_ACCESS){
                for(size_t i = 0; i < parse_tree.getNumArgs(lhs); i++)
                    EXPECT(parse_tree.arg(lhs, i), NUMERIC)
                EXPECT(parse_tree.rhs(pn), NUMERIC)
            }else{
                size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(parse_tree.lhs(pn)));
                Symbol& sym = symbol_table.symbols[sym_id];
                if(isAbstractFunctionGroup(sym.type)){
                    Type t = resolveExpr(parse_tree.rhs(pn));
                    if(!isAbstractFunctionGroup(t)){
                        error(parse_tree.rhs(pn));
                    }else{
                        sym.type = functionSetUnion(sym.type, t);
                    }
                }else EXPECT(parse_tree.rhs(pn), sym.type)
            }
            break;
        }

        case OP_RETURN:{
            assert(!return_types.empty());
            size_t expected = return_types.top();
            if(expected == UNINITIALISED || expected == RECURSIVE_CYCLE){
                return_types.top() = resolveExpr(parse_tree.child(pn));
            }else if(isAbstractFunctionGroup(expected)){
                Type t = resolveExpr(parse_tree.child(pn));
                if(!isAbstractFunctionGroup(t)){
                    error(parse_tree.child(pn));
                }else{
                    return_types.top() = functionSetUnion(return_types.top(), t);
                }
            }else{
                EXPECT(parse_tree.child(pn), expected)
            }
            break;
        }

        case OP_RETURN_EMPTY:{
            assert(!return_types.empty());
            if(return_types.top() == UNINITIALISED){
                return_types.top() = VOID;
            }else if(return_types.top() != VOID){
                error(pn);
            }
            break;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:{
            ParseNode lhs = parse_tree.lhs(pn);
            EXPECT(parse_tree.arg(lhs, 0), NUMERIC)

            for(size_t i = 1; i < parse_tree.getNumArgs(lhs); i++){
                ParseNode sub = parse_tree.arg(lhs, i);
                if(parse_tree.getOp(sub) == OP_IDENTIFIER){
                    size_t sym_id = parse_tree.getFlag(sub);
                    Symbol& sym = symbol_table.symbols[sym_id];
                    sym.type = NUMERIC;
                }else{
                    EXPECT(sub, NUMERIC)
                }
            }

            EXPECT(parse_tree.rhs(pn), NUMERIC)
            break;
        }

        case OP_IF:
        case OP_WHILE:
            EXPECT(parse_tree.lhs(pn), BOOLEAN)
            resolveStmt(parse_tree.rhs(pn));
            break;

        case OP_IF_ELSE:
            EXPECT(parse_tree.arg(pn, 0), BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            break;

        case OP_FOR:
            resolveStmt(parse_tree.arg(pn, 0));
            EXPECT(parse_tree.arg(pn, 1), BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 2));
            resolveStmt(parse_tree.arg(pn, 3));
            break;

        case OP_EXPR_STMT:
            resolveExpr(parse_tree.child(pn));
            break;

        case OP_ALGORITHM:{
            ParseNode params = parse_tree.paramList(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getOp(param) == OP_EQUAL) resolveStmt(param);
            }

            TypeResolver::DeclareSignature sig;
            sig.push_back(pn);

            ParseNode cap_list = parse_tree.valCapList(pn);
            size_t cap_list_size = parse_tree.valListSize(cap_list);
            for(size_t i = 0; i < cap_list_size; i++){
                ParseNode cap = parse_tree.arg(cap_list, i);
                size_t inner_id = parse_tree.getFlag(cap);
                assert(inner_id < symbol_table.symbols.size());
                const Symbol& inner = symbol_table.symbols[inner_id];
                const Symbol& outer = symbol_table.symbols[inner.shadowed_var];
                Type t = outer.type;
                sig.push_back(t);
            }

            ParseNode ref_list = parse_tree.refCapList(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
                ParseNode ref = parse_tree.arg(ref_list, i);
                if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
                size_t sym_id = parse_tree.getFlag(ref);
                Type t = symbol_table.symbols[sym_id].type;
                sig.push_back(t);
            }

            Type t = declare(sig);

            size_t sym_id = parse_tree.getFlag(parse_tree.algName(pn));
            symbol_table.symbols[sym_id].type = makeFunctionSet(t);

            break;
        }

        case OP_PROTOTYPE_ALG:{
            size_t sym_id = parse_tree.getFlag(parse_tree.arg(pn, 0));
            symbol_table.symbols[sym_id].type = UNINITIALISED;
            break;
        }

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveStmt(parse_tree.arg(pn, i));
            break;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveExpr(parse_tree.arg(pn, i));
            break;

        case OP_ASSERT:
            EXPECT(parse_tree.child(pn), BOOLEAN)
            break;

        default:
            assert(false);
    }

    #undef EXPECT
}

Type TypeResolver::fillDefaultsAndInstantiate(ParseNode call_node, TypeResolver::CallSignature sig){
    const TypeResolver::DeclareSignature& dec = declared(sig.front());
    ParseNode fn = dec.front();

    ParseNode params = parse_tree.paramList(fn);

    size_t n_params = parse_tree.getNumArgs(params);
    size_t n_args = sig.size()-1;
    if(n_args > n_params) return error(call_node, TOO_MANY_ARGS);

    //Resolve default args
    for(size_t i = n_args; i < n_params; i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) != OP_EQUAL) return error(call_node, TOO_FEW_ARGS);
        ParseNode default_var = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getFlag(default_var);
        Type t = symbol_table.symbols[sym_id].type;
        sig.push_back(t);
    }

    return instantiate(sig);
}

size_t TypeResolver::resolveExpr(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    #define EXPECT_OR(node, expected, code) \
        if(resolveExpr(node) != expected) return error(node, code);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) != expected) return error(node);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA:{
            ParseNode params = parse_tree.paramList(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getOp(param) == OP_EQUAL) resolveStmt(param);
            }

            TypeResolver::DeclareSignature sig;
            sig.push_back(pn);

            ParseNode val_list = parse_tree.valCapList(pn);
            size_t num_vals = parse_tree.valListSize(val_list);
            for(size_t i = 0; i < num_vals; i++){
                ParseNode cap = parse_tree.arg(val_list, i);
                size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(cap));
                Type t = symbol_table.symbols[sym_id].type;
                sig.push_back(t);
            }

            ParseNode ref_list = parse_tree.refCapList(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
                ParseNode ref = parse_tree.arg(ref_list, i);
                if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
                size_t sym_id = parse_tree.getFlag(ref);
                Type t = symbol_table.symbols[sym_id].type;
                sig.push_back(t);
            }

            Type t = declare(sig);

            return makeFunctionSet(t);
        }
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:{
            ParseNode var = parse_tree.getFlag(pn);
            size_t sym_id = parse_tree.getFlag(var);
            Symbol& sym = symbol_table.symbols[sym_id];

            return sym.type;
        }
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            size_t expected = resolveExpr(parse_tree.arg(pn, 0));
            if(expected != NUMERIC && expected != STRING) return error(parse_tree.arg(pn, 0), TYPE_NOT_ADDABLE);

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), expected)

            return expected;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT_OR(parse_tree.arg(pn, i), NUMERIC, BAD_READ_OR_SUBSCRIPT);
            return NUMERIC;
        case OP_SLICE_ALL:
            return NUMERIC;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), NUMERIC)
            return NUMERIC;
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:
            return resolveExpr(parse_tree.child(pn));
        case OP_ARCTANGENT2:
        case OP_BACKSLASH:
        case OP_BINOMIAL:
        case OP_CROSS:
        case OP_DIVIDE:
        case OP_DOT:
        case OP_FRACTION:
        case OP_FORWARDSLASH:
        case OP_IDENTITY_MATRIX:
        case OP_LOGARITHM_BASE:
        case OP_MODULUS:
        case OP_NORM_p:
        case OP_ONES_MATRIX:
        case OP_POWER:
        case OP_ROOT:
        case OP_SUBTRACTION:
        case OP_ZERO_MATRIX:
            EXPECT(parse_tree.lhs(pn), NUMERIC)
            EXPECT(parse_tree.rhs(pn), NUMERIC)
            return NUMERIC;
        case OP_LENGTH:
            EXPECT(parse_tree.child(pn), NUMERIC)
            return NUMERIC;
        case OP_ARCCOSINE:
        case OP_ARCCOTANGENT:
        case OP_ARCSINE:
        case OP_ARCTANGENT:
        case OP_COSINE:
        case OP_DAGGER:
        case OP_EXP:
        case OP_FACTORIAL:
        case OP_HYPERBOLIC_ARCCOSECANT:
        case OP_HYPERBOLIC_ARCCOSINE:
        case OP_HYPERBOLIC_ARCCOTANGENT:
        case OP_HYPERBOLIC_ARCSECANT:
        case OP_HYPERBOLIC_ARCSINE:
        case OP_HYPERBOLIC_ARCTANGENT:
        case OP_HYPERBOLIC_COSECANT:
        case OP_LOGARITHM:
        case OP_NATURAL_LOG:
        case OP_NORM:
        case OP_NORM_1:
        case OP_NORM_INFTY:
        case OP_SINE:
        case OP_SQRT:
        case OP_TANGENT:
        case OP_TRANSPOSE:
        case OP_UNARY_MINUS:
            EXPECT(parse_tree.child(pn), NUMERIC)
            return NUMERIC;
        case OP_SUMMATION:
        case OP_PRODUCT:{
            ParseNode asgn = parse_tree.arg(pn, 0);
            assert(parse_tree.getOp(asgn) == OP_ASSIGN);
            symbol_table.symbols[parse_tree.getFlag(parse_tree.lhs(asgn))].type = NUMERIC;
            EXPECT(parse_tree.rhs(asgn), NUMERIC)
            EXPECT(parse_tree.arg(pn, 1), NUMERIC)
            EXPECT(parse_tree.arg(pn, 2), NUMERIC)
            return NUMERIC;
        }
        case OP_LOGICAL_NOT:
            EXPECT(parse_tree.child(pn), BOOLEAN)
            return BOOLEAN;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:
            EXPECT(parse_tree.lhs(pn), BOOLEAN)
            EXPECT(parse_tree.rhs(pn), BOOLEAN)
            return BOOLEAN;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            EXPECT(parse_tree.lhs(pn), NUMERIC)
            EXPECT(parse_tree.rhs(pn), NUMERIC)
            return BOOLEAN;
        case OP_CASES:{
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2)
                EXPECT(parse_tree.arg(pn, i), BOOLEAN)

            size_t expected = resolveExpr(parse_tree.arg(pn, 0));
            if(isAbstractFunctionGroup(expected))
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2){
                    Type t = resolveExpr(parse_tree.arg(pn, i));
                    if(!isAbstractFunctionGroup(t)) return error(parse_tree.arg(pn, i));
                    expected = functionSetUnion(expected, t);
                }
            else
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2)
                    EXPECT(parse_tree.arg(pn, i), expected)
            return expected;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:{
            EXPECT(parse_tree.rhs(pn), resolveExpr(parse_tree.lhs(pn)))
            return BOOLEAN;
        }
        case OP_DECIMAL_LITERAL:
        case OP_INTEGER_LITERAL:
        case OP_PI:
        case OP_EULERS_NUMBER:
        case OP_GOLDEN_RATIO:
        case OP_SPEED_OF_LIGHT:
        case OP_PLANCK_CONSTANT:
        case OP_REDUCED_PLANCK_CONSTANT:
        case OP_STEFAN_BOLTZMANN_CONSTANT:
        case OP_IDENTITY_AUTOSIZE:
        case OP_GRAVITY:
            return NUMERIC;
        case OP_FALSE:
        case OP_TRUE:
            return BOOLEAN;
        case OP_STRING:
            return STRING;
        case OP_PROXY:
            return resolveExpr(parse_tree.getFlag(pn));
        default:
            assert(false);
            return BOOLEAN;
    }
}

size_t TypeResolver::callSite(size_t pn) noexcept{
    ParseNode call_expr = parse_tree.arg(pn, 0);
    size_t node_size = parse_tree.getNumArgs(pn);
    size_t callable_type = resolveExpr(call_expr);

    if(callable_type == NUMERIC){
        bool is_mult = (node_size == 2 && resolveExpr(parse_tree.rhs(pn)) == NUMERIC);
        return is_mult ? NUMERIC : error(pn, NOT_CALLABLE);
    }else if(callable_type == UNINITIALISED){
        return error(call_expr, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(callable_type)){
        return error(call_expr, NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(callable_type));
    sig.push_back(arg(callable_type, 0));

    for(size_t i = 1; i < node_size; i++){
        ParseNode arg = parse_tree.arg(pn, i);
        sig.push_back(resolveExpr(arg));
    }

    return instantiateSetOfFuncs(pn, callable_type, sig);
}

size_t TypeResolver::implicitMult(size_t pn, size_t start) noexcept{
    ParseNode lhs = parse_tree.arg(pn, start);
    size_t tl = resolveExpr(lhs);

    if(start == parse_tree.getNumArgs(pn)-1) return tl;
    else if(tl == NUMERIC){
        if(implicitMult(pn, start+1) != NUMERIC) return error(parse_tree.arg(pn, start+1));
        return NUMERIC;
    }else if(tl == UNINITIALISED){
        return error(lhs, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(tl)){
        return error(lhs, NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(tl));
    sig.push_back(arg(tl, 0));

    Type tr = implicitMult(pn, start+1);
    sig.push_back(tr);

    return instantiateSetOfFuncs(pn, tl, sig);
}

size_t TypeResolver::instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig){
    Type expected = RECURSIVE_CYCLE;
    size_t expected_index;

    for(expected_index = 0; expected_index < numElements(fun_group) && expected == RECURSIVE_CYCLE; expected_index++){
        sig[0] = arg(fun_group, expected_index);
        expected = fillDefaultsAndInstantiate(call_node, sig);
    }

    if(expected == RECURSIVE_CYCLE){
        for(size_t i = 0; i < numElements(fun_group); i++){
            size_t decl_index = arg(fun_group, i);
            const DeclareSignature& dec = declared(decl_index);
            ParseNode fn = getFuncFromDeclSig(dec);
            if(parse_tree.getOp(fn) != OP_LAMBDA) fn = parse_tree.arg(fn, 0);
            error(fn, RECURSIVE_TYPE);
        }
        return error(call_node, RECURSIVE_TYPE);
    }

    if(isAbstractFunctionGroup(expected)){
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return error(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return error(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
    }else{
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return error(call_node);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return error(call_node);
        }
    }

    return expected;
}

size_t TypeResolver::error(size_t pn, ErrorCode code) noexcept{
    if(retry_at_recursion) return RECURSIVE_CYCLE;

    errors.push_back(Error(parse_tree.getSelection(pn), code));
    return TypeResolver::FAILURE;
}

TypeResolver::ParseNode TypeResolver::getFuncFromCallSig(const CallSignature& sig) const noexcept{
    size_t decl_index = sig[0];
    const DeclareSignature& dec = declared(decl_index);

    return getFuncFromDeclSig(dec);
}

TypeResolver::ParseNode TypeResolver::getFuncFromDeclSig(const DeclareSignature& sig) const noexcept{
    return sig[0];
}

constexpr bool TypeResolver::isAbstractFunctionGroup(size_t type) noexcept {
    return type < FAILURE;
}

Type TypeResolver::declare(const DeclareSignature& fn){
    auto result = declared_func_map.insert({fn, declared_funcs.size()});
    if(result.second) declared_funcs.push_back(fn);

    return result.first->second;
}

Type TypeResolver::instantiate(const CallSignature& fn){
    auto lookup = called_func_map.find(fn);
    if(lookup != called_func_map.end()){
        Type return_type = lookup->second;
        retry_at_recursion |= (return_type == RECURSIVE_CYCLE && first_attempt); //revisited in process of instantiation
        return return_type;
    }

    if(recursion_fallback == nullptr)
        recursion_fallback = &fn;

    size_t old_val_cap_index = old_val_cap.size();
    size_t old_ref_cap_index = old_ref_cap.size();
    size_t old_args_index = old_args.size();

    //Instantiate
    //EVENTUALLY: in addition to type checking, should clone the function for type-specific operations refinement
    called_func_map[fn] = RECURSIVE_CYCLE;

    const DeclareSignature& dec = declared(fn[0]);
    ParseNode pn = dec[0];

    ParseNode val_list = parse_tree.valCapList(pn);
    ParseNode ref_list = parse_tree.refCapList(pn);
    ParseNode params = parse_tree.paramList(pn);
    ParseNode body = parse_tree.body(pn);
    size_t N_vals = parse_tree.valListSize(val_list);

    if(val_list != ParseTree::EMPTY){
        size_t scope_index = parse_tree.getFlag(val_list);
        const ScopeSegment& scope = symbol_table.scopes[scope_index];
        for(size_t i = 0; i < N_vals; i++){
            size_t sym_id = scope.sym_begin + i;
            Symbol& sym = symbol_table.symbols[sym_id];
            old_val_cap.push_back(sym.type);
            sym.type = dec[1+i];
        }
    }

    size_t j = 0;
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        size_t sym_id = parse_tree.getFlag(ref);
        Symbol& sym = symbol_table.symbols[sym_id];
        old_ref_cap.push_back(sym.type);
        sym.type = dec[1+N_vals+j];
        j++;
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getFlag(param);
        Symbol& sym = symbol_table.symbols[sym_id];
        old_args.push_back(sym.type);
        sym.type = fn[1+i];
    }

    Type return_type;

    bool is_alg = parse_tree.getOp(pn) != OP_LAMBDA;

    if(is_alg){
        return_types.push(UNINITIALISED);
        resolveStmt(body);
        return_type = return_types.top();
        if(return_type == UNINITIALISED) return_type = VOID; //Function with no return statement
        return_types.pop();
    }else{
        return_type = resolveExpr(body);
    }

    called_func_map[fn] = return_type;

    if(recursion_fallback == &fn){
        if(retry_at_recursion){
            retry_at_recursion = false;
            first_attempt = false;

            if(is_alg){
                return_types.push(UNINITIALISED);
                resolveStmt(body);
                return_type = return_types.top();
                if(return_type == UNINITIALISED) return_type = VOID; //Function with no return statement
                return_types.pop();
            }else{
                return_type = resolveExpr(body);
            }

            first_attempt = true;
            called_func_map[fn] = return_type;
        }

        recursion_fallback = nullptr;
    }else if(first_attempt && return_type == RECURSIVE_CYCLE){
        //Didn't work, be sure to try instantiation again
        called_func_map.erase(fn);
    }

    for(size_t i = 0; i < N_vals; i++){
        ParseNode val = parse_tree.arg(val_list, i);
        size_t sym_id = parse_tree.getFlag(val);
        Symbol& sym = symbol_table.symbols[sym_id];
        sym.type = old_val_cap[i+old_val_cap_index];
    }
    old_val_cap.resize(old_val_cap_index);

    j = 0;
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        size_t sym_id = parse_tree.getFlag(ref);
        Symbol& sym = symbol_table.symbols[sym_id];
        sym.type = old_ref_cap[j+old_ref_cap_index];
        j++;
    }
    old_ref_cap.resize(old_ref_cap_index);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getFlag(param);
        Symbol& sym = symbol_table.symbols[sym_id];
        sym.type = old_args[i+old_args_index];
    }
    old_args.resize(old_args_index);

    return return_type;
}

static constexpr std::string_view type_strs[] = {
    "Failure",
    "Recursive-Cycle",
    "Void",
    "Bool",
    "Str",
    "Num",
    "Unknown",
};

std::string TypeResolver::typeString(Type t) const{
    if(isAbstractFunctionGroup(t)){
        return abstractFunctionSetString(t);
    }else{
        return std::string(type_strs[t - FAILURE]);
    }
}

Type TypeResolver::makeFunctionSet(ParseNode pn) noexcept{
    std::vector<ParseNode> possible_fns = {pn};

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(1);
        push_back(pn);
    }

    return lookup.first->second;
}

Type TypeResolver::functionSetUnion(Type a, Type b){
    assert(v(a) == TYPE_FUNCTION_SET);
    assert(v(b) == TYPE_FUNCTION_SET);

    if(a == b) return a;

    std::vector<ParseNode> possible_fns;

    size_t last_a = last(a);
    size_t last_b = last(b);
    size_t index_a = first(a);
    size_t index_b = first(b);

    for(;;){
        ParseNode fn_a = v(index_a);
        ParseNode fn_b = v(index_b);

        if(fn_a <= fn_b){
            possible_fns.push_back(fn_a);
            index_a++;
            index_b += (fn_a == fn_b);
        }else{
            possible_fns.push_back(fn_b);
            index_b++;
        }

        if(index_a > last_a){
            while(index_b <= last_b) possible_fns.push_back(v(index_b++));
            break;
        }else if(index_b > last_b){
            while(index_a <= last_a) possible_fns.push_back(v(index_a++));
            break;
        }
    }

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(possible_fns.size());
        insert(end(), possible_fns.begin(), possible_fns.end());
    }

    return lookup.first->second;
}

size_t TypeResolver::numElements(size_t index) const noexcept{
    assert(v(index) == TYPE_FUNCTION_SET);

    return v(index+1);
}

TypeResolver::ParseNode TypeResolver::arg(size_t index, size_t n) const noexcept{
    assert(n < numElements(index));
    return v(index + 2 + n);
}

const TypeResolver::DeclareSignature& TypeResolver::declared(size_t index) const noexcept{
    assert(index < declared_funcs.size());
    return declared_funcs[index];
}

size_t TypeResolver::v(size_t index) const noexcept{
    return operator[](index);
}

size_t TypeResolver::first(size_t index) const noexcept{
    return index+2;
}

size_t TypeResolver::last(size_t index) const noexcept{
    index++;
    return index + v(index);
}

std::string TypeResolver::abstractFunctionSetString(Type t) const{
    assert(v(t) == TYPE_FUNCTION_SET);

    if(numElements(t) == 1) return declFunctionString( v(first(t)) );

    size_t index = first(t);
    std::string str = "{" + declFunctionString( v(index++) );
    while(index <= last(t))
        str += ", " + declFunctionString(v(index++));
    str += "}";

    return str;
}

std::string TypeResolver::declFunctionString(size_t i) const{
    assert(i < declared_funcs.size());
    const DeclareSignature& fn = declared_funcs[i];
    assert(!fn.empty());
    ParseNode pn = fn[0];
    std::string str = parse_tree.getOp(pn) == OP_LAMBDA ?
                      "λ" + std::to_string(pn):
                      parse_tree.str(parse_tree.algName(pn));
    str += '[';

    if(fn.size() >= 2){
        str += typeString(fn[1]);
        for(size_t i = 2; i < fn.size(); i++){
            str += ", ";
            str += typeString(fn[i]);
        }
    }

    str += ']';

    return str;
}

std::string TypeResolver::instFunctionString(const CallSignature& sig) const{
    std::string str = declFunctionString(sig[0]);
    str += "(";

    if(sig.size() >= 2){
        str += typeString(sig[1]);
        for(size_t i = 2; i < sig.size(); i++){
            str += ", ";
            str += typeString(sig[i]);
        }
    }

    str += ')';

    return str;
}

}

}
