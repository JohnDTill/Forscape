#include "symbolsubstitutioneditor.h"

#include <forscape_serial.h>
#include <forscape_unicode.h>
#include <typeset_shorthand.h>
#include <qt_compatability.h>
#include <QCoreApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QSettings>
using Forscape::Typeset::Shorthand;

static int FIRST_COL = 0;
static int SECOND_COL = 1;
static int RESULT_COL = 2;

#define CLOSE_SYMBOL " ✕ "
#define FIELD_NAME "SHORTHAND"

SymbolSubstitutionEditor::SymbolSubstitutionEditor(QSettings& settings, QWidget* parent)
    : QTableWidget(parent), settings(settings) {
    setColumnCount(3);
    setHorizontalHeaderLabels({"First", "Second", "Result"});

    if(settings.contains(FIELD_NAME)) load();
    populateFromMap();

    connect(this, SIGNAL(cellChanged(int,int)), this, SLOT(onItemEdited(int,int)));
    connect(verticalHeader(), SIGNAL(sectionClicked(int)), this, SLOT(removeRow(int)));
    connect(this, SIGNAL(cellClicked(int,int)), this, SLOT(onItemEdited(int,int)));
}

SymbolSubstitutionEditor::~SymbolSubstitutionEditor(){
    QList<QVariant> list;

    for(const auto& entry : Shorthand::map){
        list.append(entry.first.first);
        list.append(entry.first.second);
        list.append(toQString(entry.second));
    }

    settings.setValue(FIELD_NAME, list);
}

void SymbolSubstitutionEditor::resetDefaults(){
    Shorthand::reset();
    clear();

    setColumnCount(3);
    setHorizontalHeaderLabels({"First", "Second", "Result"});
    blockSignals(true);
    populateFromMap();
    blockSignals(false);
}

void SymbolSubstitutionEditor::addRow(){
    blockSignals(true);

    int row = rowCount();
    setRowCount(rowCount()+1);

    QTableWidgetItem* first_item = new QTableWidgetItem();
    first_item->setTextAlignment(Qt::AlignCenter);
    first_item->setData(Qt::UserRole, uint(0));
    setItem(row, FIRST_COL, first_item);

    QTableWidgetItem* second_item = new QTableWidgetItem();
    second_item->setTextAlignment(Qt::AlignCenter);
    second_item->setData(Qt::UserRole, uint(0));
    setItem(row, SECOND_COL, second_item);

    QTableWidgetItem* final_item = new QTableWidgetItem();
    final_item->setTextAlignment(Qt::AlignCenter);
    final_item->setData(Qt::UserRole, QString());
    setItem(row, RESULT_COL, final_item);

    QTableWidgetItem* row_item = new QTableWidgetItem();
    row_item->setText(CLOSE_SYMBOL);
    setVerticalHeaderItem(row, row_item);

    blockSignals(false);

    QScrollBar* v_scroll = verticalScrollBar();
    QCoreApplication::processEvents();
    v_scroll->setValue(v_scroll->maximum());
}

void SymbolSubstitutionEditor::removeRow(int row){
    uint32_t first = getCode(row, FIRST_COL);
    uint32_t second = getCode(row, SECOND_COL);
    Shorthand::map.erase(std::make_pair(first, second));

    QTableWidget::removeRow(row);
}

void SymbolSubstitutionEditor::onItemEdited(int row, int column){
    if(column == FIRST_COL || column == SECOND_COL || column == RESULT_COL) updateRow(row);
}

void SymbolSubstitutionEditor::load(){
    assert(settings.contains(FIELD_NAME));

    QList<QVariant> list = settings.value(FIELD_NAME).toList();
    assert(list.size() % 3 == 0);
    Shorthand::map.clear();
    for(int i = 0; i < list.size(); i += 3){
        bool success;
        uint32_t first = list[i].toUInt(&success);
        assert(success);
        uint32_t second = list[i+1].toUInt(&success);
        assert(success);
        QString result = list[i+2].toString();
        assert(Shorthand::map.find(std::make_pair(first, second)) == Shorthand::map.end());
        Shorthand::map[std::make_pair(first, second)] = toCppString(result);
    }
}

