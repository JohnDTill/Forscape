#include "hope_type_resolver.h"

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

TypeResolver::TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors) {}

void TypeResolver::resolve(){
    reset();
    resolveStmt(parse_tree.root);
}

void TypeResolver::reset() noexcept{
    clear();
    memoized_abstract_function_groups.clear();
    declared_funcs.clear();
    declared_func_map.clear();
    called_funcs.clear();
    called_func_map.clear();
    assert(return_types.empty());

    for(Symbol& sym : symbol_table.symbols)
        sym.type = TypeResolver::UNKNOWN;
}

void TypeResolver::resolveStmt(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) == TypeResolver::UNKNOWN){} /*DO THIS: DELETE THIS LINE*/ else \
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
                    EXPECT(parse_tree.arg(lhs, i), TypeResolver::NUMERIC)
                EXPECT(parse_tree.rhs(pn), TypeResolver::NUMERIC)
            }else{
                size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(parse_tree.lhs(pn)));
                Symbol& sym = symbol_table.symbols[sym_id];
                if(TypeResolver::isAbstractFunctionGroup(sym.type)){
                    Type t = resolveExpr(parse_tree.rhs(pn));
                    if(!TypeResolver::isAbstractFunctionGroup(t)){
                        error(parse_tree.rhs(pn));
                    }else{
                        sym.type = functionSetUnion(sym.type, t);
                    }
                }else EXPECT(parse_tree.rhs(pn), sym.type)
            }
            break;
        }

        case OP_RETURN:{
            //DO THIS - function groups can accumulate here

            assert(!return_types.empty());
            size_t expected = return_types.top();
            if(expected == TypeResolver::UNKNOWN){
                return_types.top() = resolveExpr(parse_tree.child(pn));
            }else if(TypeResolver::isAbstractFunctionGroup(expected)){
                Type t = resolveExpr(parse_tree.child(pn));
                if(!TypeResolver::isAbstractFunctionGroup(t)){
                    error(parse_tree.child(pn));
                }else{
                    return_types.top() = functionSetUnion(return_types.top(), t); //DO THIS - inefficient
                }
            }else{
                EXPECT(parse_tree.child(pn), expected)
            }
            break;
        }

        case OP_RETURN_EMPTY:{
            assert(!return_types.empty());
            if(return_types.top() == TypeResolver::UNKNOWN){
                return_types.top() = TypeResolver::VOID;
            }else if(return_types.top() != TypeResolver::VOID){
                error(pn);
            }
            break;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:{
            ParseNode lhs = parse_tree.lhs(pn);
            EXPECT(parse_tree.arg(lhs, 0), TypeResolver::NUMERIC)

            for(size_t i = 1; i < parse_tree.getNumArgs(lhs); i++){
                ParseNode sub = parse_tree.arg(lhs, i);
                if(parse_tree.getOp(sub) == OP_IDENTIFIER){
                    size_t sym_id = parse_tree.getFlag(sub);
                    Symbol& sym = symbol_table.symbols[sym_id];
                    sym.type = TypeResolver::NUMERIC;
                }else{
                    EXPECT(sub, TypeResolver::NUMERIC)
                }
            }

            EXPECT(parse_tree.rhs(pn), TypeResolver::NUMERIC)
            break;
        }

        case OP_IF:
        case OP_WHILE:
            EXPECT(parse_tree.lhs(pn), TypeResolver::BOOLEAN)
            resolveStmt(parse_tree.rhs(pn));
            break;

        case OP_IF_ELSE:
            EXPECT(parse_tree.arg(pn, 0), TypeResolver::BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            break;

        case OP_FOR:
            resolveStmt(parse_tree.arg(pn, 0));
            EXPECT(parse_tree.arg(pn, 1), TypeResolver::BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 2));
            resolveStmt(parse_tree.arg(pn, 3));
            break;

        case OP_EXPR_STMT:
            resolveExpr(parse_tree.child(pn));
            break;

        case OP_ALGORITHM:{
            ParseNode params = parse_tree.arg(pn, 3);
            for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getOp(param) == OP_EQUAL) resolveStmt(param);
            }

            TypeResolver::DeclareSignature sig;
            sig.push_back(pn);

            ParseNode capture_list = parse_tree.arg(pn, 1);
            if(capture_list != ParseTree::EMPTY){
                for(size_t i = 0; i < parse_tree.getNumArgs(capture_list); i++){
                    ParseNode cap = parse_tree.arg(capture_list, i);
                    size_t inner_id = parse_tree.getFlag(cap);
                    assert(inner_id < symbol_table.symbols.size());
                    const Symbol& inner = symbol_table.symbols[inner_id];
                    const Symbol& outer = symbol_table.symbols[inner.shadowed_var];
                    Type t = outer.type;
                    sig.push_back(t);
                }
            }

            //DO THIS - reference_list isn't created yet!
            /*
            ParseNode reference_list = parse_tree.arg(pn, 2);
            for(size_t i = 0; i < parse_tree.getNumArgs(reference_list); i++){
                ParseNode ref = parse_tree.arg(reference_list, i);
                size_t sym_id = parse_tree.getFlag(ref);
                Type t = symbol_table.symbols[sym_id].type;
                sig.push_back(t);
            }
            */

            Type t = declare(sig);

            size_t sym_id = parse_tree.getFlag(parse_tree.arg(pn, 0));
            symbol_table.symbols[sym_id].type = makeFunctionSet(t);

            break;
        }

        case OP_PROTOTYPE_ALG:{
            //DO THIS - you don't know what the algorithm captures!
            //This is easily resolved with a rule that you cannot have any path calling a prototyped alg
            //before it is defined, which is what the interpreter already did

            size_t sym_id = parse_tree.getFlag(parse_tree.arg(pn, 0));
            symbol_table.symbols[sym_id].type = TypeResolver::UNKNOWN;
            break;
        }

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveStmt(parse_tree.arg(pn, i));
            break;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveExpr(parse_tree.arg(pn, i));
            break;

        case OP_ASSERT:
            EXPECT(parse_tree.child(pn), TypeResolver::BOOLEAN)
            break;

        default:
            assert(false);
    }

    #undef EXPECT
}

