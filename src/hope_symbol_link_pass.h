#ifndef HOPE_SYMBOL_LINK_PASS_H
#define HOPE_SYMBOL_LINK_PASS_H

#include <hope_symbol_build_pass.h>

namespace Hope {

namespace Code {

class SymbolTableLinker{
private:
    SymbolTable& symbol_table;
    ParseTree& parse_tree;

public:
    SymbolTableLinker(SymbolTable& symbol_table, ParseTree& parse_tree) noexcept;
    void link(); //I don't believe user errors are possible
};

}

}

#endif // HOPE_SYMBOL_LINK_PASS_H