void SymbolSubstitutionEditor::populateFromMap(){
    setRowCount(static_cast<int>(Shorthand::map.size()));
    int row = 0;
    for(const auto& entry : Shorthand::map){
        QTableWidgetItem* first_item = new QTableWidgetItem();
        first_item->setTextAlignment(Qt::AlignCenter);
        uint32_t first = entry.first.first;
        first_item->setData(Qt::UserRole, first);
        first_item->setText(toQString(Forscape::fromCodepointBytes(first)));
        setItem(row, FIRST_COL, first_item);

        QTableWidgetItem* second_item = new QTableWidgetItem();
        second_item->setTextAlignment(Qt::AlignCenter);
        uint32_t second = entry.first.second;
        second_item->setData(Qt::UserRole, second);
        second_item->setText(toQString(Forscape::fromCodepointBytes(second)));
        setItem(row, SECOND_COL, second_item);

        QTableWidgetItem* final_item = new QTableWidgetItem();
        final_item->setTextAlignment(Qt::AlignCenter);
        QString result = toQString(entry.second);
        final_item->setData(Qt::UserRole, result);
        final_item->setText(result);
        setItem(row, RESULT_COL, final_item);

        QTableWidgetItem* row_item = new QTableWidgetItem();
        row_item->setText(CLOSE_SYMBOL);
        setVerticalHeaderItem(row, row_item);

        row++;
    }
}

#define GOOD_BRUSH QBrush()
#define BAD_BRUSH QBrush(Qt::red)

void SymbolSubstitutionEditor::updateRow(int row) {
    uint32_t first = getCode(row, FIRST_COL);
    uint32_t second = getCode(row, SECOND_COL);
    const QString& result = item(row, RESULT_COL)->text();

    if(!first){
        item(row, FIRST_COL)->setBackground(BAD_BRUSH);
        item(row, FIRST_COL)->setToolTip("Must be a single, non-space character");
    }else{
        item(row, FIRST_COL)->setBackground(GOOD_BRUSH);
        item(row, FIRST_COL)->setToolTip(QString());
    }

    if(!second){
        item(row, SECOND_COL)->setBackground(BAD_BRUSH);
        item(row, SECOND_COL)->setToolTip("Must be a single, non-space character");
    }else{
        item(row, SECOND_COL)->setBackground(GOOD_BRUSH);
        item(row, SECOND_COL)->setToolTip(QString());
    }

    const std::string result_str = toCppString(result);
    if(result.isEmpty()){
        item(row, RESULT_COL)->setBackground(BAD_BRUSH);
        item(row, RESULT_COL)->setToolTip("Result cannot be empty");
        return;
    }else if(Forscape::isIllFormedUtf8(result_str)){
        item(row, RESULT_COL)->setBackground(BAD_BRUSH);
        item(row, RESULT_COL)->setToolTip("Invalid UTF-8");
        return;
    }else if(!Forscape::isValidSerial(result_str)){
        item(row, RESULT_COL)->setBackground(BAD_BRUSH);
        item(row, RESULT_COL)->setToolTip("Invalid Forscape typesetting characters");
        return;
    }else{
        item(row, RESULT_COL)->setBackground(GOOD_BRUSH);
        item(row, RESULT_COL)->setToolTip(QString());
    }

    if(!first || !second) return;

    uint32_t old_first = item(row, FIRST_COL)->data(Qt::UserRole).toUInt();
    uint32_t old_second = item(row, SECOND_COL)->data(Qt::UserRole).toUInt();
    QString old_result = item(row, RESULT_COL)->data(Qt::UserRole).toString();
    bool key_unchanged = (first == old_first && second == old_second);

    if(key_unchanged){
        if(result == old_result || result.isEmpty()) return;
        Shorthand::map[std::make_pair(first, second)] = result_str;
        blockSignals(true);
        item(row, RESULT_COL)->setData(Qt::UserRole, result);
        blockSignals(false);
    }else if(!result.isEmpty()){
        auto in = Shorthand::map.insert({std::make_pair(first, second), result_str});
        if(in.second){
            Shorthand::map.erase(std::make_pair(old_first, old_second));
            blockSignals(true);
            item(row, FIRST_COL)->setData(Qt::UserRole, first);
            item(row, SECOND_COL)->setData(Qt::UserRole, second);
            item(row, RESULT_COL)->setData(Qt::UserRole, result);
            blockSignals(false);
        }else{
            item(row, FIRST_COL)->setBackground(BAD_BRUSH);
            item(row, FIRST_COL)->setToolTip("Previously defined");
            item(row, SECOND_COL)->setBackground(BAD_BRUSH);
            item(row, SECOND_COL)->setToolTip("Previously defined");
        }
    }
}

uint32_t SymbolSubstitutionEditor::getCode(int row, int col) const {
    assert(col == FIRST_COL || col == SECOND_COL);
    assert(row < rowCount());

    std::string str = toCppString(item(row, col)->text());
    if(!Forscape::isSingleCodepoint(str) || str[0] == ' ') return 0;
    return Forscape::codepointInt(str);
}
