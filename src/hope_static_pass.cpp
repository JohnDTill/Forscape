#include "hope_static_pass.h"

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"

namespace Hope {

namespace Code {

StaticPass::StaticPass(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors, std::vector<Error>& warnings) noexcept
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors), warnings(warnings) {}

void StaticPass::resolve(){
    reset();
    if(!errors.empty()) return;
    parse_tree.root = resolveStmt(parse_tree.root);

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

    //EVENTUALLY: replace this with something less janky
    for(const Usage& usage : symbol_table.usages){
        const Symbol& sym = symbol_table.symbols[usage.var_id];

        if(sym.type == StaticPass::NUMERIC && (sym.rows != 1 || sym.cols != 1)){
            Typeset::Selection sel = parse_tree.getSelection(usage.pn);
            switch (sel.getFormat()) {
                case SEM_ID: sel.format(SEM_ID_MAT); break;
                case SEM_PREDEF: sel.format(SEM_PREDEFINEDMAT); break;
                case SEM_ID_FUN_IMPURE: sel.format(SEM_ID_MAT_IMPURE); break;
                default: break;
            }
        }
    }
}

void StaticPass::reset() noexcept{
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

ParseNode StaticPass::resolveStmt(size_t pn) noexcept{
    if(!errors.empty()) return pn;

    assert(pn != ParseTree::EMPTY);

    #define EXPECT_TYPE(node, expected) \
        if(parse_tree.getType(node) != expected){ \
            error(node); \
            return pn; \
        }

    switch (parse_tree.getOp(pn)) {
        case OP_ASSIGN:
        case OP_EQUAL:{
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            size_t sym_id = parse_tree.getFlag(parse_tree.lhs(pn));
            Symbol& sym = symbol_table.symbols[sym_id];
            sym.type = parse_tree.getType(rhs);
            sym.rows = parse_tree.getRows(rhs);
            sym.cols = parse_tree.getCols(rhs);
            return pn;
        }
        case OP_REASSIGN:{
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            ParseNode lhs = parse_tree.lhs(pn);
            if(parse_tree.getOp(lhs) == OP_SUBSCRIPT_ACCESS){
                for(size_t i = 0; i < parse_tree.getNumArgs(lhs); i++){
                    ParseNode arg = resolveExpr(parse_tree.arg(lhs, i));
                    parse_tree.setArg(lhs, i, arg);
                    if(parse_tree.getType(arg) != NUMERIC) return error(arg);
                }
                if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);

                parse_tree.setType(pn, OP_REASSIGN_SUBSCRIPT);
                return pn;
            }else{
                size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(parse_tree.lhs(pn)));
                Symbol& sym = symbol_table.symbols[sym_id];
                if(isAbstractFunctionGroup(sym.type)){
                    Type t = parse_tree.getType(rhs);
                    if(!isAbstractFunctionGroup(t)){
                        return error(parse_tree.rhs(pn));
                    }else{
                        sym.type = functionSetUnion(sym.type, t);
                        return pn;
                    }
                }else{
                    if(parse_tree.getType(rhs) != sym.type) return error(rhs);
                    return pn;
                }
            }
        }

        case OP_RETURN:{
            assert(!return_types.empty());
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            Type child_type = parse_tree.getType(child);
            Type expected = return_types.top();
            if(expected == UNINITIALISED || expected == RECURSIVE_CYCLE){
                return_types.top() = child_type;
            }else if(isAbstractFunctionGroup(expected)){
                if(!isAbstractFunctionGroup(child_type)){
                    error(child);
                }else{
                    return_types.top() = functionSetUnion(expected, child_type);
                }
            }else{
                EXPECT_TYPE(child, expected)
            }
            return pn;
        }

        case OP_RETURN_EMPTY:{
            assert(!return_types.empty());
            if(return_types.top() == UNINITIALISED){
                return_types.top() = VOID_TYPE;
            }else if(return_types.top() != VOID_TYPE){
                error(pn);
            }
            return pn;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:{
            ParseNode lhs = parse_tree.lhs(pn);
            ParseNode var = resolveExpr(parse_tree.arg<0>(lhs));
            parse_tree.setArg<0>(lhs, var);
            if(parse_tree.getType(var) != NUMERIC) return error(var);

            for(size_t i = 1; i < parse_tree.getNumArgs(lhs); i++){
                ParseNode sub = parse_tree.arg(lhs, i);
                if(parse_tree.getOp(sub) == OP_IDENTIFIER){
                    size_t sym_id = parse_tree.getFlag(sub);
                    Symbol& sym = symbol_table.symbols[sym_id];
                    sym.type = NUMERIC;
                }else{
                    sub = resolveExpr(sub);
                    parse_tree.setArg(lhs, i, sub);
                    if(parse_tree.getType(sub) != NUMERIC) return error(sub);
                }
            }

            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);

            return pn;
        }

        case OP_IF:
        case OP_WHILE:{
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            EXPECT_TYPE(lhs, BOOLEAN)

            parse_tree.setArg<1>(pn, resolveStmt(parse_tree.arg<1>(pn)));
            return pn;
        }

        case OP_IF_ELSE:{
            ParseNode cond = resolveExpr(parse_tree.arg<0>(pn));
            parse_tree.setArg<0>(pn, cond);
            EXPECT_TYPE(cond, BOOLEAN)

            parse_tree.setArg<1>(pn, resolveStmt(parse_tree.arg<1>(pn)));
            parse_tree.setArg<2>(pn, resolveStmt(parse_tree.arg<2>(pn)));
            return pn;
        }

        case OP_FOR:{
            parse_tree.setArg<0>(pn, resolveStmt(parse_tree.arg<0>(pn)));
            ParseNode cond = resolveExpr(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, cond);
            EXPECT_TYPE(cond, BOOLEAN)
            parse_tree.setArg<2>(pn, resolveStmt(parse_tree.arg<2>(pn)));
            parse_tree.setArg<3>(pn, resolveStmt(parse_tree.arg<3>(pn)));
            return pn;
        }

        case OP_EXPR_STMT:{
            ParseNode expr = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, expr);
            if(parse_tree.getOp(expr) != OP_CALL || !isAbstractFunctionGroup(parse_tree.getType(parse_tree.arg<0>(expr)))){
                warnings.push_back(Error(parse_tree.getSelection(expr), ErrorCode::UNUSED_EXPRESSION));
                parse_tree.setOp(pn, OP_DO_NOTHING);
                //EVENTUALLY: check the call stmt has side effects
            }
            return pn;
        }

