#include "hope_optimiser.h"

#include "hope_parse_tree.h"
#include "hope_symbol_table.h"
#include "hope_type_resolver.h"

#include <math.h>

namespace Hope {

namespace Code {

Optimiser::Optimiser(ParseTree& parse_tree, SymbolTable& symbol_table, std::vector<Error>& errors, std::vector<Error>& warnings)
    : parse_tree(parse_tree), symbol_table(symbol_table), errors(errors), warnings(warnings) {}

void Optimiser::optimise(){
    parse_tree.root = visitStmt(parse_tree.root);

    //EVENTUALLY: replace this with something less janky
    for(const Usage& usage : symbol_table.usages){
        const Symbol& sym = symbol_table.symbols[usage.var_id];

        if(sym.type == TypeResolver::NUMERIC && (sym.rows > 1 || sym.cols > 1)){
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

ParseNode Optimiser::visitStmt(ParseNode pn){
    assert(pn != ParseTree::EMPTY);

    switch (parse_tree.getOp(pn)) {
        case OP_ASSIGN:
        case OP_EQUAL:{
            ParseNode rhs = visitExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            size_t sym_id = parse_tree.getFlag(parse_tree.lhs(pn));
            Symbol& sym = symbol_table.symbols[sym_id];
            sym.rows = parse_tree.getRows(rhs);
            sym.cols = parse_tree.getCols(rhs);
            return pn;
        }

        case OP_REASSIGN: {
            ParseNode rhs = visitExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            return pn;
        }

        case OP_ALGORITHM:{
            ParseNode params = parse_tree.paramList(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getOp(param) == OP_EQUAL)
                    parse_tree.setArg<1>(param, visitExpr(parse_tree.rhs(param)));
            }

            parse_tree.setBody(pn, visitStmt(parse_tree.body(pn)));
            return pn;
        }

        case OP_IF:
        case OP_WHILE:{
            ParseNode lhs = visitExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);

            parse_tree.setArg<1>(pn, visitStmt(parse_tree.arg<1>(pn)));
            return pn;
        }

        case OP_IF_ELSE:{
            ParseNode cond = visitExpr(parse_tree.arg<0>(pn));
            parse_tree.setArg<0>(pn, cond);

            parse_tree.setArg<1>(pn, visitStmt(parse_tree.arg<1>(pn)));
            parse_tree.setArg<2>(pn, visitStmt(parse_tree.arg<2>(pn)));
            return pn;
        }

        case OP_FOR:{
            parse_tree.setArg<0>(pn, visitStmt(parse_tree.arg<0>(pn)));
            ParseNode cond = visitExpr(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, cond);
            parse_tree.setArg<2>(pn, visitStmt(parse_tree.arg<2>(pn)));
            parse_tree.setArg<3>(pn, visitStmt(parse_tree.arg<3>(pn)));
            return pn;
        }

        case OP_EXPR_STMT:{
            ParseNode expr = visitExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, expr);
            if(parse_tree.getOp(expr) != OP_CALL || !TypeResolver::isAbstractFunctionGroup(parse_tree.getType(parse_tree.arg<0>(expr)))){
                warnings.push_back(Error(parse_tree.getSelection(expr), ErrorCode::UNUSED_EXPRESSION));
                parse_tree.setOp(pn, OP_DO_NOTHING);
                //EVENTUALLY: check the call stmt has side effects
            }
            return pn;
        }

        case OP_BLOCK:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode stmt = visitStmt(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, stmt);
            }
            return pn;

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode expr = visitExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, expr);
                //EVENTUALLY: Specialise printing for type
            }
            return pn;

        case OP_ASSERT:{
            ParseNode child = visitExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            return pn;
        }

        default:
            return pn;
    }
}

ParseNode Optimiser::visitExpr(ParseNode pn){
    if(pn == ParseTree::EMPTY) return ParseTree::EMPTY;

    parse_tree.setValue(pn, NIL);

    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:{
            ParseNode var = parse_tree.getFlag(pn);
            size_t sym_id = parse_tree.getFlag(var);
            Symbol& sym = symbol_table.symbols[sym_id];
            parse_tree.setRows(pn, sym.rows);
            parse_tree.setCols(pn, sym.cols);

            return pn;
        }

        case OP_MATRIX: return visitMatrix(pn);
        case OP_MULTIPLICATION: return visitMult(pn);
        case OP_POWER: return visitPower(pn);
        case OP_TRANSPOSE: return visitTranspose(pn);
        case OP_UNARY_MINUS: return visitUnaryMinus(pn);

        default:
            //for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
            //    parse_tree.setArg(pn, i, visitExpr(parse_tree.arg(pn, i)));
            return pn;
    }
}

ParseNode Optimiser::visitLambda(ParseNode pn){
    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            parse_tree.setArg<1>(param, visitExpr(parse_tree.rhs(param)));
    }

    parse_tree.setBody(pn, visitExpr(parse_tree.body(pn)));
    return pn;
}

