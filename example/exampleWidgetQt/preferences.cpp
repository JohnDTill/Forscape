#include "preferences.h"
#include "ui_preferences.h"

#include <typeset_themes.h>
#include <QColorDialog>
#include <QSettings>
#include <QWindow>

//DO THIS - make sure all colours have roles
//DO THIS - make sure everything uses colour themes - toolbars, window, etcetera
//DO THIS - cache colour preferences in settings

Preferences::Preferences(QWidget* parent) :
    QWidget(parent), ui(new Ui::Preferences){
    ui->setupUi(this);

    connect(ui->colour_dropdown, SIGNAL(activated(int)), this, SLOT(onPresetSelect(int)));

    for(int i = 0; i < Hope::Typeset::NUM_COLOUR_PRESETS; i++){
        std::string_view name = Hope::Typeset::getPresetName(i).data();
        ui->colour_dropdown->addItem(name.data());
    }

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
