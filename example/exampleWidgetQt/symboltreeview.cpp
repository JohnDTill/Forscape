#include "symboltreeview.h"

SymbolTreeView::SymbolTreeView(const Hope::Code::ParseTree& parse_tree, const Hope::Code::SymbolTable& symbol_table){
    setWindowTitle("Symbol Table");
    setHeaderLabels({"Name", "Type", "Const"});
    resolveScope(parse_tree, symbol_table, 0, nullptr);
}

void SymbolTreeView::resolveScope(
        const Hope::Code::ParseTree& parse_tree,
        const Hope::Code::SymbolTable& symbol_table,
        size_t index,
        QTreeWidgetItem* parent
        ){
    if(index == Hope::Code::NONE) return; //DO THIS - shouldn't be necessary

    const Hope::Code::Scope& scope = symbol_table.scopes[index];

    for(size_t i = 0; i < scope.subscopes.size()-1; i++){
        const Hope::Code::Scope::Subscope& subscope = scope.subscopes[i];
        resolveSubscope(parse_tree, symbol_table, subscope, parent);
        QTreeWidgetItem* item = (parent == nullptr) ?
                    new QTreeWidgetItem(this) :
                    new QTreeWidgetItem(parent);
        item->setText(0, "Subscope " + QString::number(i)); //DO THIS - give a real name
        resolveScope(parse_tree, symbol_table, subscope.subscope_id, item);
    }
    resolveSubscope(parse_tree, symbol_table, scope.subscopes.back(), parent);
}

void SymbolTreeView::resolveSubscope(
        const Hope::Code::ParseTree& parse_tree,
        const Hope::Code::SymbolTable& symbol_table,
        const Hope::Code::Scope::Subscope& subscope,
        QTreeWidgetItem* parent
        ){
    for(const auto& usage : subscope.usages){
        if(usage.type == Hope::Code::Scope::DECLARE){
            QTreeWidgetItem* item = (parent == nullptr) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(parent);
            constexpr int NAME_COLUMN = 0;
            const auto& symbol = symbol_table.symbols[usage.var_id];
            item->setText(NAME_COLUMN, QString::fromStdString(parse_tree.str(usage.pn)));
            item->setText(2, QChar('0' + symbol.is_const));
        }
    }
}
