#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#ifndef NDEBUG

#include <QTreeWidget>

#include <forscape_parse_tree.h>
#include <forscape_static_pass.h>
#include <forscape_symbol_table.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Forscape::Code::SymbolTable& symbol_table, const Forscape::Code::StaticPass& ts);
};

#endif

#endif // SYMBOLTREEVIEW_H
