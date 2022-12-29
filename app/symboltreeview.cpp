#include "symboltreeview.h"

#ifndef NDEBUG

#include <stack>

using Forscape::NONE; //But there is

SymbolTreeView::SymbolTreeView(const Forscape::Code::SymbolTable& symbol_table, const Forscape::Code::StaticPass& ts){
    setWindowTitle("Symbol Table");
    constexpr size_t N_FIELDS = 4;
    setHeaderLabels({"Name", "Type", "Const", "Description"});
    constexpr int NAME_COLUMN = 0;

    QTreeWidgetItem* last_added_item = nullptr;

    std::stack<QTreeWidgetItem*> items;
    for(const Forscape::Code::ScopeSegment& scope : symbol_table.scope_segments){
        if(scope.isStartOfScope()){
            if(scope.parent_lexical_segment_index == NONE){
                items.push(nullptr);
            }else{
                Forscape::Code::ScopeSegmentIndex grandparent = symbol_table.scope_segments[scope.parent_lexical_segment_index].parent_lexical_segment_index;
                QTreeWidgetItem* scope_item = (grandparent == NONE) ?
                            new QTreeWidgetItem(this) :
                            new QTreeWidgetItem(items.top());
                items.push(scope_item);
                std::string_view name = symbol_table.getName(scope);
                scope_item->setText(NAME_COLUMN, QString::fromStdString(std::string(name)));
                for(int i = 0; i < N_FIELDS; i++) scope_item->setBackground(i, QColor::fromRgb(200, 200, 255));

                if(last_added_item && last_added_item->text(0) == scope_item->text(0)){
                    for(int i = 1; i < N_FIELDS; i++)
                        scope_item->setText(i, last_added_item->text(i));

                    delete last_added_item;
                    last_added_item = nullptr;
                }
            }
        }

        for(size_t i = scope.first_sym_index; i < scope.exclusive_end_sym_index; i++){
            QTreeWidgetItem* item = (scope.parent_lexical_segment_index == NONE) ?
                        new QTreeWidgetItem(this) :
                        new QTreeWidgetItem(items.top());
            const auto& symbol = symbol_table.symbols[i];
            item->setText(NAME_COLUMN, QString::fromStdString(symbol_table.getSel(i).str()));
            item->setText(1, QString::fromStdString(ts.typeString(symbol)));
            item->setText(2, QChar('0' + symbol.is_const));
            if(symbol.comment != NONE){
                std::string desc = symbol_table.parse_tree.str(symbol.comment);
                item->setText(3, QString::fromStdString(desc));
            }
            last_added_item = item;
        }

        if(scope.isEndOfScope()) items.pop();
    }
}

#endif
