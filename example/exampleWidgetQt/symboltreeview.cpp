#include "symboltreeview.h"

#include <stack>

#include "hope_type_resolver.h"

SymbolTreeView::SymbolTreeView(const Hope::Code::SymbolTable& symbol_table, const Hope::Code::TypeResolver& ts){
    setWindowTitle("Symbol Table");
    constexpr size_t N_FIELDS = 4;
    setHeaderLabels({"Name", "Type", "Const", "Description"});
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
                for(int i = 0; i < N_FIELDS; i++) scope_item->setBackground(i, QColor::fromRgb(200, 200, 255));

                if(last_added_item && last_added_item->text(0) == scope_item->text(0)){
                    for(int i = 1; i < N_FIELDS; i++){
                        scope_item->setText(i, last_added_item->text(i));
                    }

                    delete last_added_item;
                    last_added_item = nullptr;
                }
            }
        }

        for(size_t i = scope.sym_begin; i < scope.sym_end; i++){
            QTreeWidgetItem* item = (scope.parent == Hope::Code::NONE) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(items.top());
            const auto& symbol = symbol_table.symbols[i];
            item->setText(NAME_COLUMN, QString::fromStdString(symbol_table.getSel(i).str()));
            item->setText(1, QString::fromStdString(ts.typeString(symbol.type)));
            item->setText(2, QChar('0' + symbol.is_const));
            if(symbol.comment != Hope::Code::ParseTree::EMPTY){
                std::string desc = symbol_table.parse_tree.str(symbol.comment);
                item->setText(3, QString::fromStdString(desc));
            }
            last_added_item = item;
        }

        if(scope.isEndOfScope()){
            items.pop();
        }
    }
}
