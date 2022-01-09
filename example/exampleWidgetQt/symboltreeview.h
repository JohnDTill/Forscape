#ifndef SYMBOLTREEVIEW_H
#define SYMBOLTREEVIEW_H

#include <QTreeWidget>

#include <hope_symbol_build_pass.h>

class SymbolTreeView : public QTreeWidget {
public:
    SymbolTreeView(const Hope::Code::ParseTree& parse_tree, const Hope::Code::SymbolTable& symbol_table);

private:
    void resolveScope(
            const Hope::Code::ParseTree& parse_tree,
            const Hope::Code::SymbolTable& symbol_table,
            size_t index,
            QTreeWidgetItem* parent
            );

    void resolveSubscope(
            const Hope::Code::ParseTree& parse_tree,
            const Hope::Code::SymbolTable& symbol_table,
            const Hope::Code::Scope::Subscope& subscope,
            QTreeWidgetItem* parent
            );
};

#endif // SYMBOLTREEVIEW_H
