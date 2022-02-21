#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#include <QTreeWidget>

#include <hope_parse_tree.h>
#include <hope_symbol_table.h>
#include <hope_type_resolver.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Hope::Code::SymbolTable& symbol_table, const Hope::Code::TypeResolver& ts);
};

#endif // SYMBOLTREEVIEW_H