        case OP_ALGORITHM: return resolveAlg(pn);

        case OP_PROTOTYPE_ALG:{
            size_t sym_id = parse_tree.getFlag(parse_tree.arg(pn, 0));
            symbol_table.symbols[sym_id].type = UNINITIALISED;
            return pn;
        }

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode stmt = resolveStmt(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, stmt);
            }
            return pn;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode expr = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, expr);
                //Specialise printing for type
            }
            return pn;

        case OP_ASSERT:{
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            EXPECT_TYPE(child, BOOLEAN)
            return pn;
        }

        default:
            assert(false);
            return pn;
    }

    #undef EXPECT_TYPE
}

Type StaticPass::fillDefaultsAndInstantiate(ParseNode call_node, StaticPass::CallSignature sig){
    const StaticPass::DeclareSignature& dec = declared(sig.front());
    ParseNode fn = dec.front();

    ParseNode params = parse_tree.paramList(fn);

    size_t n_params = parse_tree.getNumArgs(params);
    size_t n_args = sig.size()-1;
    if(n_args > n_params) return errorType(call_node, TOO_MANY_ARGS);

    //Resolve default args
    for(size_t i = n_args; i < n_params; i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) != OP_EQUAL) return errorType(call_node, TOO_FEW_ARGS);
        ParseNode default_var = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getFlag(default_var);
        Type t = symbol_table.symbols[sym_id].type;
        sig.push_back(t);
    }

    return instantiate(sig);
}

