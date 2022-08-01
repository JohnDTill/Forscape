#include "preferences.h"
#include "ui_preferences.h"

#include "hope_common.h"
#include "keywordsubstitutioneditor.h"
#include <typeset_themes.h>
#include <typeset_view.h>
#include <QColorDialog>
#include <QScrollBar>
#include <QSettings>
#include <QWindow>
#include "symbolsubstitutioneditor.h"
#include "typeset_integral_preference.h"

#define COLOUR_SETTING "COLOUR_PRESET"
#define INTEGRAL_LAYOUT_SETTING "VERT_INTS"

Preferences::Preferences(QSettings& settings, QWidget* parent) :
    QWidget(parent), ui(new Ui::Preferences), settings(settings){
    ui->setupUi(this);

    if(settings.contains(INTEGRAL_LAYOUT_SETTING)){
        bool vertical_integrals = settings.value(INTEGRAL_LAYOUT_SETTING).toBool();
        ui->integralCheckBox->setChecked(vertical_integrals);
        Hope::Typeset::integral_bounds_vertical = vertical_integrals;
    }

    for(int i = 0; i < Hope::Typeset::NUM_COLOUR_PRESETS; i++){
        std::string_view name = Hope::Typeset::getPresetName(i).data();
        ui->colour_dropdown->addItem(name.data());
    }

    if(settings.contains(COLOUR_SETTING)){
        QString colour_preset = settings.value(COLOUR_SETTING).toString();
        if(colour_preset == "Custom") addCustomDropdownIfNotPresent();
        else for(size_t i = 0; i < Hope::Typeset::NUM_COLOUR_PRESETS; i++)
            if(colour_preset == Hope::Typeset::getPresetName(i).data()){
                Hope::Typeset::setPreset(i);
                ui->colour_dropdown->setCurrentIndex(static_cast<int>(i));
            }
    }

    for(size_t i = 0; i < Hope::Typeset::NUM_COLOUR_ROLES; i++){
        std::string_view name = Hope::Typeset::getColourName(i).data();
        if(!settings.contains(name.data())) continue;
        Hope::Typeset::setColour(i, settings.value(name.data()).value<QColor>());
    }

    connect(ui->colour_dropdown, SIGNAL(activated(int)), this, SLOT(onPresetSelect(int)));

    connect(ui->colour_table, SIGNAL(itemDoubleClicked(QTableWidgetItem*)),
            this, SLOT(onColourSelect(QTableWidgetItem*)) );

    ui->colour_table->setColumnCount(1);
    QTableWidgetItem* col_item = new QTableWidgetItem("Colour");
    ui->colour_table->setHorizontalHeaderItem(0, col_item);
    ui->colour_table->setRowCount(Hope::Typeset::NUM_COLOUR_ROLES);

    for(int i = 0; i < Hope::Typeset::NUM_COLOUR_ROLES; i++){
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setBackground(Hope::Typeset::getColour(i));
        item->setFlags(item->flags() & ~Qt::ItemFlag::ItemIsEditable);
        ui->colour_table->setItem(i, 0, item);

        QTableWidgetItem* row_item = new QTableWidgetItem();
        row_item->setText(Hope::Typeset::getColourName(i).data());
        ui->colour_table->setVerticalHeaderItem(i, row_item);
    }

    keyword_editor = new KeywordSubstitutionEditor(settings, ui->scrollArea);
    ui->scrollArea->setWidget(keyword_editor);

    symbol_editor = new SymbolSubstitutionEditor(settings, this);
    ui->symbolLayout->insertWidget(1, symbol_editor);
}

Preferences::~Preferences(){
    settings.setValue(INTEGRAL_LAYOUT_SETTING, Hope::Typeset::integral_bounds_vertical);

    settings.setValue(COLOUR_SETTING, ui->colour_dropdown->currentText());
    for(size_t i = 0; i < Hope::Typeset::NUM_COLOUR_ROLES; i++){
        std::string_view name = Hope::Typeset::getColourName(i).data();
        const QColor& colour = Hope::Typeset::getColour(i);
        settings.setValue(name.data(), colour);
    }

    delete ui;
}

void Preferences::onPresetSelect(int index){
    if(index > Hope::Typeset::NUM_COLOUR_PRESETS) return;

    Hope::Typeset::setPreset(index);

    for(size_t i = 0; i < Hope::Typeset::NUM_COLOUR_ROLES; i++){
        QTableWidgetItem* item = ui->colour_table->item(static_cast<int>(i), 0);
        item->setBackground(Hope::Typeset::getColour(i));
    }

    removeCustomDropdownIfPresent();
    emit colourChanged();
    updateWindows();
}

void Preferences::onColourSelect(QTableWidgetItem* item){
    int role = item->row();
    const QColor& current = Hope::Typeset::getColour(role);
    QColor colour = QColorDialog::getColor(current, this, Hope::Typeset::getColourName(role).data(), QColorDialog::DontUseNativeDialog);
    if(!colour.isValid() || colour == current) return;
    item->setBackground(colour);
    Hope::Typeset::setColour(role, colour);
    addCustomDropdownIfNotPresent();
    emit colourChanged();
    QCoreApplication::processEvents();
    updateWindows();
}

void Preferences::addCustomDropdownIfNotPresent(){
    if(ui->colour_dropdown->count() != Hope::Typeset::NUM_COLOUR_PRESETS) return;
    ui->colour_dropdown->addItem("Custom");
    ui->colour_dropdown->setCurrentIndex(Hope::Typeset::NUM_COLOUR_PRESETS);
}

void Preferences::removeCustomDropdownIfPresent(){
    if(ui->colour_dropdown->count() == Hope::Typeset::NUM_COLOUR_PRESETS) return;
    ui->colour_dropdown->removeItem(ui->colour_dropdown->count()-1);
}

void Preferences::updateWindows() const {
    for(auto window : QGuiApplication::topLevelWindows())
        window->requestUpdate();
}

void Preferences::on_keywordDefaultsButton_clicked(){
    keyword_editor->resetDefaults();
}


void Preferences::on_keywordAddButton_clicked(){
    keyword_editor->addSlot();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    auto scrollbar = ui->scrollArea->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}


void Preferences::on_symbolsDefaultsButton_clicked(){
    symbol_editor->resetDefaults();
}

void Preferences::on_symbolsAddButton_clicked(){
    symbol_editor->addRow();
}

void Preferences::on_integralCheckBox_toggled(bool checked){
    Hope::Typeset::integral_bounds_vertical = checked;

    for(Hope::Typeset::View* view : Hope::Typeset::View::all_views)
        view->updateModel();
}

