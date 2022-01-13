#include "hope_symbol_link_pass.h"

namespace Hope {

namespace Code {

Hope::Code::SymbolTableLinker::SymbolTableLinker(SymbolTable& symbol_table, Hope::Code::ParseTree& parse_tree) noexcept
    : symbol_table(symbol_table), parse_tree(parse_tree) {}

void SymbolTableLinker::link(){
    std::vector<std::unordered_map<size_t, size_t>> closures;
    std::vector<size_t> closure_size;
    std::vector<size_t> stack_frame;
    size_t stack_size = 0;

    for(ScopeSegment& scope : symbol_table.scopes){
        if(scope.isStartOfScope()){
            if(scope.closure != NONE){
                closures.push_back(std::unordered_map<size_t, size_t>());
                closure_size.push_back(0);
            }

            stack_frame.push_back(stack_size);
        }

        for(size_t i = scope.usage_begin; i < scope.usage_end; i++){
            const Usage& usage = symbol_table.usages[i];

            Symbol& sym = symbol_table.symbols[usage.var_id];
            ParseNode pn = usage.pn;

            if(usage.type == DECLARE){
                if(sym.is_closure_nested && !sym.is_captured){
                    assert(sym.declaration_closure_depth <= closures.size());

                    parse_tree.setOp(pn, OP_READ_UPVALUE);
                    parse_tree.setClosureIndex(pn, closure_size.back());
                    closures.back()[usage.var_id] = closure_size.back()++;
                }else{
                    sym.flag = stack_size++;
                }
            }else{
                if(sym.is_closure_nested){
                    assert(sym.declaration_closure_depth <= closures.size());

                    for(size_t i = sym.declaration_closure_depth; i < closures.size(); i++){
                        std::unordered_map<size_t, size_t>& closure = closures[i];
                        if(closure.find(usage.var_id) == closure.end())
                            closure[usage.var_id] = closure_size[i]++;
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
            if(scope.closure != NONE){
                ParseNode fn = scope.closure;

                ParseTree::NaryBuilder upvalue_builder = parse_tree.naryBuilder(OP_LIST);
                for(const auto& entry : closures.back()){
                    size_t symbol_index = entry.first;
                    const Symbol& sym = symbol_table.symbols[symbol_index];
                    ParseNode n = parse_tree.addTerminal(OP_IDENTIFIER, sym.document_occurences.front());

                    parse_tree.setFlag(n, entry.second);

                    if(sym.declaration_closure_depth != closures.size())
                        parse_tree.setOp(n, OP_READ_UPVALUE);

                    upvalue_builder.addNaryChild(n);
                }

                ParseNode upvalue_list = upvalue_builder.finalize(parse_tree.getSelection(fn));

                switch (parse_tree.getOp(fn)) {
                    case OP_ALGORITHM:
                        parse_tree.setArg(fn, 2, upvalue_list);
                        break;
                    case OP_LAMBDA:
                        parse_tree.setArg(fn, 1, upvalue_list);
                        break;
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