ParseNode StaticPass::resolveExpr(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    if(!errors.empty()) return pn;

    #define EXPECT_OR(node, expected, code) \
        if(resolveExpr(node) != expected) return error(node, code);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) != expected) return error(node);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA: return resolveLambda(pn);
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:{
            ParseNode var = parse_tree.getFlag(pn);
            size_t sym_id = parse_tree.getFlag(var);
            Symbol& sym = symbol_table.symbols[sym_id];
            parse_tree.setRows(pn, sym.rows);
            parse_tree.setCols(pn, sym.cols);
            parse_tree.setType(pn, sym.type);

            return pn;
        }
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            ParseNode arg0 = resolveExpr(parse_tree.arg(pn, 0));
            parse_tree.setArg<0>(pn, arg0);
            Type expected = parse_tree.getType(arg0);
            if(expected != NUMERIC && expected != STRING) return error(arg0, TYPE_NOT_ADDABLE);
            parse_tree.setType(pn, expected);

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != expected) return error(arg, TYPE_NOT_ADDABLE);
            }

            return pn;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != NUMERIC) return error(arg, BAD_READ_OR_SUBSCRIPT);
                if(dimsDisagree(parse_tree.getRows(arg), 1)) return error(arg, DIMENSION_MISMATCH);
                if(dimsDisagree(parse_tree.getCols(arg), 1)) return error(arg, DIMENSION_MISMATCH);
            }
            parse_tree.setScalar(pn);
            return pn;
        case OP_SLICE_ALL:
            parse_tree.setScalar(pn);
            return pn;
        case OP_MATRIX: return resolveMatrix(pn);
        case OP_MULTIPLICATION: return resolveMult(pn);
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != NUMERIC) return error(arg);
            }
            parse_tree.setType(pn, NUMERIC);
            return pn;
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:{
            ParseNode expr = resolveExpr(parse_tree.child(pn));
            parse_tree.copyDims(pn, expr);
            return expr;
        }
        case OP_POWER: return resolvePower(pn);
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
        case OP_ROOT:
        case OP_SUBTRACTION:
        case OP_ZERO_MATRIX:{
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);
            parse_tree.setType(pn, NUMERIC);
            return pn;
        }
        case OP_LENGTH:{
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != NUMERIC) return error(child);
            return pn;
        }
        case OP_ARCCOSINE:
        case OP_ARCCOTANGENT:
        case OP_ARCSINE:
        case OP_ARCTANGENT:
        case OP_BIJECTIVE_MAPPING:
        case OP_COMP_ERR_FUNC:
        case OP_COSINE:
        case OP_DAGGER:
        case OP_ERROR_FUNCTION:
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
        {
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            if(parse_tree.getType(child) != NUMERIC) return error(child);
            return pn;
        }
        case OP_TRANSPOSE:{
            ParseNode child = resolveExpr(parse_tree.child(pn));
            if(parse_tree.getType(child) != NUMERIC) return error(child);
            parse_tree.setType(pn, NUMERIC);
            parse_tree.transposeDims(pn, child);
            if(parse_tree.definitelyScalar(child)) return child;
            return pn;
        }
        case OP_UNARY_MINUS: return resolveUnaryMinus(pn);
        case OP_SUMMATION:
        case OP_PRODUCT:{
            ParseNode asgn = parse_tree.arg<0>(pn);
            assert(parse_tree.getOp(asgn) == OP_ASSIGN);
            symbol_table.symbols[parse_tree.getFlag(parse_tree.lhs(asgn))].type = NUMERIC;
            ParseNode initialiser = resolveExpr(parse_tree.rhs(asgn));
            parse_tree.setArg<1>(asgn, initialiser);
            if(parse_tree.getType(initialiser) != NUMERIC) return error(initialiser);
            parse_tree.setScalar(parse_tree.lhs(asgn));

            ParseNode final = resolveExpr(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, final);
            if(parse_tree.getType(final) != NUMERIC) return error(initialiser);

            ParseNode body = resolveExpr(parse_tree.arg<2>(pn));
            parse_tree.setArg<2>(pn, body);
            if(parse_tree.getType(body) != NUMERIC) return error(body);
            if(parse_tree.getOp(pn) == OP_PRODUCT && parse_tree.nonSquare(body)) return error(body, DIMENSION_MISMATCH);

            parse_tree.setType(pn, NUMERIC);
            parse_tree.copyDims(pn, body);
            return pn;
        }
        case OP_LOGICAL_NOT:{
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != BOOLEAN) return error(child);
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        }
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:{
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != BOOLEAN) return error(lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != BOOLEAN) return error(rhs);
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        }
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:{
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        }
        case OP_CASES:{
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2){
                ParseNode cond = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, cond);
                if(parse_tree.getType(cond) != BOOLEAN) return error(cond, EXPECT_BOOLEAN);
            }

            ParseNode arg0 = resolveExpr(parse_tree.arg(pn, 0));
            parse_tree.setArg<0>(pn, arg0);
            Type expected = parse_tree.getType(arg0);
            if(isAbstractFunctionGroup(expected))
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2){
                    ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                    parse_tree.setArg(pn, i , arg);
                    Type t = parse_tree.getType(arg);
                    if(!isAbstractFunctionGroup(t)) return error(arg);
                    expected = functionSetUnion(expected, t);
                }
            else
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2){
                    ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                    parse_tree.setArg(pn, i , arg);
                    Type t = parse_tree.getType(arg);
                    if(t != expected) return error(arg);
                }
            parse_tree.setType(pn, expected);
            return pn;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:{
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(lhs) != parse_tree.getType(rhs)) return error(pn);
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        }
        case OP_DECIMAL_LITERAL:
        case OP_INTEGER_LITERAL:{
            double val = stod(parse_tree.str(pn));
            parse_tree.setFlag(pn, val);
            parse_tree.setScalar(pn);
            parse_tree.setValue(pn, val);
            return pn;
        }
        case OP_PI:
        case OP_EULERS_NUMBER:
        case OP_GOLDEN_RATIO:
        case OP_SPEED_OF_LIGHT:
        case OP_PLANCK_CONSTANT:
        case OP_REDUCED_PLANCK_CONSTANT:
        case OP_STEFAN_BOLTZMANN_CONSTANT:
        case OP_IDENTITY_AUTOSIZE:
        case OP_GRAVITY:
            parse_tree.setType(pn, NUMERIC);
            return pn;
        case OP_FALSE:
        case OP_TRUE:
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        case OP_STRING:
            parse_tree.setType(pn, STRING);
            return pn;
        case OP_PROXY:
            return resolveExpr(parse_tree.getFlag(pn));
        case OP_INVERT:
        case OP_LINEAR_SOLVE:
            return pn;
        case OP_NORM_SQUARED:
            parse_tree.setScalar(pn);
            return pn;
        default:
            assert(false);
            return BOOLEAN;
    }
}

