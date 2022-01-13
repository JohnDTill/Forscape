#include "symboltreeview.h"

#include <stack>

SymbolTreeView::SymbolTreeView(const Hope::Code::SymbolTable& symbol_table){
    setWindowTitle("Symbol Table");
    setHeaderLabels({"Name", "Type", "Const"});
    constexpr int NAME_COLUMN = 0;

    QTreeWidgetItem* last_added_item = nullptr;

    std::stack<QTreeWidgetItem*> items;
    for(const Hope::Code::ScopeSegment& scope : symbol_table.scopes){
        if(scope.isStartOfScope()){
            if(scope.parent == Hope::Code::NONE){
                items.push(nullptr);
            }else{
                Hope::Code::ScopeId grandparent = symbol_table.scopes[scope.parent].parent;
                QTreeWidgetItem* scope_item = (grandparent == Hope::Code::NONE) ?
                            new QTreeWidgetItem(this) :
                            new QTreeWidgetItem(items.top());
                items.push(scope_item);
                scope_item->setText(NAME_COLUMN, QString::fromStdString(scope.name.str()));
                for(int i = 0; i < 3; i++) scope_item->setBackground(i, QColor::fromRgb(200, 200, 255));

                if(last_added_item && last_added_item->text(0) == scope_item->text(0)) delete last_added_item;
            }
        }else if(scope.isEndOfScope()){
            items.pop();
        }

        for(size_t i = scope.sym_begin; i < scope.sym_end; i++){
            QTreeWidgetItem* item = (scope.parent == Hope::Code::NONE) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(items.top());
            const auto& symbol = symbol_table.symbols[i];
            item->setText(NAME_COLUMN, QString::fromStdString(symbol.document_occurences.front().str()));
            item->setText(2, QChar('0' + symbol.is_const));
            last_added_item = item;
        }
    }
}
