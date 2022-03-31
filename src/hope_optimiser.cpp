#include "hope_optimiser.h"

#include "hope_parse_tree.h"

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
        visitNode(parse_tree.arg(pn, i));

    switch (parse_tree.getOp(pn)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            parse_tree.setFlag(pn, stod(parse_tree.str(pn)));
            break;
    }

    return pn;
}

}

}