size_t StaticPass::callSite(size_t pn) noexcept{
    ParseNode call_expr = resolveExpr(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, call_expr);
    size_t node_size = parse_tree.getNumArgs(pn);
    Type callable_type = parse_tree.getType(call_expr);

    if(callable_type == NUMERIC){
        if(node_size == 2){
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) == NUMERIC){
                parse_tree.setOp(pn, OP_MULTIPLICATION);
                return resolveMult(pn);
            }else{
                return error(rhs);
            }
        }else{
            return error(pn, NOT_CALLABLE);
        }
    }else if(callable_type == UNINITIALISED){
        return error(call_expr, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(callable_type)){
        return error(call_expr, NOT_CALLABLE);
    }

    CallSignature sig;
    assert(numElements(callable_type));
    sig.push_back(arg(callable_type, 0));

    for(size_t i = 1; i < node_size; i++){
        ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
        parse_tree.setArg(pn, i, arg);
        sig.push_back(parse_tree.getType(arg));
    }

    parse_tree.setType(pn, instantiateSetOfFuncs(pn, callable_type, sig));
    return pn;
}

size_t StaticPass::implicitMult(size_t pn, size_t start) noexcept{
    ParseNode lhs = resolveExpr(parse_tree.arg(pn, start));
    parse_tree.setArg(pn, start, lhs);
    Type tl = parse_tree.getType(lhs);

    if(start == parse_tree.getNumArgs(pn)-1) return lhs;
    else if(tl == NUMERIC){
        ParseNode rhs = implicitMult(pn, start+1);
        if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);
        return resolveExpr(parse_tree.addBinary(OP_MULTIPLICATION, lhs, rhs));
    }else if(tl == UNINITIALISED){
        return error(lhs, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(tl)){
        return error(lhs, NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(tl));
    sig.push_back(arg(tl, 0));

    ParseNode rhs = implicitMult(pn, start+1);
    Type tr = parse_tree.getType(rhs);
    sig.push_back(tr);

    ParseNode call = parse_tree.addBinary(OP_CALL, lhs, rhs);
    parse_tree.setType(call, instantiateSetOfFuncs(pn, tl, sig));

    return call;
}

size_t StaticPass::instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig){
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
            errorType(fn, RECURSIVE_TYPE);
        }
        return errorType(call_node, RECURSIVE_TYPE);
    }

    if(isAbstractFunctionGroup(expected)){
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return errorType(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return errorType(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
    }else{
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return errorType(call_node);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return errorType(call_node);
        }
    }

    return expected;
}

