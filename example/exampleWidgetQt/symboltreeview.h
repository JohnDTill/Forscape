#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#include <QTreeWidget>

#include <hope_parse_tree.h>
#include <hope_scope_tree.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Hope::Code::SymbolTable& symbol_table);
};

#endif // SYMBOLTREEVIEW_H
