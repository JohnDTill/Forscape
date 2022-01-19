#include "hope_type_resolver.h"

#include "hope_error.h"
#include "hope_parse_tree.h"

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
    function_signatures.clear();
}

void TypeResolver::resolveStmt(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    switch (parse_tree.getOp(pn)) {
        case OP_ASSIGN:
        case OP_EQUAL:
            parse_tree.setFlag(parse_tree.lhs(pn), resolveExpr(parse_tree.rhs(pn)));
            break;
        case OP_REASSIGN:{
            ParseNode var = parse_tree.getFlag(parse_tree.lhs(pn));
            if(parse_tree.getFlag(var) != resolveExpr(parse_tree.rhs(pn)))
                error(parse_tree.rhs(pn));
            break;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:
            if(resolveExpr(parse_tree.lhs(pn)) != TYPE_NUMERIC) error(parse_tree.lhs(pn));
            else if(resolveExpr(parse_tree.rhs(pn)) != TYPE_NUMERIC) error(parse_tree.rhs(pn));
            break;

        case OP_IF:
        case OP_WHILE:
            if(resolveExpr(parse_tree.lhs(pn)) != TYPE_BOOLEAN)
                error(parse_tree.lhs(pn));
            resolveStmt(parse_tree.rhs(pn));
            break;

        case OP_IF_ELSE:
            if(resolveExpr(parse_tree.arg(pn, 0)) != TYPE_BOOLEAN)
                error(parse_tree.arg(pn, 0));
            resolveStmt(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            break;

        case OP_FOR:
            resolveStmt(parse_tree.arg(pn, 0));
            if(resolveExpr(parse_tree.arg(pn, 1)) != TYPE_BOOLEAN) error(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            resolveStmt(parse_tree.arg(pn, 3));
            break;

        case OP_EXPR_STMT:
            resolveExpr(parse_tree.child(pn));
            break;

        case OP_ALGORITHM:
            //DO THIS
            resolveParams(pn, parse_tree.arg(pn, 3));
            break;

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveStmt(parse_tree.arg(pn, i));
            break;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveExpr(parse_tree.arg(pn, i));
            break;

        default:
            assert(false);
    }
}

void TypeResolver::resolveParams(size_t pn, size_t params) noexcept{
    parse_tree.setFlag(pn, function_signatures.size());
    function_signatures.push_back(FuncSignature());
    FuncSignature& fs = function_signatures.back();
    fs.nargs = parse_tree.getNumArgs(params);
    for(size_t i = 0; i < fs.nargs; i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            fs.default_arg_types.push_back(resolveExpr(parse_tree.rhs(param)));
    }
}

size_t TypeResolver::resolveExpr(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA:
            //DO THIS
            resolveParams(pn, parse_tree.arg(pn, 2));
            return TYPE_CALLABLE;
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:
            return parse_tree.getFlag(parse_tree.getFlag(pn));
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            size_t type = resolveExpr(parse_tree.arg(pn, 0));
            if(type != TYPE_NUMERIC && type != TYPE_STRING)
                return error(parse_tree.arg(pn, 0), TYPE_NOT_ADDABLE);
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
                if(resolveExpr(parse_tree.arg(pn, i)) != type)
                    return error(parse_tree.arg(pn, i), TYPE_NOT_ADDABLE);
            return type;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                if(resolveExpr(parse_tree.arg(pn, i)) != TYPE_NUMERIC)
                    return error(parse_tree.arg(pn, i), BAD_READ_OR_SUBSCRIPT);
            return TYPE_NUMERIC;
        case OP_SLICE_ALL:
            return TYPE_NUMERIC;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                if(resolveExpr(parse_tree.arg(pn, i)) != TYPE_NUMERIC)
                    return error(parse_tree.arg(pn, i));
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
            if(resolveExpr(parse_tree.lhs(pn)) != TYPE_NUMERIC) return error(parse_tree.lhs(pn));
            if(resolveExpr(parse_tree.rhs(pn)) != TYPE_NUMERIC) return error(parse_tree.rhs(pn));
            return TYPE_NUMERIC;
        case OP_LENGTH:
            if(resolveExpr(parse_tree.child(pn)) != TYPE_NUMERIC) return error(parse_tree.child(pn));
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
            if(resolveExpr(parse_tree.child(pn)) != TYPE_NUMERIC) return error(parse_tree.child(pn));
            return TYPE_NUMERIC;
        case OP_LOGICAL_NOT:
            if(resolveExpr(parse_tree.child(pn)) != TYPE_BOOLEAN) return error(parse_tree.child(pn));
            return TYPE_BOOLEAN;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:
            if(resolveExpr(parse_tree.lhs(pn)) != TYPE_BOOLEAN) return error(parse_tree.lhs(pn));
            if(resolveExpr(parse_tree.rhs(pn)) != TYPE_BOOLEAN) return error(parse_tree.rhs(pn));
            return TYPE_BOOLEAN;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            if(resolveExpr(parse_tree.lhs(pn)) != TYPE_NUMERIC) return error(parse_tree.lhs(pn));
            if(resolveExpr(parse_tree.rhs(pn)) != TYPE_NUMERIC) return error(parse_tree.rhs(pn));
            return TYPE_BOOLEAN;
        case OP_CASES:{
            if(resolveExpr(parse_tree.arg(pn, 0)) != TYPE_BOOLEAN) return error(parse_tree.arg(pn, 0));
            size_t type = resolveExpr(parse_tree.arg(pn, 1));
            for(size_t i = 2; i < parse_tree.getNumArgs(pn); i+=2)
                if(resolveExpr(parse_tree.arg(pn, i)) != TYPE_BOOLEAN) return error(parse_tree.arg(pn, i));
                else if(resolveExpr(parse_tree.arg(pn, i+1)) != type) return error(parse_tree.arg(pn, i+1));
            return type;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:
            if(resolveExpr(parse_tree.lhs(pn)) != resolveExpr(parse_tree.rhs(pn))) return error(parse_tree.lhs(pn));
            return TYPE_BOOLEAN;
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
    size_t callable_type = resolveExpr(call_expr);
    size_t node_size = parse_tree.getNumArgs(pn);
    if(callable_type == TYPE_NUMERIC){
        bool is_mult = (node_size == 2 && resolveExpr(parse_tree.rhs(pn)) == TYPE_NUMERIC);
        return is_mult ? TYPE_NUMERIC : error(pn, NOT_CALLABLE);
    }else if(callable_type != TYPE_CALLABLE){
        return error(parse_tree.arg(pn, 0), NOT_CALLABLE);
    }

    CallSignature sig;
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
        sig.arg_types.push_back(resolveExpr(parse_tree.arg(pn, i)));

    //DO THIS - confirm that the func signature and call signature are compatible
    //DO THIS - fill in any default arg types

    //DO THIS
    //Your callsite theory depends on the idea that you can always statically resolve a called value,
    //but the function is the result of an expression.

    auto lookup = instantiated_functions.find(sig);
    return (lookup != instantiated_functions.end()) ? parse_tree.getFlag(lookup->second) : instantiate(sig);

}

size_t TypeResolver::implicitMult(size_t pn, size_t start) noexcept{
    ParseNode lhs = parse_tree.arg(pn, start);
    size_t tl = resolveExpr(lhs);
    if(start == parse_tree.getNumArgs(pn)-1) return tl;
    size_t tr = implicitMult(pn, start+1);
    if(tl == TYPE_NUMERIC) return (tr == TYPE_NUMERIC) ? TYPE_NUMERIC : error(parse_tree.arg(pn, start+1));
    else if(tl != TYPE_CALLABLE) return error(lhs, NOT_CALLABLE);

    //DO THIS - resolve function call

    return TYPE_ERROR;
}

size_t TypeResolver::instantiate(const CallSignature& sig) noexcept{
    //DO THIS
    //Beware of Achille's heel of type-differing recursion (?)

    return TYPE_ERROR;
}

size_t TypeResolver::error(size_t pn, ErrorCode code) noexcept{
    errors.push_back(Error(parse_tree.getSelection(pn), code));
    return TYPE_ERROR;
}

}

}