size_t StaticPass::error(ParseNode pn, ErrorCode code) noexcept{
    if(retry_at_recursion) parse_tree.setType(pn, RECURSIVE_CYCLE);
    else if(errors.empty()) errors.push_back(Error(parse_tree.getSelection(pn), code));
    return pn;
}

size_t StaticPass::errorType(ParseNode pn, ErrorCode code) noexcept{
    if(retry_at_recursion) return RECURSIVE_CYCLE;

    if(errors.empty()) errors.push_back(Error(parse_tree.getSelection(pn), code));
    return FAILURE;
}

StaticPass::ParseNode StaticPass::getFuncFromCallSig(const CallSignature& sig) const noexcept{
    size_t decl_index = sig[0];
    const DeclareSignature& dec = declared(decl_index);

    return getFuncFromDeclSig(dec);
}

ParseNode StaticPass::getFuncFromDeclSig(const DeclareSignature& sig) const noexcept{
    return sig[0];
}

StaticPass::ParseNode StaticPass::resolveAlg(ParseNode pn){
    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            parse_tree.setArg(params, i, resolveStmt(param));
    }

    DeclareSignature sig;
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

    Type t = makeFunctionSet(declare(sig));

    size_t sym_id = parse_tree.getFlag(parse_tree.algName(pn));
    symbol_table.symbols[sym_id].type = t;
    parse_tree.setType(pn, t);

    return pn;
}

ParseNode StaticPass::resolveInverse(ParseNode pn){
    ParseNode child = parse_tree.child(pn);
    if(dimsDisagree(parse_tree.getRows(child), parse_tree.getCols(child))) return error(pn, DIMENSION_MISMATCH);
    parse_tree.copyDims(pn, child);
    return pn;
}

ParseNode StaticPass::resolveLambda(ParseNode pn){
    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            parse_tree.setArg(params, i, resolveStmt(param));
    }

    DeclareSignature sig;
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

    Type t = makeFunctionSet(declare(sig));
    parse_tree.setType(pn, t);

    return pn;
}

StaticPass::ParseNode StaticPass::resolveMatrix(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);
    size_t nargs = parse_tree.getNumArgs(pn);
    for(size_t i = 0; i < nargs; i++){
        ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
        parse_tree.setArg(pn, i, arg);
        if(parse_tree.getType(arg) != NUMERIC) return error(arg);
    }

    if(nargs == 1) return copyChildProperties(pn);

    size_t typeset_rows = parse_tree.getFlag(pn);
    size_t typeset_cols = nargs/typeset_rows;
    assert(typeset_cols*typeset_rows == nargs);

    //EVENTUALLY: break nested allocation
    std::vector<size_t> elem_cols; elem_cols.resize(typeset_cols);
    std::vector<size_t> elem_rows; elem_rows.resize(typeset_rows);
    std::vector<ParseNode> elements; elements.resize(nargs);

    size_t curr = 0;
    for(size_t i = 0; i < typeset_rows; i++){
        for(size_t j = 0; j < typeset_cols; j++){
            ParseNode arg = parse_tree.arg(pn, curr);

            if(i==0) elem_cols[j] = parse_tree.getCols(arg);
            else if(elem_cols[j] == UNKNOWN_SIZE) elem_cols[j] = parse_tree.getCols(arg);
            else if(dimsDisagree(elem_cols[j], parse_tree.getCols(arg))) return error(pn);

            if(j==0) elem_rows[i] = parse_tree.getRows(arg);
            else if(elem_rows[i] == UNKNOWN_SIZE) elem_rows[i] = parse_tree.getRows(arg);
            else if(dimsDisagree(elem_rows[i], parse_tree.getRows(arg))) return error(pn);
        }
    }

    size_t rows = 0;
    for(auto count : elem_rows)
        if(count != UNKNOWN_SIZE){
            rows += count;
        }else{
            rows = UNKNOWN_SIZE;
            break;
        }

    size_t cols = 0;
    for(auto count : elem_cols)
        if(count != UNKNOWN_SIZE){
            cols += count;
        }else{
            cols = UNKNOWN_SIZE;
            break;
        }

    parse_tree.setRows(pn, rows);
    parse_tree.setCols(pn, cols);

    return pn;
}

