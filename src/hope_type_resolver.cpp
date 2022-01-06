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
    performPass();
    //reportErrors(root);
}

void TypeResolver::performPass() noexcept{
    traverseNode(parse_tree.root);
}

void TypeResolver::traverseNode(size_t pn) noexcept{
    if(pn == ParseTree::EMPTY) return;
    //compareWithChildren(pn);
    parse_tree.setType(pn, TYPE_ANY);
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) traverseNode(parse_tree.arg(pn, i));
    compareWithChildren(pn);
}

void TypeResolver::compareWithChildren(size_t pn) noexcept{
    switch (parse_tree.getOp(pn)) {
        case OP_SLICE:
        case OP_SLICE_ALL:
            enforceNumeric(pn); //DO THIS: Wait, what?
            enforceSameAsChildren(pn); break;
            break;
        case OP_IDENTIFIER:
            //DO THIS
            break;
        case OP_CALL:
            //DO THIS
            break;
        case OP_ADDITION:
            enforceType(pn, TYPE_NUMERIC|TYPE_STRING);
            enforceSameAsChildren(pn);
            break;
        case OP_MULTIPLICATION:
        case OP_MATRIX:
        case OP_SUBSCRIPT_ACCESS:
        case OP_ELEMENTWISE_ASSIGNMENT:
        case OP_UNIT_VECTOR:
            enforceNumeric(pn);
            enforceSameAsChildren(pn);
            break;
        case OP_IMPLICIT_MULTIPLY:
            //DO THIS - more complexity here
            enforceNumeric(pn);
            enforceSameAsChildren(pn);
            break;
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:
            enforceSame(pn, parse_tree.child(pn));
            break;
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
            enforceNumeric(pn);
            enforceNumeric(parse_tree.lhs(pn));
            enforceNumeric(parse_tree.rhs(pn));
            break;

        case OP_LENGTH:
            enforceNumeric(pn);
            enforceType(parse_tree.child(pn), TYPE_NUMERIC);
            break;

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
            enforceNumeric(pn);
            enforceNumeric(parse_tree.child(pn));
            break;
        case OP_LOGICAL_AND:
        case OP_LOGICAL_NOT:
        case OP_LOGICAL_OR:
            enforceBinary(pn);
            enforceSameAsChildren(pn);
            break;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            enforceBinary(pn);
            enforceNumeric(parse_tree.lhs(pn));
            enforceNumeric(parse_tree.rhs(pn));
            break;
        case OP_EQUAL:
        case OP_NOT_EQUAL:
            enforceBinary(pn);
            enforceSame(parse_tree.lhs(pn), parse_tree.rhs(pn));
            break;
        case OP_ASSIGN:
        case OP_REASSIGN:
            parse_tree.setType(pn, TYPE_UNCHECKED);
            enforceSame(parse_tree.lhs(pn), parse_tree.rhs(pn));
            break;
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
            enforceNumeric(pn);
            break;
        case OP_FALSE:
        case OP_TRUE:
            enforceBinary(pn);
            break;
        case OP_STRING:
            enforceString(pn);
            break;

        default:
            parse_tree.setType(pn, TYPE_UNCHECKED);
    }
}

void TypeResolver::enforceSameAsChildren(size_t pn) noexcept{
    size_t type = parse_tree.getType(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        type &= parse_tree.getType(parse_tree.arg(pn, i));
    parse_tree.setType(pn, type);
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        parse_tree.setType(parse_tree.arg(pn,i), type);
}

void TypeResolver::enforceType(size_t pn, size_t type) noexcept{
    parse_tree.setType(pn, parse_tree.getType(pn) & type);
}

void TypeResolver::enforceNumeric(size_t pn) noexcept{
    parse_tree.setType(pn, parse_tree.getType(pn) & TYPE_NUMERIC);
}

void TypeResolver::enforceBinary(size_t pn) noexcept{
    parse_tree.setType(pn, parse_tree.getType(pn) & TYPE_BINARY);
}

void TypeResolver::enforceString(size_t pn) noexcept{
    parse_tree.setType(pn, parse_tree.getType(pn) & TYPE_STRING);
}

void TypeResolver::enforceSame(size_t a, size_t b) noexcept{
    size_t type = parse_tree.getType(a) & parse_tree.getType(b);
    parse_tree.setType(a, type);
    parse_tree.setType(b, type);
}

void TypeResolver::reportErrors(size_t pn) noexcept{
    if(pn == ParseTree::EMPTY) return;
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++) reportErrors(parse_tree.arg(pn, i));
    size_t type = parse_tree.getType(pn);

    switch (type) {
        case TYPE_ERROR:
            //Type is overconstrained
            errors.push_back(Error(parse_tree.getSelection(pn), ErrorCode::TYPE_NOT_ADDABLE));
            //std::cout << "Overconstrained node: " << parse_tree.str(pn)  << " = " << toBinaryField(type) << std::endl;
            break;
        case TYPE_NUMERIC:
        case TYPE_STRING:
        case TYPE_BINARY:
        case TYPE_ALG:
        case TYPE_UNCHECKED:
            break;
        default:
            //Type is underconstrained
            errors.push_back(Error(parse_tree.getSelection(pn), ErrorCode::TYPE_NOT_ADDABLE));
            //std::cout << "Underconstrained node: " << parse_tree.str(pn)  << " = " << toBinaryField(type) << std::endl;
    }
}

}

}
