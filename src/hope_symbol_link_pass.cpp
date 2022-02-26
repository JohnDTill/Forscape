#include "hope_symbol_link_pass.h"

namespace Hope {

namespace Code {

Hope::Code::SymbolTableLinker::SymbolTableLinker(SymbolTable& symbol_table, Hope::Code::ParseTree& parse_tree) noexcept
    : symbol_table(symbol_table), parse_tree(parse_tree) {}

void SymbolTableLinker::link() noexcept{
    for(ScopeSegment& scope : symbol_table.scopes){
        if(scope.isStartOfScope()){
            if(scope.fn != NONE){
                bool is_alg = (parse_tree.getOp(scope.fn) != OP_LAMBDA);
                ParseNode val_list = parse_tree.arg(scope.fn, is_alg);
                ParseNode ref_list = parse_tree.arg(scope.fn, 1+is_alg);
                size_t N_cap = val_list == NONE ? 0 : parse_tree.getNumArgs(val_list);

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
                bool is_alg = (parse_tree.getOp(fn) != OP_LAMBDA);
                ParseNode val_list = parse_tree.arg(fn, is_alg);
                ParseNode ref_list = parse_tree.arg(fn, 1+is_alg);
                size_t N_val = val_list == NONE ? 0 : parse_tree.getNumArgs(val_list);

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