ParseNode StaticPass::resolveMult(ParseNode pn){
    ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
    parse_tree.setArg<0>(pn, lhs);
    if(parse_tree.getType(lhs) != NUMERIC) return error(lhs);
    ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
    parse_tree.setArg<1>(pn, rhs);
    if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);
    parse_tree.setType(pn, NUMERIC);

    if(parse_tree.definitelyScalar(lhs)){
        parse_tree.copyDims(pn, rhs);
    }else if(parse_tree.definitelyScalar(rhs)){
        parse_tree.copyDims(pn, lhs);
    }else if(parse_tree.definitelyNotScalar(lhs) && parse_tree.definitelyNotScalar(rhs)){
        parse_tree.setRows(pn, parse_tree.getRows(lhs));
        parse_tree.setCols(pn, parse_tree.getCols(rhs));
        if(dimsDisagree(parse_tree.getCols(lhs), parse_tree.getRows(rhs))) return error(pn, DIMENSION_MISMATCH);
    }

    if(parse_tree.getOp(lhs) == OP_INVERT){
        parse_tree.setArg(pn, 0, parse_tree.child(lhs));
        parse_tree.setOp(pn, OP_LINEAR_SOLVE);
    }

    return pn;
}

ParseNode StaticPass::resolvePower(ParseNode pn){
    ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
    parse_tree.setArg<0>(pn, lhs);
    if(parse_tree.getType(lhs) != NUMERIC) return error(lhs);
    ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
    parse_tree.setArg<1>(pn, rhs);
    if(parse_tree.getType(rhs) != NUMERIC) return error(rhs);
    parse_tree.setType(pn, NUMERIC);

    if(parse_tree.definitelyScalar(rhs)){
        parse_tree.copyDims(pn, lhs);
    }

    switch (parse_tree.getOp(rhs)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:{
            double rhs_val = parse_tree.getFlagAsDouble(rhs);

            ParseNode lhs = parse_tree.lhs(pn);
            Code::Op lhs_op = parse_tree.getOp(lhs);
            if(lhs_op == OP_INTEGER_LITERAL || lhs_op == OP_DECIMAL_LITERAL){
                double val = pow(parse_tree.getFlagAsDouble(lhs), rhs_val);
                parse_tree.setFlag(pn, val);
                parse_tree.setOp(pn, OP_DECIMAL_LITERAL);
                parse_tree.setNumArgs(pn, 0);
                parse_tree.setScalar(pn);
                return pn;
            }else if(rhs_val == -1){
                parse_tree.setOp(pn, OP_INVERT);
                parse_tree.setNumArgs(pn, 1);
                return resolveInverse(pn);
            }else if(rhs_val == 1){
                return parse_tree.lhs(pn);
            }else if(rhs_val == 2){
                if(lhs_op == OP_NORM){
                    parse_tree.setOp(lhs, OP_NORM_SQUARED);
                    return lhs;
                }
            }
            //Can only simplify x^0 to 1 assuming no NaN in LHS
        }
    }

    return pn;
}

ParseNode StaticPass::resolveUnaryMinus(ParseNode pn){
    ParseNode child = resolveExpr(parse_tree.child(pn));
    parse_tree.setArg<0>(pn, child);
    parse_tree.copyDims(pn, child);
    if(parse_tree.getType(child) != NUMERIC) return error(pn);
    parse_tree.setType(pn, NUMERIC);

    switch (parse_tree.getOp(child)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            parse_tree.setFlag(child, -parse_tree.getFlagAsDouble(child));
            return child;
        case OP_UNARY_MINUS:
            return parse_tree.child(child);
        default:
            parse_tree.setRows(pn, parse_tree.getRows(child));
            parse_tree.setCols(pn, parse_tree.getCols(child));
            return pn;
    }
}

ParseNode StaticPass::copyChildProperties(ParseNode pn) noexcept{
    ParseNode child = parse_tree.child(pn);
    parse_tree.setRows(pn, parse_tree.getRows(child));
    parse_tree.setCols(pn, parse_tree.getCols(child));
    parse_tree.setValue(pn, parse_tree.getValue(child));

    return child;
}

bool StaticPass::dimsDisagree(size_t a, size_t b) noexcept{
    return a != UNKNOWN_SIZE && b != UNKNOWN_SIZE && a != b;
}

