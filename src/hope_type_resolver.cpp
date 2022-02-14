#include "hope_type_resolver.h"

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_symbol_table.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

std::vector<size_t> TypeResolver::function_type_pool;
std::vector<TypeResolver::FuncSignature> TypeResolver::function_sig_pool;
std::unordered_set<size_t, TypeResolver::FuncSignatureHash, TypeResolver::FuncSignatureEqual> TypeResolver::memoized_signatures;

TypeResolver::TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors) {}

void TypeResolver::resolve(){
    reset();
    resolveStmt(parse_tree.root);
}

void TypeResolver::reset() noexcept{
    for(Symbol& sym : symbol_table.symbols)
        sym.type = TYPE_UNKNOWN;
}

void TypeResolver::resolveStmt(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) == TYPE_UNKNOWN){} /*DO THIS: DELETE THIS LINE*/ else \
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
            //DO THIS - function groups can accumulate here

            ParseNode lhs = parse_tree.lhs(pn);
            if(parse_tree.getOp(lhs) == OP_SUBSCRIPT_ACCESS){
                for(size_t i = 0; i < parse_tree.getNumArgs(lhs); i++)
                    EXPECT(parse_tree.arg(lhs, i), TYPE_NUMERIC)
                EXPECT(parse_tree.rhs(pn), TYPE_NUMERIC)
            }else{
                size_t sym_id = parse_tree.getFlag(parse_tree.getFlag(parse_tree.lhs(pn)));
                Symbol& sym = symbol_table.symbols[sym_id];
                EXPECT(parse_tree.rhs(pn), sym.type)
            }
            break;
        }

        case OP_RETURN:{
            //DO THIS - function groups can accumulate here

            assert(!return_types.empty());
            size_t expected = return_types.top();
            if(expected == TYPE_UNKNOWN){
                return_types.top() = resolveExpr(parse_tree.child(pn));
            }else{
                EXPECT(parse_tree.child(pn), expected)
            }
            break;
        }

        case OP_RETURN_EMPTY:{
            assert(!return_types.empty());
            if(return_types.top() == TYPE_UNKNOWN){
                return_types.top() = TYPE_VOID;
            }else if(return_types.top() != TYPE_VOID){
                error(pn);
            }
            break;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:{
            ParseNode lhs = parse_tree.lhs(pn);
            EXPECT(parse_tree.arg(lhs, 0), TYPE_NUMERIC)

            for(size_t i = 1; i < parse_tree.getNumArgs(lhs); i++){
                ParseNode sub = parse_tree.arg(lhs, i);
                if(parse_tree.getOp(sub) == OP_IDENTIFIER){
                    size_t sym_id = parse_tree.getFlag(sub);
                    Symbol& sym = symbol_table.symbols[sym_id];
                    sym.type = TYPE_NUMERIC;
                }else{
                    EXPECT(sub, TYPE_NUMERIC)
                }
            }

            EXPECT(parse_tree.rhs(pn), TYPE_NUMERIC)
            break;
        }

        case OP_IF:
        case OP_WHILE:
            EXPECT(parse_tree.lhs(pn), TYPE_BOOLEAN)
            resolveStmt(parse_tree.rhs(pn));
            break;

        case OP_IF_ELSE:
            EXPECT(parse_tree.arg(pn, 0), TYPE_BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            break;

        case OP_FOR:
            resolveStmt(parse_tree.arg(pn, 0));
            EXPECT(parse_tree.arg(pn, 1), TYPE_BOOLEAN)
            resolveStmt(parse_tree.arg(pn, 2));
            resolveStmt(parse_tree.arg(pn, 3));
            break;

        case OP_EXPR_STMT:
            resolveExpr(parse_tree.child(pn));
            break;

        case OP_ALGORITHM:{
            size_t sym_id = parse_tree.getFlag(parse_tree.arg(pn, 0));
            //DO THIS - SymbolTableBuilder::resolveAlgorithm sets the flag and should do something else
            //symbol_table.symbols[sym_id].type = FUNCTION_PLACEHOLDER;

            //DO THIS: type check default args
            //DO THIS: type check captured vars
            //DO THIS: type is a function pool

            break;
        }

        case OP_PROTOTYPE_ALG:{
            //DO THIS
            break;
        }

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveStmt(parse_tree.arg(pn, i));
            break;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveExpr(parse_tree.arg(pn, i));
            break;

        case OP_ASSERT:
            EXPECT(parse_tree.child(pn), TYPE_BOOLEAN)
            break;

        default:
            assert(false);
    }

    #undef EXPECT
}

