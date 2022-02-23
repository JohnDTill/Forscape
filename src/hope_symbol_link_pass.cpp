#include "hope_symbol_link_pass.h"

namespace Hope {

namespace Code {

Hope::Code::SymbolTableLinker::SymbolTableLinker(SymbolTable& symbol_table, Hope::Code::ParseTree& parse_tree) noexcept
    : symbol_table(symbol_table), parse_tree(parse_tree) {}

void SymbolTableLinker::link(){
    //DO THIS - should be simpler now that reference list is constructed in builder

    std::vector<std::unordered_map<size_t, size_t>> closures;
    std::vector<size_t> closure_size;
    std::vector<size_t> stack_frame;
    size_t stack_size = 0;

    for(ScopeSegment& scope : symbol_table.scopes){
        if(scope.isStartOfScope()){
            if(scope.fn != NONE){
                closures.push_back(std::unordered_map<size_t, size_t>());
                closure_size.push_back(0);

                bool is_alg = (parse_tree.getOp(scope.fn) != OP_LAMBDA);
                ParseNode cap_list = parse_tree.arg(scope.fn, is_alg);
                ParseNode ref_list = parse_tree.arg(scope.fn, 1+is_alg);

                if(cap_list != NONE){
                    for(size_t i = 0; i < parse_tree.getNumArgs(cap_list); i++){
                        size_t var_id = scope.sym_begin+i;
                        closures.back()[var_id] = closure_size.back()++;
                    }
                }

                for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
                    ParseNode ref = parse_tree.arg(ref_list, i);
                    size_t var_id = parse_tree.getFlag(ref);
                    closures.back()[var_id] = closure_size.back()++;
                }
            }

            stack_frame.push_back(stack_size);
        }

        for(size_t i = scope.usage_begin; i < scope.usage_end; i++){
            const Usage& usage = symbol_table.usages[i];

            Symbol& sym = symbol_table.symbols[usage.var_id];
            ParseNode pn = usage.pn;

            if(usage.type == DECLARE){
                if(sym.is_closure_nested && !sym.is_captured_by_value){
                    assert(sym.declaration_closure_depth <= closures.size());

                    parse_tree.setOp(pn, OP_READ_UPVALUE);
                    auto lookup = closures.back().find(usage.var_id);
                    assert(lookup != closures.back().end());
                    parse_tree.setClosureIndex(pn, lookup->second);
                }else if(!sym.is_captured_by_value){
                    sym.flag = stack_size++;
                }
            }else{
                if(sym.is_closure_nested){
                    assert(sym.declaration_closure_depth <= closures.size());

                    for(size_t i = sym.declaration_closure_depth; i < closures.size(); i++){
                        std::unordered_map<size_t, size_t>& closure = closures[i];
                        assert(closure.find(usage.var_id) != closure.end());
                    }
                    parse_tree.setOp(pn, OP_READ_UPVALUE);
                    parse_tree.setClosureIndex(pn, closures.back()[usage.var_id]);
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
                ParseNode ref_list = parse_tree.arg(fn, 1+is_alg);

                for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
                    ParseNode n = parse_tree.arg(ref_list, i);
                    size_t symbol_index = parse_tree.getFlag(n);
                    const Symbol& sym = symbol_table.symbols[symbol_index];

                    if(sym.declaration_closure_depth != closures.size()){
                        parse_tree.setOp(n, OP_READ_UPVALUE);
                        const auto& outer_closure = closures[closures.size()-2];
                        auto lookup = outer_closure.find(symbol_index);
                        assert(lookup != outer_closure.end());
                        parse_tree.setFlag(n, lookup->second);
                    }else if(sym.is_captured_by_value){
                        continue;
                    }else{
                        parse_tree.setFlag(n, symbol_index);
                    }
                }

                closures.pop_back();
                closure_size.pop_back();
            }

            stack_size = stack_frame.back();
            stack_frame.pop_back();
        }
    }
}

}

}