Type TypeResolver::fillDefaultsAndInstantiate(TypeResolver::CallSignature sig){
    const TypeResolver::DeclareSignature& dec = declared(sig.front());
    ParseNode fn = dec.front();

    ParseNode params = parse_tree.getOp(fn) == OP_ALGORITHM ?
                       parse_tree.arg(fn, 3) :
                       parse_tree.arg(fn, 2);

    size_t n_params = parse_tree.getNumArgs(params);
    size_t n_args = sig.size()-1;
    if(n_args > n_params) return error(fn); //Too many args supplied at callsite

    //Resolve default args
    for(size_t i = n_args; i < n_params; i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) != OP_EQUAL) return error(param); //Too few args supplied at callsite
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
        if(resolveExpr(node) == TypeResolver::UNKNOWN) return TypeResolver::UNKNOWN; /*DO THIS: DELETE THIS LINE*/ else \
        if(resolveExpr(node) != expected) return error(node, code);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) == TypeResolver::UNKNOWN) return TypeResolver::UNKNOWN; /*DO THIS: DELETE THIS LINE*/ else \
        if(resolveExpr(node) != expected) return error(node);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA:{
            ParseNode params = parse_tree.arg(pn, 2);
            for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getOp(param) == OP_EQUAL) resolveStmt(param);
            }

            TypeResolver::DeclareSignature sig;
            sig.push_back(pn);

            ParseNode capture_list = parse_tree.arg(pn, 0);
            if(capture_list != ParseTree::EMPTY){
                for(size_t i = 0; i < parse_tree.getNumArgs(capture_list); i++){
                    ParseNode cap = parse_tree.arg(capture_list, i);
                    size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(cap));
                    Type t = symbol_table.symbols[sym_id].type;
                    sig.push_back(t);
                }
            }

            //DO THIS - reference_list isn't created yet!
            /*
            ParseNode reference_list = parse_tree.arg(pn, 1);
            for(size_t i = 0; i < parse_tree.getNumArgs(reference_list); i++){
                ParseNode ref = parse_tree.arg(reference_list, i);
                size_t sym_id = parse_tree.getFlag(ref);
                Type t = symbol_table.symbols[sym_id].type;
                sig.push_back(t);
            }
            */

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
            if(expected == UNKNOWN) return UNKNOWN; //DO THIS - delete
            if(expected != TypeResolver::NUMERIC && expected != TypeResolver::STRING) return error(parse_tree.arg(pn, 0), TYPE_NOT_ADDABLE);

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), expected)

            return expected;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT_OR(parse_tree.arg(pn, i), TypeResolver::NUMERIC, BAD_READ_OR_SUBSCRIPT);
            return TypeResolver::NUMERIC;
        case OP_SLICE_ALL:
            return TypeResolver::NUMERIC;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), TypeResolver::NUMERIC)
            return TypeResolver::NUMERIC;
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
            EXPECT(parse_tree.lhs(pn), TypeResolver::NUMERIC)
            EXPECT(parse_tree.rhs(pn), TypeResolver::NUMERIC)
            return TypeResolver::NUMERIC;
        case OP_LENGTH:
            EXPECT(parse_tree.child(pn), TypeResolver::NUMERIC)
            return TypeResolver::NUMERIC;
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
            EXPECT(parse_tree.child(pn), TypeResolver::NUMERIC)
            return TypeResolver::NUMERIC;
        case OP_SUMMATION:
        case OP_PRODUCT:{
            ParseNode asgn = parse_tree.arg(pn, 0);
            assert(parse_tree.getOp(asgn) == OP_ASSIGN);
            symbol_table.symbols[parse_tree.getFlag(parse_tree.lhs(asgn))].type = TypeResolver::NUMERIC;
            EXPECT(parse_tree.rhs(asgn), TypeResolver::NUMERIC)
            EXPECT(parse_tree.arg(pn, 1), TypeResolver::NUMERIC)
            EXPECT(parse_tree.arg(pn, 2), TypeResolver::NUMERIC)
            return TypeResolver::NUMERIC;
        }
        case OP_LOGICAL_NOT:
            EXPECT(parse_tree.child(pn), TypeResolver::BOOLEAN)
            return TypeResolver::BOOLEAN;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:
            EXPECT(parse_tree.lhs(pn), TypeResolver::BOOLEAN)
            EXPECT(parse_tree.rhs(pn), TypeResolver::BOOLEAN)
            return TypeResolver::BOOLEAN;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            EXPECT(parse_tree.lhs(pn), TypeResolver::NUMERIC)
            EXPECT(parse_tree.rhs(pn), TypeResolver::NUMERIC)
            return TypeResolver::BOOLEAN;
        case OP_CASES:{
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2)
                EXPECT(parse_tree.arg(pn, i), TypeResolver::BOOLEAN)

            size_t expected = resolveExpr(parse_tree.arg(pn, 0));
            if(TypeResolver::isAbstractFunctionGroup(expected))
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2){
                    Type t = resolveExpr(parse_tree.arg(pn, i));
                    if(!TypeResolver::isAbstractFunctionGroup(t)) return error(parse_tree.arg(pn, i));
                    expected = functionSetUnion(expected, t); //DO THIS - terribly inefficient
                }
            else
                for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2)
                    EXPECT(parse_tree.arg(pn, i), expected)
            return expected;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:{
            EXPECT(parse_tree.rhs(pn), resolveExpr(parse_tree.lhs(pn)))
            return TypeResolver::BOOLEAN;
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
            return TypeResolver::NUMERIC;
        case OP_FALSE:
        case OP_TRUE:
            return TypeResolver::BOOLEAN;
        case OP_STRING:
            return TypeResolver::STRING;
        default:
            assert(false);
            return TypeResolver::BOOLEAN;
    }
}

