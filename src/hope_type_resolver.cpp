#include "hope_type_resolver.h"

#include "hope_error.h"
#include "hope_parse_tree.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

#ifndef NDEBUG
std::string toBinaryField(size_t n, uint8_t digits = 64){
    std::string str;
    for(uint8_t i = digits; i-->0;)
        str += '0' + ((n >> i) & 1);
    return str;
}
#endif

TypeResolver::TypeResolver(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Code::Error>& errors) noexcept
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors) {}

void TypeResolver::resolve(){
    resolveStmt(parse_tree.root);
}

void TypeResolver::resolveStmt(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    switch (parse_tree.getOp(pn)) {
        case OP_ASSIGN:
        case OP_EQUAL:
            parse_tree.setFlag(parse_tree.lhs(pn), traverseNode(parse_tree.rhs(pn)));
            break;
        case OP_REASSIGN:{
            ParseNode var = parse_tree.getFlag(parse_tree.lhs(pn));
            if(parse_tree.getFlag(var) != traverseNode(parse_tree.rhs(pn)))
                error(parse_tree.rhs(pn));
            break;
        }

        case OP_IF:
        case OP_WHILE:
            if(traverseNode(parse_tree.lhs(pn)) != TYPE_BOOLEAN)
                error(parse_tree.lhs(pn));
            resolveStmt(parse_tree.rhs(pn));
            break;

        case OP_IF_ELSE:
            if(traverseNode(parse_tree.arg(pn, 0)) != TYPE_BOOLEAN)
                error(parse_tree.arg(pn, 0));
            resolveStmt(parse_tree.arg(pn, 1));
            resolveStmt(parse_tree.arg(pn, 2));
            break;

        case OP_ALGORITHM:
            break; //Function does not have a signature - this is resolved at call site

        default:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) resolveStmt(parse_tree.arg(pn, i));
    }
}

size_t TypeResolver::traverseNode(size_t pn) noexcept{
    assert(pn != ParseTree::EMPTY);

    switch (parse_tree.getOp(pn)) {
        case OP_LAMBDA:
            return TYPE_CALLABLE; //Function does not have a signature - this is resolved at call site
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:
            return parse_tree.getFlag(parse_tree.getFlag(pn));
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            size_t type = traverseNode(parse_tree.arg(pn, 0));
            if(type != TYPE_NUMERIC && type != TYPE_STRING)
                return error(parse_tree.arg(pn, 0), TYPE_NOT_ADDABLE);
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
                if(traverseNode(parse_tree.arg(pn, i)) != type)
                    return error(parse_tree.arg(pn, i), TYPE_NOT_ADDABLE);
            return type;
        }
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                if(traverseNode(parse_tree.arg(pn, i)) != TYPE_NUMERIC)
                    return error(parse_tree.arg(pn, i), BAD_READ_OR_SUBSCRIPT);
            return TYPE_NUMERIC;
        case OP_SLICE_ALL:
            if(traverseNode(parse_tree.child(pn)) != TYPE_NUMERIC)
                return error(parse_tree.child(pn));
            return TYPE_NUMERIC;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
                if(traverseNode(parse_tree.arg(pn, i)) != TYPE_NUMERIC)
                    return error(parse_tree.arg(pn, i));
            return TYPE_NUMERIC;
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:
            return traverseNode(parse_tree.child(pn));
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
            if(traverseNode(parse_tree.lhs(pn)) != TYPE_NUMERIC) return error(parse_tree.lhs(pn));
            if(traverseNode(parse_tree.rhs(pn)) != TYPE_NUMERIC) return error(parse_tree.rhs(pn));
            return TYPE_NUMERIC;
        case OP_LENGTH:
            if(traverseNode(parse_tree.child(pn)) != TYPE_NUMERIC) return error(parse_tree.child(pn));
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
            if(traverseNode(parse_tree.child(pn)) != TYPE_NUMERIC) return error(parse_tree.child(pn));
            return TYPE_NUMERIC;
        case OP_LOGICAL_NOT:
            if(traverseNode(parse_tree.child(pn)) != TYPE_BOOLEAN) return error(parse_tree.child(pn));
            return TYPE_BOOLEAN;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:
            if(traverseNode(parse_tree.lhs(pn)) != TYPE_BOOLEAN) return error(parse_tree.lhs(pn));
            if(traverseNode(parse_tree.rhs(pn)) != TYPE_BOOLEAN) return error(parse_tree.rhs(pn));
            return TYPE_BOOLEAN;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            if(traverseNode(parse_tree.lhs(pn)) != TYPE_NUMERIC) return error(parse_tree.lhs(pn));
            if(traverseNode(parse_tree.rhs(pn)) != TYPE_NUMERIC) return error(parse_tree.rhs(pn));
            return TYPE_BOOLEAN;
        case OP_EQUAL:
        case OP_NOT_EQUAL:
            if(traverseNode(parse_tree.lhs(pn)) != traverseNode(parse_tree.rhs(pn))) return error(parse_tree.lhs(pn));
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
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) traverseNode(parse_tree.arg(pn, i));
            return TYPE_UNCHECKED;
    }
}

size_t TypeResolver::callSite(size_t pn) noexcept{
    FunctionSignature sig;
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
        sig.type_fields.push_back(traverseNode(parse_tree.arg(pn, i)));

    //DO THIS
    //Your callsite theory depends on the idea that you can always statically resolve a called value...

    auto lookup = concrete_functions.find(sig);
    return (lookup != concrete_functions.end()) ? parse_tree.getFlag(lookup->second) : instantiate(sig);

}

size_t TypeResolver::implicitMult(size_t pn) noexcept{
    //DO THIS
    //Determine if multiplication or function call

    return TYPE_ERROR;
}

size_t TypeResolver::instantiate(const FunctionSignature& sig) noexcept{
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
