#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#include <QTreeWidget>

#include <hope_parse_tree.h>
#include <hope_symbol_table.h>
#include <hope_type_system.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Hope::Code::SymbolTable& symbol_table, const Hope::Code::TypeSystem& ts);
};

#endif // SYMBOLTREEVIEW_H
