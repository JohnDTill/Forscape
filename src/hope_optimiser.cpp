#include "hope_optimiser.h"

#include "hope_parse_tree.h"

#include <math.h>

namespace Hope {

namespace Code {

Optimiser::Optimiser(ParseTree& parse_tree)
    : parse_tree(parse_tree){}

void Optimiser::optimise(){
    visitNode(parse_tree.root);
}

ParseNode Optimiser::visitNode(ParseNode pn){
    if(pn == ParseTree::EMPTY) return ParseTree::EMPTY;

    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        parse_tree.setArg(pn, i, visitNode(parse_tree.arg(pn, i)));

    switch (parse_tree.getOp(pn)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            parse_tree.setFlag(pn, stod(parse_tree.str(pn)));
            break;

        case OP_UNARY_MINUS:{
            ParseNode child = parse_tree.child(pn);

            switch (parse_tree.getOp(child)) {
                case OP_INTEGER_LITERAL:
                case OP_DECIMAL_LITERAL:
                    parse_tree.setFlag(child, -parse_tree.getFlagAsDouble(child));
                    return child;
                case OP_UNARY_MINUS:
                    return parse_tree.child(child);
            }

            break;
        }

        case OP_POWER:{
            ParseNode rhs = parse_tree.rhs(pn);

            switch (parse_tree.getOp(rhs)) {
                case OP_INTEGER_LITERAL:
                case OP_DECIMAL_LITERAL:{
                    double rhs_val = parse_tree.getFlagAsDouble(rhs);

                    ParseNode lhs = parse_tree.lhs(pn);
                    if(parse_tree.getOp(lhs) == OP_INTEGER_LITERAL
                       || parse_tree.getOp(lhs) == OP_DECIMAL_LITERAL){
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
                    }
                    //Can only simplify x^0 to 1 assuming no NaN in LHS
                }
            }

            break;
        }

        case OP_MULTIPLICATION:{
            ParseNode lhs = parse_tree.lhs(pn);
            if(parse_tree.getOp(lhs) == OP_INVERT){
                parse_tree.setArg(pn, 0, parse_tree.child(lhs));
                parse_tree.setOp(pn, OP_LINEAR_SOLVE);
                break;
            }
        }
    }

    return pn;
}

}

}