size_t TypeResolver::callSite(size_t pn) noexcept{
    ParseNode call_expr = parse_tree.arg(pn, 0);
    size_t node_size = parse_tree.getNumArgs(pn);
    size_t callable_type = resolveExpr(call_expr);

    if(callable_type == TypeResolver::UNKNOWN) return TypeResolver::UNKNOWN; //DO THIS: DELETE THIS LINE

    if(callable_type == TypeResolver::NUMERIC){
        bool is_mult = (node_size == 2 && resolveExpr(parse_tree.rhs(pn)) == TypeResolver::NUMERIC);
        return is_mult ? TypeResolver::NUMERIC : error(pn, NOT_CALLABLE);
    }else if(!TypeResolver::isAbstractFunctionGroup(callable_type)){
        return error(parse_tree.arg(pn, 0), NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(callable_type));
    sig.push_back(arg(callable_type, 0));

    for(size_t i = 1; i < node_size; i++){
        ParseNode arg = parse_tree.arg(pn, i);
        sig.push_back(resolveExpr(arg));
    }

    Type expected = fillDefaultsAndInstantiate(sig);
    if(isAbstractFunctionGroup(expected)){
        for(size_t i = 1; i < numElements(callable_type); i++){
            sig[0] = arg(callable_type, i);
            Type evaluated = fillDefaultsAndInstantiate(sig);
            if(evaluated == UNKNOWN) continue; //DO THIS - delete
            if(!isAbstractFunctionGroup(evaluated)) return error(callable_type);
            expected = functionSetUnion(expected, evaluated);
        }
    }else{
        for(size_t i = 1; i < numElements(callable_type); i++){
            sig[0] = arg(callable_type, i);
            Type evaluated = fillDefaultsAndInstantiate(sig);
            if(expected == UNKNOWN || evaluated == UNKNOWN) continue; //DO THIS - delete
            if(evaluated != expected) return error(callable_type);
        }
    }

    return expected;
}

size_t TypeResolver::implicitMult(size_t pn, size_t start) noexcept{
    ParseNode lhs = parse_tree.arg(pn, start);
    size_t tl = resolveExpr(lhs);

    if(tl == TypeResolver::UNKNOWN) return TypeResolver::UNKNOWN; //DO THIS: DELETE THIS LINE

    if(start == parse_tree.getNumArgs(pn)-1) return tl;
    else if(tl == TypeResolver::NUMERIC){
        if(implicitMult(pn, start+1) != TypeResolver::NUMERIC) return error(parse_tree.arg(pn, start+1));
        return TypeResolver::NUMERIC;
    }else if(!TypeResolver::isAbstractFunctionGroup(tl)){
        return error(lhs, NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(tl));
    sig.push_back(arg(tl, 0));

    Type tr = implicitMult(pn, start+1);
    sig.push_back(tr);

    Type expected = fillDefaultsAndInstantiate(sig);
    for(size_t i = 1; i < numElements(tl); i++){
        sig[0] = arg(tl, i);
        if(fillDefaultsAndInstantiate(sig) != expected) return error(pn);
    }

    return expected;
}

size_t TypeResolver::error(size_t pn, ErrorCode code) noexcept{
    errors.push_back(Error(parse_tree.getSelection(pn), code));
    return TypeResolver::FAILURE;
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
    if(lookup != called_func_map.end()) return lookup->second;

    //Instantiate
    //DO THIS - you're checking, but you should be actually cloning the function for type-specific operations
    //and you probably shouldn't just give the editor the final instantiation of multiple!
    called_funcs.push_back(fn);

    called_func_map[fn] = UNKNOWN; //DO THIS - handle recursion with dignity

    const DeclareSignature& dec = declared(fn[0]);
    ParseNode pn = dec[0];

    bool is_alg = parse_tree.getOp(pn) != OP_LAMBDA;
    if(is_alg) return_types.push(UNKNOWN);
    ParseNode value_list = parse_tree.arg(pn, is_alg);
    ParseNode ref_list = parse_tree.arg(pn, 1+is_alg);
    ParseNode params = parse_tree.arg(pn, 2+is_alg);
    ParseNode body = parse_tree.arg(pn, 3+is_alg);
    size_t N_vals = value_list == ParseTree::EMPTY ?
                    0 :
                    parse_tree.getNumArgs(value_list);

    if(value_list != ParseTree::EMPTY){
        size_t scope_index = parse_tree.getFlag(value_list);
        const ScopeSegment& scope = symbol_table.scopes[scope_index];
        for(size_t i = 0; i < N_vals; i++){
            size_t sym_id = scope.sym_begin + i;
            symbol_table.symbols[sym_id].type = dec[1+i];
        }
    }

    //DO THIS - reference_list isn't created yet!
    /*
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        size_t sym_id = parse_tree.getFlag(ref);
        symbol_table.symbols[sym_id].type = dec[1+N_vals+i];
    }
    */

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getFlag(param);
        symbol_table.symbols[sym_id].type = fn[1+i];
    }

    Type return_type;

    if(is_alg){
        resolveStmt(body);
        return_type = return_types.top();
        return_types.pop();
    }else{
        return_type = resolveExpr(body);
    }

    called_func_map[fn] = return_type;

    return return_type;
}

static constexpr std::string_view type_strs[] = {
    "Failure",
    "Void",
    "Boolean",
    "String",
    "Numeric",
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

    size_t index = first(t);
    std::string str = "{" + declFunctionString( v(index++) );
    while(index <= last(t))
        str += ", " + declFunctionString(v(index++));
    str += "} : AbstractFunctionSet";

    return str;
}

std::string TypeResolver::declFunctionString(size_t i) const{
    assert(i < declared_funcs.size());
    const DeclareSignature& fn = declared_funcs[i];
    assert(!fn.empty());
    ParseNode pn = fn[0];
    std::string str = parse_tree.getOp(pn) == OP_LAMBDA ?
                      "λ" + std::to_string(pn):
                      parse_tree.str(parse_tree.arg(pn, 0));
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

}

}
