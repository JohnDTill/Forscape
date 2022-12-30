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
    for(size_t scope_segment_index = 0; scope_segment_index < symbol_table.scope_segments.size(); scope_segment_index++){
        const Forscape::Code::ScopeSegment& scope_segment = symbol_table.scope_segments[scope_segment_index];
        const size_t sym_end = (&scope_segment == &symbol_table.scope_segments.back()) ?
                               symbol_table.symbols.size() :
                               symbol_table.scope_segments[scope_segment_index+1].first_sym_index;

        if(scope_segment.isStartOfScope()){
            if(scope_segment.parent_lexical_segment_index == NONE){
                items.push(nullptr);
            }else{
                Forscape::Code::ScopeSegmentIndex grandparent = symbol_table.scope_segments[scope_segment.parent_lexical_segment_index].parent_lexical_segment_index;
                QTreeWidgetItem* scope_item = (grandparent == NONE) ?
                            new QTreeWidgetItem(this) :
                            new QTreeWidgetItem(items.top());
                items.push(scope_item);
                std::string_view name = symbol_table.getName(scope_segment);
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

        for(size_t i = scope_segment.first_sym_index; i < sym_end; i++){
            QTreeWidgetItem* item = (scope_segment.parent_lexical_segment_index == NONE) ?
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

        if(scope_segment.is_end_of_scope) items.pop();
    }
}

#endif
