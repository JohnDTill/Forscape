#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#include <QTreeWidget>

#include <hope_parse_tree.h>
#include <hope_static_pass.h>
#include <hope_symbol_table.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Hope::Code::SymbolTable& symbol_table, const Hope::Code::StaticPass& ts);
};

#endif // SYMBOLTREEVIEW_H
