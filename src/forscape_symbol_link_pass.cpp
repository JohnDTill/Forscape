#include "forscape_symbol_link_pass.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

Forscape::Code::SymbolTableLinker::SymbolTableLinker(SymbolTable& symbol_table, Forscape::Code::ParseTree& parse_tree) noexcept
    : symbol_table(symbol_table), parse_tree(parse_tree) {}

void SymbolTableLinker::link() noexcept{
    for(ScopeSegment& scope : symbol_table.scopes){
        if(scope.isStartOfScope()){
            if(scope.fn != NONE){
                ParseNode val_list = parse_tree.valCapList(scope.fn);
                ParseNode ref_list = parse_tree.refCapList(scope.fn);
                size_t N_cap = parse_tree.valListSize(val_list);

                for(size_t i = 0; i < N_cap; i++){
                    size_t var_id = scope.sym_begin+i;
                    Symbol& sym = symbol_table.symbols[var_id];
                    old_flags.push_back(sym.flag);
                    sym.flag = i;
                }

                for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
                    ParseNode ref = parse_tree.arg(ref_list, i);
                    size_t var_id = parse_tree.getFlag(ref);
                    Symbol& sym = symbol_table.symbols[var_id];
                    old_flags.push_back(sym.flag);
                    sym.flag = N_cap + i;
                }

                closure_depth++;
            }

            stack_frame.push_back(stack_size);
        }

        for(size_t i = scope.usage_begin; i < scope.usage_end; i++){
            const Usage& usage = symbol_table.usages[i];

            Symbol& sym = symbol_table.symbols[usage.var_id];
            ParseNode pn = usage.pn;

            if(usage.type == DECLARE){
                if(sym.is_closure_nested && !sym.is_captured_by_value){
                    parse_tree.setOp(pn, OP_READ_UPVALUE);
                    parse_tree.setClosureIndex(pn, sym.flag);
                }else if(!sym.is_captured_by_value){
                    sym.flag = stack_size++;
                }
            }else{
                if(sym.is_closure_nested){
                    parse_tree.setOp(pn, OP_READ_UPVALUE);
                    parse_tree.setClosureIndex(pn, sym.flag);
                }else if(sym.declaration_closure_depth == 0){
                    parse_tree.setOp(pn, OP_READ_GLOBAL);
                    parse_tree.setGlobalIndex(pn, sym.flag);
                }else{
                    parse_tree.setStackOffset(pn, stack_size - 1 - sym.flag);
                }
            }
        }

        if(scope.isEndOfScope()){
            if(scope.fn != NONE){
                ParseNode fn = scope.fn;
                ParseNode val_list = parse_tree.valCapList(fn);
                ParseNode ref_list = parse_tree.refCapList(fn);
                size_t N_val = parse_tree.valListSize(val_list);

                for(size_t i = parse_tree.getNumArgs(ref_list); i-->0;){
                    ParseNode n = parse_tree.arg(ref_list, i);
                    size_t symbol_index = parse_tree.getFlag(n);
                    Symbol& sym = symbol_table.symbols[symbol_index];
                    sym.flag = old_flags.back();
                    old_flags.pop_back();

                    if(sym.declaration_closure_depth == closure_depth)
                        parse_tree.setFlag(n, symbol_index);
                }

                for(size_t i = N_val; i-->0;){
                    ParseNode n = parse_tree.arg(val_list, i);
                    size_t symbol_index = parse_tree.getFlag(n);
                    Symbol& sym = symbol_table.symbols[symbol_index];
                    sym.flag = old_flags.back();
                    old_flags.pop_back();
                }

                for(size_t i = parse_tree.getNumArgs(ref_list); i-->0;){
                    ParseNode n = parse_tree.arg(ref_list, i);
                    size_t symbol_index = parse_tree.getFlag(n);
                    Symbol& sym = symbol_table.symbols[symbol_index];

                    if(sym.declaration_closure_depth != closure_depth){
                        parse_tree.setOp(n, OP_READ_UPVALUE);
                        parse_tree.setFlag(n, sym.flag);
                    }
                }

                closure_depth--;
            }

            stack_size = stack_frame.back();
            stack_frame.pop_back();
        }
    }
}

}

}
