#ifndef HOPE_SYMBOL_LINK_PASS_H
#define HOPE_SYMBOL_LINK_PASS_H

#include <hope_symbol_build_pass.h>

namespace Hope {

namespace Code {

class SymbolTableLinker{
private:
    std::vector<size_t> stack_frame;
    std::vector<size_t> old_flags;
    size_t stack_size = 0;
    size_t closure_depth = 0;
    SymbolTable& symbol_table;
    ParseTree& parse_tree;

public:
    SymbolTableLinker(SymbolTable& symbol_table, ParseTree& parse_tree) noexcept;
    void link() noexcept; //No user errors possible at link stage
};

}

}

#endif // HOPE_SYMBOL_LINK_PASS_H
