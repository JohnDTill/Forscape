#include "preferences.h"
#include "ui_preferences.h"

#include <typeset_themes.h>
#include <QColorDialog>
#include <QSettings>
#include <QWindow>

//DO THIS - make sure all colours have roles
//DO THIS - make sure everything uses colour themes - toolbars, window, etcetera

Preferences::Preferences(QSettings& settings, QWidget* parent) :
    QWidget(parent), ui(new Ui::Preferences), settings(settings){
    ui->setupUi(this);

    for(int i = 0; i < Hope::Typeset::NUM_COLOUR_PRESETS; i++){
        std::string_view name = Hope::Typeset::getPresetName(i).data();
        ui->colour_dropdown->addItem(name.data());
    }

    if(settings.contains("COLOUR_PRESET")){
        QString colour_preset = settings.value("COLOUR_PRESET").toString();
        if(colour_preset == "Custom") addCustomDropdownIfNotPresent();
        else for(size_t i = 0; i < Hope::Typeset::NUM_COLOUR_PRESETS; i++)
            if(colour_preset == Hope::Typeset::getPresetName(i).data()){
                Hope::Typeset::setPreset(i);
                ui->colour_dropdown->setCurrentIndex(i);
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
}

Preferences::~Preferences(){
    settings.setValue("COLOUR_PRESET", ui->colour_dropdown->currentText());
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
        QTableWidgetItem* item = ui->colour_table->item(i, 0);
        item->setBackground(Hope::Typeset::getColour(i));
    }

    removeCustomDropdownIfPresent();
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

void Preferences::updateWindows() const{
    for(auto window : QGuiApplication::topLevelWindows())
        window->requestUpdate();
}
