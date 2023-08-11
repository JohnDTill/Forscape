#include "forscape_symbol_link_pass.h"

#include "forscape_parse_tree.h"
#include "forscape_static_pass.h"
#include "forscape_symbol_table.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

Forscape::Code::SymbolTableLinker::SymbolTableLinker(Forscape::Code::ParseTree& parse_tree) noexcept
    : parse_tree(parse_tree) {}

void SymbolTableLinker::link() noexcept {
    resolveBlock(parse_tree.root);
}

void SymbolTableLinker::resolveStmt(ParseNode pn) noexcept {
    switch (parse_tree.getOp(pn)) {
        case OP_ALGORITHM:
            resolveDeclaration(parse_tree.algName(pn));
            //Non-capturing algorithms were moved to the top of their lexical scope,
            //despite possibly being defined later. Resolve them last so their dependencies
            //will definitely be marked with a stack slot.
            if(parse_tree.valCapList(pn) != NONE || parse_tree.getNumArgs(parse_tree.refCapList(pn)) != 0)
                resolveAlgorithm(pn);
            break;
        case OP_ASSERT: resolveExpr(parse_tree.child(pn)); break;
        case OP_ASSIGN: resolveAssignment(pn); break;
        case OP_BLOCK: resolveBlock(pn); break;
        case OP_DEFINE_PROTO: resolveAlgorithm(pn); break;
        case OP_DO_NOTHING: break;
        case OP_ELEMENTWISE_ASSIGNMENT: resolveEWiseAssignment(pn); break;
        case OP_EQUAL: resolveAssignment(pn); break;
        case OP_EXPR_STMT: resolveExpr(pn); break;
        case OP_FOR: resolveFor(pn); break;
        case OP_FROM_IMPORT: resolveImport(pn); break;
        case OP_IF: resolveIf(pn); break;
        case OP_IF_ELSE: resolveIfElse(pn); break;
        case OP_IMPORT: resolveImport(pn); break;
        case OP_NAMESPACE: resolveNamespace(pn); break;
        case OP_PLOT: resolveAllChildrenAsExpressions(pn); break;
        case OP_PRINT: resolveAllChildrenAsExpressions(pn); break;
        case OP_PROTOTYPE_ALG: resolveDeclaration(parse_tree.child(pn)); break;
        case OP_RANGED_FOR: resolveRangedFor(pn); break;
        case OP_REASSIGN: resolveReassignment(pn); break;
        case OP_RETURN: resolveExpr(parse_tree.child(pn)); break;
        case OP_SWITCH:
        case OP_SWITCH_NUMERIC:
        case OP_SWITCH_STRING:
            resolveSwitch(pn); break;
        default: assert(false);
    }
}

void SymbolTableLinker::resolveExpr(ParseNode pn) noexcept {
    switch(parse_tree.getOp(pn)){
        case OP_DEFINITE_INTEGRAL: resolveDefiniteIntegral(pn); break;
        case OP_DERIVATIVE: resolveDerivative(pn); break;
        case OP_IDENTIFIER: resolveReference(pn); break;
        case OP_LAMBDA: resolveAlgorithm(pn); break;
        case OP_PARTIAL: resolveDerivative(pn); break;
        case OP_PRODUCT: resolveBig(pn); break;
        case OP_SINGLE_CHAR_MULT_PROXY: resolveAllChildrenAsExpressions(parse_tree.getFlag(pn)); break;
        case OP_SUMMATION: resolveBig(pn); break;
        default: resolveAllChildrenAsExpressions(pn);
    }
}

void SymbolTableLinker::resolveAlgorithm(ParseNode pn) noexcept {
    increaseClosureDepth(pn);
    ParseNode param_list = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(param_list); i++){
        ParseNode param = parse_tree.arg(param_list, i);
        if(parse_tree.getOp(param) == OP_EQUAL){
            resolveExpr(parse_tree.rhs(param));
            resolveDeclaration(parse_tree.lhs(param));
        }else{
            resolveDeclaration(param);
        }
    }
    if(parse_tree.getOp(pn) != OP_LAMBDA) resolveStmt(parse_tree.body(pn));
    else resolveExpr(parse_tree.body(pn));

    decreaseClosureDepth(pn);
}

void SymbolTableLinker::resolveAssignment(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_ASSIGN || parse_tree.getOp(pn) == OP_EQUAL);
    resolveExpr(parse_tree.rhs(pn));
    resolveDeclaration(parse_tree.lhs(pn));
}