constexpr bool StaticPass::isAbstractFunctionGroup(size_t type) noexcept {
    return type < FAILURE;
}

Type StaticPass::declare(const DeclareSignature& fn){
    auto result = declared_func_map.insert({fn, declared_funcs.size()});
    if(result.second) declared_funcs.push_back(fn);

    return result.first->second;
}

Type StaticPass::instantiate(const CallSignature& fn){
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
    called_func_map[fn] = RECURSIVE_CYCLE;

    const DeclareSignature& dec = declared(fn[0]);
    ParseNode pn = parse_tree.clone(dec[0]);
    //ParseNode pn = dec[0]; //DO THIS - delete and get instantiation working

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
        parse_tree.setType(param, fn[1+i]);
    }

    Type return_type;

    bool is_alg = parse_tree.getOp(pn) != OP_LAMBDA;

    if(is_alg){
        return_types.push(UNINITIALISED);
        resolveStmt(body);
        return_type = return_types.top();
        if(return_type == UNINITIALISED) return_type = VOID_TYPE; //Function with no return statement
        return_types.pop();
    }else{
        resolveExpr(body);
        return_type = parse_tree.getType(body);
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
                if(return_type == UNINITIALISED) return_type = VOID_TYPE; //Function with no return statement
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

std::string StaticPass::typeString(Type t) const{
    if(isAbstractFunctionGroup(t)){
        return abstractFunctionSetString(t);
    }else{
        return std::string(type_strs[t - FAILURE]);
    }
}

static void toSuperscript(const std::string& str, std::string& out){
    for(size_t i = 0; i < str.size(); i++){
        switch (str[i]) {
            case '0': out += "⁰"; break;
            case '1': out += "¹"; break;
            case '2': out += "²"; break;
            case '3': out += "³"; break;
            case '4': out += "⁴"; break;
            case '5': out += "⁵"; break;
            case '6': out += "⁶"; break;
            case '7': out += "⁷"; break;
            case '8': out += "⁸"; break;
            case '9': out += "⁹"; break;
            default: assert(false);
        }
    }
}

std::string StaticPass::typeString(const Symbol& sym) const{
    if(sym.type == NUMERIC){
        std::string str = "ℝ";
        if(sym.rows == 1 && sym.cols == 1) return str;

        if(sym.rows == UNKNOWN_SIZE){
            str += "ⁿ";
        }else{
            std::string dim = std::to_string(sym.rows);
            toSuperscript(dim, str);
        }

        if(sym.cols == 1) return str;

        str += "˟";
        if(sym.cols == UNKNOWN_SIZE){
            str += "ᵐ";
        }else{
            std::string dim = std::to_string(sym.cols);
            toSuperscript(dim, str);
        }

        return str;
    }else{
        return typeString(sym.type);
    }
}

Type StaticPass::makeFunctionSet(ParseNode pn) noexcept{
    std::vector<ParseNode> possible_fns = {pn};

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(1);
        push_back(pn);
    }

    return lookup.first->second;
}

Type StaticPass::functionSetUnion(Type a, Type b){
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

size_t StaticPass::numElements(size_t index) const noexcept{
    assert(v(index) == TYPE_FUNCTION_SET);

    return v(index+1);
}

StaticPass::ParseNode StaticPass::arg(size_t index, size_t n) const noexcept{
    assert(n < numElements(index));
    return v(index + 2 + n);
}

const StaticPass::DeclareSignature& StaticPass::declared(size_t index) const noexcept{
    assert(index < declared_funcs.size());
    return declared_funcs[index];
}

size_t StaticPass::v(size_t index) const noexcept{
    return operator[](index);
}

size_t StaticPass::first(size_t index) const noexcept{
    return index+2;
}

size_t StaticPass::last(size_t index) const noexcept{
    index++;
    return index + v(index);
}

std::string StaticPass::abstractFunctionSetString(Type t) const{
    assert(v(t) == TYPE_FUNCTION_SET);

    if(numElements(t) == 1) return declFunctionString( v(first(t)) );

    size_t index = first(t);
    std::string str = "{" + declFunctionString( v(index++) );
    while(index <= last(t))
        str += ", " + declFunctionString(v(index++));
    str += "}";

    return str;
}

std::string StaticPass::declFunctionString(size_t i) const{
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

std::string StaticPass::instFunctionString(const CallSignature& sig) const{
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