size_t TypeResolver::instantiateFunc(size_t body, size_t params, bool is_lambda) noexcept{
    /*
    //Resolve default params
    FuncSignature sig;

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL){
            sig.n_default++;
            size_t type = resolveExpr(parse_tree.rhs(param), TYPE_UNKNOWN);
            size_t sym_id = parse_tree.getFlag(parse_tree.lhs(param));
            symbol_table.symbols[sym_id].type = type;
        }
    }

    //Resolve body
    size_t return_type;
    if(is_lambda){
        return_type = resolveExpr(body, TYPE_UNKNOWN);
    }else{
        return_types.push(TYPE_UNKNOWN);
        resolveStmt(body);

        //Handle return type
        size_t return_type = return_types.top();
        return_types.pop();
        if(return_type == TYPE_UNKNOWN) return_type = TYPE_VOID; //DO THIS - perhaps you failed to deduce the value of a return expr
    }
    sig.args_begin = function_type_pool.size();
    function_type_pool.push_back(return_type);

    std::cout << "Return resolved. Resolving params..." << std::endl;

    //Handle the parameters
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode arg = parse_tree.arg(params, i);
        Symbol& sym = symbol_table.symbols[parse_tree.getFlag(arg)];
        if(sym.type == TYPE_UNKNOWN) return error(arg); //Failed to deduce type
        else function_type_pool.push_back(sym.type);
    }

    std::cout << "SUCCESS" << std::endl;

    sig.args_end = function_type_pool.size();

    return getMemoizedType(sig);
    */

    //DO THIS
    return TYPE_UNKNOWN;
}

size_t TypeResolver::resolveExpr(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    #define EXPECT_OR(node, expected, code) \
        if(resolveExpr(node) == TYPE_UNKNOWN) return TYPE_UNKNOWN; /*DO THIS: DELETE THIS LINE*/ else \
        if(resolveExpr(node) != expected) return error(node, code);

    #define EXPECT(node, expected) \
        if(resolveExpr(node) == TYPE_UNKNOWN) return TYPE_UNKNOWN; /*DO THIS: DELETE THIS LINE*/ else \
        if(resolveExpr(node) != expected) return error(node);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA:
            //DO THIS - type is a function pool
            return FUNCTION_PLACEHOLDER;
            //return resolveFunction(parse_tree.arg(pn, 3), parse_tree.arg(pn, 2), true);
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:{
            ParseNode var = parse_tree.getFlag(pn);
            size_t sym_id = parse_tree.getFlag(var);
            if(sym_id == ParseTree::EMPTY) return TYPE_UNKNOWN; //DO THIS: delete after fixing prototypes
            Symbol& sym = symbol_table.symbols[sym_id];

            return sym.type;
        }
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            size_t expected = resolveExpr(parse_tree.arg(pn, 0));
            if(expected != TYPE_NUMERIC && expected != TYPE_STRING) return error(parse_tree.arg(pn, 0), TYPE_NOT_ADDABLE);

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), expected)

            return expected;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT_OR(parse_tree.arg(pn, i), TYPE_NUMERIC, BAD_READ_OR_SUBSCRIPT);
            return TYPE_NUMERIC;
        case OP_SLICE_ALL:
            return TYPE_NUMERIC;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT: //DO THIS - you need to distinguish between ewise assignment and normal var subscripts
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                EXPECT(parse_tree.arg(pn, i), TYPE_NUMERIC)
            return TYPE_NUMERIC;
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:
            return resolveExpr(parse_tree.child(pn));
        case OP_SUBTRACTION:
        case OP_DIVIDE:
        case OP_FRACTION:
        case OP_BACKSLASH:
        case OP_FORWARDSLASH:
        case OP_BINOMIAL:
        case OP_DOT:
        case OP_CROSS:
        case OP_IDENTITY_MATRIX:
        case OP_ONES_MATRIX:
        case OP_ZERO_MATRIX:
        case OP_LOGARITHM_BASE:
        case OP_ARCTANGENT2:
        case OP_NORM_p:
        case OP_POWER:
        case OP_ROOT:
            EXPECT(parse_tree.lhs(pn), TYPE_NUMERIC)
            EXPECT(parse_tree.rhs(pn), TYPE_NUMERIC)
            return TYPE_NUMERIC;
        case OP_LENGTH:
            EXPECT(parse_tree.child(pn), TYPE_NUMERIC)
            return TYPE_NUMERIC;
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
            EXPECT(parse_tree.child(pn), TYPE_NUMERIC)
            return TYPE_NUMERIC;
        case OP_SUMMATION:
        case OP_PRODUCT:{
            ParseNode asgn = parse_tree.arg(pn, 0);
            assert(parse_tree.getOp(asgn) == OP_ASSIGN);
            symbol_table.symbols[parse_tree.getFlag(parse_tree.lhs(asgn))].type = TYPE_NUMERIC;
            EXPECT(parse_tree.rhs(asgn), TYPE_NUMERIC)
            EXPECT(parse_tree.arg(pn, 1), TYPE_NUMERIC)
            EXPECT(parse_tree.arg(pn, 2), TYPE_NUMERIC)
            return TYPE_NUMERIC;
        }
        case OP_LOGICAL_NOT:
            EXPECT(parse_tree.child(pn), TYPE_BOOLEAN)
            return TYPE_BOOLEAN;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:
            EXPECT(parse_tree.lhs(pn), TYPE_BOOLEAN)
            EXPECT(parse_tree.rhs(pn), TYPE_BOOLEAN)
            return TYPE_BOOLEAN;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            EXPECT(parse_tree.lhs(pn), TYPE_NUMERIC)
            EXPECT(parse_tree.rhs(pn), TYPE_NUMERIC)
            return TYPE_BOOLEAN;
        case OP_CASES:{
            //DO THIS - function groups can accumulate here

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2)
                EXPECT(parse_tree.arg(pn, i), TYPE_BOOLEAN)

            size_t expected = resolveExpr(parse_tree.arg(pn, 0));
            for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2)
                EXPECT(parse_tree.arg(pn, i), expected)
            return expected;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:{
            EXPECT(parse_tree.rhs(pn), resolveExpr(parse_tree.lhs(pn)))
            return TYPE_BOOLEAN;
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
            return TYPE_NUMERIC;
        case OP_FALSE:
        case OP_TRUE:
            return TYPE_BOOLEAN;
        case OP_STRING:
            return TYPE_STRING;
        default:
            assert(false);
            return TYPE_BOOLEAN;
    }
}

