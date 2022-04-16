#ifndef HOPE_OPTIMISER_H
#define HOPE_OPTIMISER_H

#include <code_error_types.h>
#include <stddef.h>
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

struct Error;
class ParseTree;
class SymbolTable;
typedef size_t ParseNode;

class Optimiser{
private:
    ParseTree& parse_tree;
    SymbolTable& symbol_table;
    std::vector<Error>& errors;
    std::vector<Error>& warnings;

public:
    Optimiser(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Error>& errors, std::vector<Error>& warnings);
    void optimise();

private:
    ParseNode visitStmt(ParseNode pn);
    ParseNode visitExpr(ParseNode pn);
    ParseNode visitLambda(ParseNode pn);
    ParseNode visitLength(ParseNode pn);
    ParseNode visitMatrix(ParseNode pn);
    ParseNode visitMult(ParseNode pn);
    ParseNode visitPower(ParseNode pn);
    ParseNode visitTranspose(ParseNode pn);
    ParseNode visitUnaryMinus(ParseNode pn);
    ParseNode error(ParseNode pn, ParseNode err_node, ErrorCode code);
};

}

}

#endif // HOPE_OPTIMISER_H
