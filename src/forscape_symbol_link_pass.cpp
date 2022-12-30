#include "forscape_symbol_link_pass.h"

#include "forscape_parse_tree.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

Forscape::Code::SymbolTableLinker::SymbolTableLinker(SymbolTable& symbol_table, Forscape::Code::ParseTree& parse_tree) noexcept
    : symbol_table(symbol_table), parse_tree(parse_tree) {}

void SymbolTableLinker::link() noexcept{
    for(const ScopeSegment& scope_segment : symbol_table.scope_segments){
        if(scope_segment.isStartOfScope()){
            if(scope_segment.fn != NONE){
                ParseNode val_list = parse_tree.valCapList(scope_segment.fn);
                ParseNode ref_list = parse_tree.refCapList(scope_segment.fn);
                size_t N_cap = parse_tree.valListSize(val_list);

                for(size_t i = 0; i < N_cap; i++){
                    size_t var_id = scope_segment.first_sym_index+i;
                    Symbol& sym = symbol_table.symbols[var_id];
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

            stack_frame.push_back(stack_size);
        }

        for(size_t i = scope_segment.usage_begin; i < scope_segment.usage_end; i++){
            const SymbolUsage& usage = symbol_table.symbol_usages[i];

            Symbol& sym = symbol_table.symbols[usage.symbol_index];
            ParseNode pn = usage.pn;

            if(usage.isDeclaration()){
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

        if(scope_segment.is_end_of_scope){
            if(scope_segment.fn != NONE){
                ParseNode fn = scope_segment.fn;
                ParseNode val_list = parse_tree.valCapList(fn);
                ParseNode ref_list = parse_tree.refCapList(fn);
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
                    size_t symbol_index = parse_tree.getFlag(n);
                    Symbol& sym = symbol_table.symbols[symbol_index];
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

            stack_size = stack_frame.back();
            stack_frame.pop_back();
        }
    }
}

}

}
