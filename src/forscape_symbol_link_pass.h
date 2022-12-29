#ifndef FORSCAPE_SYMBOL_LINK_PASS_H
#define FORSCAPE_SYMBOL_LINK_PASS_H

#include <forscape_symbol_table.h>

namespace Forscape {

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

#endif // FORSCAPE_SYMBOL_LINK_PASS_H