ParseNode Optimiser::visitLength(ParseNode pn){
    parse_tree.setScalar(pn);

    ParseNode arg = parse_tree.child(pn);
    size_t rows = parse_tree.getRows(arg);
    size_t cols = parse_tree.getCols(arg);

    if(rows == UNKNOWN_SIZE || cols == UNKNOWN_SIZE) return pn;
    else if(rows > 1 && cols > 1){
        errors.push_back(Error(parse_tree.getSelection(arg), DIMENSION_MISMATCH));
        return pn;
    }
    double val = rows*cols;
    parse_tree.setFlag(pn, val);
    parse_tree.setValue(pn, val);
    parse_tree.setNumArgs(pn, 0);
    parse_tree.setOp(pn, OP_INTEGER_LITERAL);

    return pn;
}

static bool dimsDisagree(size_t a, size_t b) noexcept{
    return a != UNKNOWN_SIZE && b != UNKNOWN_SIZE && a != b;
}

ParseNode Optimiser::visitMatrix(ParseNode pn){
    size_t nargs = parse_tree.getNumArgs(pn);
    if(nargs == 1){
        ParseNode child = visitExpr(parse_tree.child(pn));
        parse_tree.setRows(pn, parse_tree.getRows(child));
        parse_tree.setCols(pn, parse_tree.getCols(child));
        parse_tree.setValue(pn, parse_tree.getValue(child));

        return child;
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        parse_tree.setArg(pn, i, visitExpr(parse_tree.arg(pn, i)));

    size_t typeset_rows = parse_tree.getFlag(pn);
    size_t typeset_cols = nargs/typeset_rows;
    assert(typeset_cols*typeset_rows == nargs);

    static std::vector<size_t> elem_cols; elem_cols.resize(typeset_cols);
    static std::vector<size_t> elem_rows; elem_rows.resize(typeset_rows);
    static std::vector<ParseNode> elements; elements.resize(nargs);

    size_t curr = 0;
    for(size_t i = 0; i < typeset_rows; i++){
        for(size_t j = 0; j < typeset_cols; j++){
            ParseNode arg = parse_tree.arg(pn, curr);

            if(i==0) elem_cols[j] = parse_tree.getCols(arg);
            else if(elem_cols[j] == UNKNOWN_SIZE) elem_cols[j] = parse_tree.getCols(arg);
            else if(dimsDisagree(elem_cols[j], parse_tree.getCols(arg)))
                return error(pn, arg, DIMENSION_MISMATCH);

            if(j==0) elem_rows[i] = parse_tree.getRows(arg);
            else if(elem_rows[i] == UNKNOWN_SIZE) elem_rows[i] = parse_tree.getRows(arg);
            else if(dimsDisagree(elem_rows[i], parse_tree.getRows(arg)))
                return error(pn, arg, DIMENSION_MISMATCH);
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

ParseNode Optimiser::visitMult(ParseNode pn){
    ParseNode lhs = visitExpr(parse_tree.lhs(pn));
    parse_tree.setArg<0>(pn, lhs);
    ParseNode rhs = visitExpr(parse_tree.rhs(pn));
    parse_tree.setArg<1>(pn, rhs);
    if(parse_tree.getOp(lhs) == OP_INVERT){
        parse_tree.setArg(pn, 0, parse_tree.child(lhs));
        parse_tree.setOp(pn, OP_LINEAR_SOLVE);
        parse_tree.copyDims(pn, rhs);
    }

    return pn;
}

ParseNode Optimiser::visitPower(ParseNode pn){
    ParseNode lhs = visitExpr(parse_tree.lhs(pn));
    parse_tree.setArg<0>(pn, lhs);
    ParseNode rhs = visitExpr(parse_tree.rhs(pn));
    parse_tree.setArg<1>(pn, rhs);

    if(parse_tree.isNonsquare(lhs)) return error(pn, lhs, DIMENSION_MISMATCH);
    else if(parse_tree.isNonsquare(rhs)) return error(pn, rhs, DIMENSION_MISMATCH);

    if(parse_tree.isScalar(rhs)) parse_tree.copyDims(pn, lhs);
    else if(parse_tree.isScalar(lhs)) parse_tree.copyDims(pn, rhs);

    switch (parse_tree.getOp(rhs)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:{
            double rhs_val = parse_tree.getFlagAsDouble(rhs);

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
                return visitExpr(pn);
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

ParseNode Optimiser::visitTranspose(ParseNode pn){
    ParseNode child = visitExpr(parse_tree.child(pn));
    parse_tree.setArg<0>(pn, child);
    parse_tree.transposeDims(pn, child);
    if(parse_tree.isScalar(child)) return child;
    return pn;
}

ParseNode Optimiser::visitUnaryMinus(ParseNode pn){
    ParseNode child = visitExpr(parse_tree.child(pn));
    parse_tree.setArg<0>(pn, child);
    parse_tree.copyDims(pn, child);

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

ParseNode Optimiser::error(ParseNode pn, ParseNode err_node, ErrorCode code){
    errors.push_back(Error(parse_tree.getSelection(err_node), code));
    return pn;
}

}

}