void SymbolTableLinker::resolveBlock(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_BLOCK);
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveStmt(parse_tree.arg(pn, i));

    //Non-capturing algorithms were moved to the top of their lexical scope,
    //despite possibly being defined later. Resolve them last so their dependencies
    //will definitely be marked with a stack slot.
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
        ParseNode child = parse_tree.arg(pn, i);
        if(parse_tree.getOp(child) != OP_ALGORITHM
            || parse_tree.valCapList(child) != NONE
            || parse_tree.getNumArgs(parse_tree.refCapList(child)) != 0) continue;
        resolveAlgorithm(child);
    }
}

void SymbolTableLinker::resolveEWiseAssignment(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_ELEMENTWISE_ASSIGNMENT);
    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);
    ParseNode id = parse_tree.arg<0>(lhs);
    resolveReference(id);

    increaseLexicalDepth();
    const size_t num_subscripts = parse_tree.getNumArgs(lhs)-1;
    ParseNode first = parse_tree.arg<1>(lhs);
    if(parse_tree.getOp(first) != OP_SLICE) resolveDeclaration(first);
    else resolveExpr(first);

    if(num_subscripts > 1){
        ParseNode second = parse_tree.arg<2>(lhs);
        if(parse_tree.getOp(second) != OP_SLICE) resolveDeclaration(second);
        else resolveExpr(second);
    }

    resolveExpr(rhs);
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveFor(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_FOR);
    ParseNode initialiser = parse_tree.arg<0>(pn);
    ParseNode condition = parse_tree.arg<1>(pn);
    ParseNode update = parse_tree.arg<2>(pn);

    increaseLexicalDepth();
    if(initialiser != NONE) resolveStmt(initialiser);
    if(condition != NONE) resolveExpr(condition);
    if(update != NONE) resolveStmt(update);
    resolveStmt(parse_tree.arg<3>(pn));
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveIf(ParseNode pn) noexcept {
    resolveExpr(parse_tree.arg<0>(pn));
    increaseLexicalDepth();
    resolveStmt(parse_tree.arg<1>(pn));
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveIfElse(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_IF_ELSE);

    resolveIf(pn);
    increaseLexicalDepth();
    resolveStmt(parse_tree.arg<2>(pn));
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveImport(ParseNode pn) noexcept {
    ParseNode root = parse_tree.getFlag(pn);
    if(root != NONE) resolveStmt(root);
}

void SymbolTableLinker::resolveNamespace(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_NAMESPACE);
    ParseNode body = parse_tree.arg<1>(pn);
    resolveBlock(body);
}

void SymbolTableLinker::resolveRangedFor(ParseNode pn) noexcept {
    increaseLexicalDepth();
    resolveDeclaration(parse_tree.arg<0>(pn));
    resolveExpr(parse_tree.arg<1>(pn));
    resolveStmt(parse_tree.arg<2>(pn));
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveReassignment(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_REASSIGN);
    resolveExpr(parse_tree.rhs(pn));
    resolveExpr(parse_tree.lhs(pn));
}

void SymbolTableLinker::resolveSwitch(ParseNode pn) noexcept {
    resolveExpr(parse_tree.arg<0>(pn));
    for(size_t i = parse_tree.getNumArgs(pn); i-->1;){
        ParseNode case_node = parse_tree.arg(pn, i);
        if(parse_tree.getOp(case_node) == OP_CASE) resolveExpr(parse_tree.lhs(case_node));
        ParseNode stmt = parse_tree.rhs(case_node);
        if(stmt != NONE){
            increaseLexicalDepth();
            resolveStmt(stmt);
            decreaseLexicalDepth();
        }
    }

    //EVENTUALLY: should remove a step here
    //ParseNode default_node = parse_tree.getFlag(pn);
    //if(default_node != NONE)
    //    parse_tree.setFlag(pn, parse_tree.rhs(default_node));
}

void SymbolTableLinker::resolveBig(ParseNode pn) noexcept {
    increaseLexicalDepth();
    ParseNode assign = parse_tree.arg<0>(pn);
    if( parse_tree.getOp(assign) != OP_ASSIGN ) return;
    ParseNode id = parse_tree.lhs(assign);
    ParseNode stop = parse_tree.arg<1>(pn);
    ParseNode body = parse_tree.arg<2>(pn);

    resolveDeclaration(id);
    resolveExpr( parse_tree.rhs(assign) );
    resolveExpr( stop );
    resolveExpr( body );

    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveDefiniteIntegral(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_DEFINITE_INTEGRAL);
    resolveExpr(parse_tree.arg<1>(pn));
    resolveExpr(parse_tree.arg<2>(pn));
    increaseLexicalDepth();
    resolveDeclaration(parse_tree.arg<0>(pn));
    resolveExpr(parse_tree.arg<3>(pn));
    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveDerivative(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_DERIVATIVE || parse_tree.getOp(pn) == OP_PARTIAL);

    ParseNode id = parse_tree.arg<1>(pn);
    ParseNode previous = parse_tree.arg<2>(pn);
    assert(previous != NONE);
    resolveReference(previous);

    increaseLexicalDepth();
    resolveDeclaration(id);
    resolveExpr(parse_tree.arg<0>(pn));

    decreaseLexicalDepth();
}

