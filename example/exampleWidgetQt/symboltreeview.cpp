#include "symboltreeview.h"

SymbolTreeView::SymbolTreeView(const Hope::Code::SymbolTable& symbol_table){
    setWindowTitle("Symbol Table");
    setHeaderLabels({"Name", "Type", "Const"});
    constexpr int NAME_COLUMN = 0;

    std::vector<QTreeWidgetItem*> items;
    for(const Hope::Code::Scope& scope : symbol_table.scopes){
        if(scope.parent == Hope::Code::NONE){
            items.push_back(nullptr);
        }else{
            Hope::Code::ScopeId grandparent = symbol_table.scopes[scope.parent].parent;
            QTreeWidgetItem* scope_item = (grandparent == Hope::Code::NONE) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(items[scope.parent]);
            items.push_back(scope_item);
            scope_item->setText(NAME_COLUMN, QString::fromStdString(scope.name.str()));
        }

        for(size_t i = scope.sym_begin; i < scope.sym_end; i++){
            QTreeWidgetItem* item = (scope.parent == Hope::Code::NONE) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(items.back());
            const auto& symbol = symbol_table.symbols[i];
            item->setText(NAME_COLUMN, QString::fromStdString(symbol.document_occurences.front().str()));
            item->setText(2, QChar('0' + symbol.is_const));
        }
    }
}
