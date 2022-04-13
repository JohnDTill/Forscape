#include "hope_optimiser.h"

#include "hope_parse_tree.h"

#include <math.h>

namespace Hope {

namespace Code {

static constexpr size_t UNKNOWN_SIZE = 0;

Optimiser::Optimiser(ParseTree& parse_tree)
    : parse_tree(parse_tree){}

void Optimiser::optimise(){
    visitNode(parse_tree.root);
}

ParseNode Optimiser::visitNode(ParseNode pn){
    if(pn == ParseTree::EMPTY) return ParseTree::EMPTY;

    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        parse_tree.setArg(pn, i, visitNode(parse_tree.arg(pn, i)));

    parse_tree.setRows(pn, UNKNOWN_SIZE);
    parse_tree.setCols(pn, UNKNOWN_SIZE);
    parse_tree.setValue(pn, NIL);

    switch (parse_tree.getOp(pn)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            return visitDecimal(pn);

        case OP_UNARY_MINUS: return visitUnaryMinus(pn);
        case OP_POWER: return visitPower(pn);
        case OP_MULTIPLICATION: return visitMult(pn);
        //case OP_CALL: return visitCall(pn);
    }

    return pn;
}

ParseNode Optimiser::visitDecimal(ParseNode pn){
    double val = stod(parse_tree.str(pn));
    parse_tree.setFlag(pn, val);
    parse_tree.setRows(pn, 1);
    parse_tree.setCols(pn, 1);
    parse_tree.setValue(pn, val);
    return pn;
}

ParseNode Optimiser::visitLength(ParseNode pn){
    parse_tree.setRows(pn, 1);
    parse_tree.setCols(pn, 1);

    ParseNode arg = parse_tree.child(pn);
    size_t rows = parse_tree.getRows(arg);
    size_t cols = parse_tree.getCols(arg);

    if(rows == UNKNOWN_SIZE || cols == UNKNOWN_SIZE) return pn;
    else if(rows > 1 && cols > 1){ /* DO THIS: static error */ }
    double val = rows*cols;
    parse_tree.setFlag(pn, val);
    parse_tree.setValue(pn, val);
    parse_tree.setNumArgs(pn, 0);
    parse_tree.setOp(pn, OP_INTEGER_LITERAL);

    return pn;

    //DO THIS: the tree is a mess! how does partial evaluation work?
    //         we need to simplify as much as possible, but retain information for the editor
    //         and maybe derivatives
}

ParseNode Optimiser::visitMult(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    if(parse_tree.getOp(lhs) == OP_INVERT){
        parse_tree.setArg(pn, 0, parse_tree.child(lhs));
        parse_tree.setOp(pn, OP_LINEAR_SOLVE);
    }

    return pn;
}

ParseNode Optimiser::visitPower(ParseNode pn){
    ParseNode rhs = parse_tree.rhs(pn);

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
                return pn;
            }else if(rhs_val == -1){
                parse_tree.setOp(pn, OP_INVERT);
                parse_tree.setNumArgs(pn, 1);
                return visitNode(pn);
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

ParseNode Optimiser::visitUnaryMinus(ParseNode pn){
    ParseNode child = parse_tree.child(pn);

    switch (parse_tree.getOp(child)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            parse_tree.setFlag(child, -parse_tree.getFlagAsDouble(child));
            return child;
        case OP_UNARY_MINUS:
            return parse_tree.child(child);
    }

    return pn;
}

ParseNode Optimiser::visitCall(ParseNode pn){
    //DO THIS - static dimensions and values need to handle different instantiations of abstract functions

    return pn;
}

}

}