void SymbolTableLinker::resolveDeclaration(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);

    Symbol& sym = *parse_tree.getSymbol(pn);

    if(sym.is_closure_nested && !sym.is_captured_by_value){
        parse_tree.setOp(pn, OP_READ_UPVALUE);
        parse_tree.setClosureIndex(pn, sym.flag);
    }else if(!sym.is_captured_by_value){
        sym.flag = stack_size++;
    }
}

void SymbolTableLinker::resolveReference(ParseNode pn) noexcept {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);

    Symbol* sym = parse_tree.getSymbol(pn);
    while(sym->type == StaticPass::ALIAS) sym = sym->shadowedVar();

    if(sym->is_closure_nested){
        parse_tree.setOp(pn, OP_READ_UPVALUE);
        parse_tree.setClosureIndex(pn, sym->flag);
    }else if(sym->declaration_closure_depth == 0){
        parse_tree.setOp(pn, OP_READ_GLOBAL);
        parse_tree.setGlobalIndex(pn, sym->flag);
    }else{
        parse_tree.setStackOffset(pn, stack_size - 1 - sym->flag);
    }
}

void SymbolTableLinker::resolveAllChildrenAsExpressions(ParseNode pn) noexcept {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveExpr(parse_tree.arg(pn, i));
}

void SymbolTableLinker::increaseLexicalDepth() noexcept {
    stack_frame.push_back(stack_size);
}

void SymbolTableLinker::decreaseLexicalDepth() noexcept {
    stack_size = stack_frame.back();
    stack_frame.pop_back();
}

void SymbolTableLinker::increaseClosureDepth(ParseNode pn) noexcept {
    increaseLexicalDepth();

    ParseNode val_list = parse_tree.valCapList(pn);
    ParseNode ref_list = parse_tree.refCapList(pn);
    size_t N_cap = parse_tree.valListSize(val_list);

    for(size_t i = 0; i < N_cap; i++){
        ParseNode cap = parse_tree.arg(val_list, i);
        Symbol& sym = *parse_tree.getSymbol(cap);
        old_flags.push_back(sym.flag);
        sym.flag = i;
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        Symbol& sym = *parse_tree.getSymbol(ref);
        old_flags.push_back(sym.flag);
        sym.flag = N_cap + i;
    }

    closure_depth++;
}

void SymbolTableLinker::decreaseClosureDepth(ParseNode pn) noexcept {
    decreaseLexicalDepth();

    ParseNode val_list = parse_tree.valCapList(pn);
    ParseNode ref_list = parse_tree.refCapList(pn);
    size_t N_val = parse_tree.valListSize(val_list);

    for(size_t i = parse_tree.getNumArgs(ref_list); i-->0;){
        ParseNode n = parse_tree.arg(ref_list, i);
        Symbol& sym = *parse_tree.getSymbol(n);
        sym.flag = old_flags.back();
        old_flags.pop_back();

        if(sym.declaration_closure_depth == closure_depth){
            size_t symbol_index = parse_tree.getFlag(n);
            parse_tree.setFlag(n, symbol_index);
        }
    }

    for(size_t i = N_val; i-->0;){
        ParseNode n = parse_tree.arg(val_list, i);
        Symbol& sym = *parse_tree.getSymbol(n);
        parse_tree.setSymbol(n, sym.shadowedVar()); //ParseNode flag should be reference to the aliased var
        resolveReference(n);
        sym.flag = old_flags.back();
        old_flags.pop_back();
    }

    for(size_t i = parse_tree.getNumArgs(ref_list); i-->0;){
        ParseNode n = parse_tree.arg(ref_list, i);
        Symbol& sym = *parse_tree.getSymbol(n);

        if(sym.declaration_closure_depth != closure_depth){
            parse_tree.setOp(n, OP_READ_UPVALUE);
            parse_tree.setFlag(n, sym.flag);
        }
    }

    closure_depth--;
}

}

}
