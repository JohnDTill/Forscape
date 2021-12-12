#ifndef HOPE_SYMBOL_LINK_PASS_H
#define HOPE_SYMBOL_LINK_PASS_H

#include <hope_symbol_build_pass.h>

namespace Hope {

namespace Code {

class SymbolTableLinker{
private:
    SymbolTable& symbol_table;
    ParseTree& parse_tree;
    std::vector<std::unordered_map<size_t, size_t>> closures;
    std::vector<size_t> closure_size;
    std::vector<size_t> stack_frame;
    size_t stack_size;

public:
    SymbolTableLinker(SymbolTable& symbol_table, ParseTree& parse_tree) noexcept;
    void link(); //I don't believe user errors are possible

private:
    void reset() noexcept;
    void link(size_t scope_index);
};

}

}

#endif // HOPE_SYMBOL_LINK_PASS_H