size_t TypeResolver::callSite(size_t pn) noexcept{
    ParseNode call_expr = parse_tree.arg(pn, 0);
    size_t node_size = parse_tree.getNumArgs(pn);
    size_t callable_type = resolveExpr(call_expr);

    if(callable_type == TYPE_NUMERIC){
        bool is_mult = (node_size == 2 && resolveExpr(parse_tree.rhs(pn)) == TYPE_NUMERIC);
        return is_mult ? TYPE_NUMERIC : error(pn, NOT_CALLABLE);
    }else if(!isAbstractFunctionGroup(callable_type)){
        return error(parse_tree.arg(pn, 0), NOT_CALLABLE);
    }

    return TYPE_UNKNOWN;

    //DO THIS - instantiate all the functions out of the pool
    //          make sure they have the same return type and no conflicts

    /*
    const FuncSignature& fun = function_sig_pool[callable_type];
    size_t n_params = fun.numParams();
    size_t n_args = node_size-1;

    if(n_args > n_params || n_args + fun.n_default < n_params) return error(pn);

    for(size_t i = 0; i < n_args; i++){
        size_t param_type = fun.paramType(i);
        ParseNode arg = parse_tree.arg(pn, 1+i);
        if(resolveExpr(arg, param_type) != param_type) return error(arg);
    }

    return fun.returnType();
    */
}

size_t TypeResolver::implicitMult(size_t pn, size_t start) noexcept{
    ParseNode lhs = parse_tree.arg(pn, start);
    size_t tl = resolveExpr(lhs);
    if(start == parse_tree.getNumArgs(pn)-1) return tl;
    else if(tl == TYPE_NUMERIC){
        if(implicitMult(pn, start+1) != TYPE_NUMERIC) return error(parse_tree.arg(pn, start+1));
        return TYPE_NUMERIC;
    }else if(!isAbstractFunctionGroup(tl)){
        return error(lhs, NOT_CALLABLE);
    }

    return TYPE_UNKNOWN;
    //DO THIS

    const FuncSignature& fun = function_sig_pool[tl];
    if(fun.numParams() != 1) return error(pn);

    return TYPE_UNKNOWN;

    //DO THIS - instantiate the functions from the pool

    //expected = fun.paramType();
    //if(implicitMult(pn, expected, start+1) != expected) return error(parse_tree.arg(pn, start+1));
    //return fun.returnType();
}

size_t TypeResolver::error(size_t pn, ErrorCode code) noexcept{
    errors.push_back(Error(parse_tree.getSelection(pn), code));
    return TYPE_FAILURE;
}

}

}
