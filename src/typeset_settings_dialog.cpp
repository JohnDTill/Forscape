#include "typeset_settings_dialog.h"
#include "ui_typeset_settings_dialog.h"

#include "forscape_common.h"
#include <typeset_themes.h>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

static std::array<QComboBox*, Code::NUM_SETTINGS> combo_boxes = { 0 };

SettingsDialog* SettingsDialog::instance = nullptr;

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog) {
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // DO THIS: codegen and have better names
    combo_boxes[0] = ui->comboBox;
    combo_boxes[1] = ui->comboBox_2;

    for(QComboBox* combo_box : combo_boxes)
        connect(combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateBackgroundColour()));
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

static QColor background_colour[Code::NUM_WARNING_LEVELS+1];
static QColor text_colour[Code::NUM_WARNING_LEVELS+1];

bool SettingsDialog::execSettingsForm(const std::vector<Code::Settings::Update>& settings) noexcept {
    if(instance == nullptr) instance = new SettingsDialog;

    //Set colour coding
    background_colour[0] = instance->palette().color(QPalette::ColorRole::Base);
    background_colour[1+Code::NO_WARNING] = Typeset::getColour(Typeset::COLOUR_HIGHLIGHTSECONDARY);
    background_colour[1+Code::WARN] = Typeset::getColour(Typeset::COLOUR_WARNINGBACKGROUND);
    background_colour[1+Code::ERROR] = Typeset::getColour(Typeset::COLOUR_ERRORBACKGROUND);
    text_colour[0] = instance->palette().color(QPalette::ColorRole::Text);
    text_colour[1+Code::NO_WARNING] = Typeset::getColour(Typeset::COLOUR_TEXT);
    text_colour[1+Code::WARN] = Typeset::getColour(Typeset::COLOUR_ID);
    text_colour[1+Code::ERROR] = Typeset::getColour(Typeset::COLOUR_ID);
    for(QComboBox* combo_box : combo_boxes){
        for(int i = 0; i < Code::NUM_WARNING_LEVELS+1; i++){
            combo_box->setItemData(i, background_colour[i], Qt::ItemDataRole::BackgroundRole);
            combo_box->setItemData(i, text_colour[i], Qt::ItemDataRole::ForegroundRole);
        }
    }

    //Set active index for each entry
    for(QComboBox* combo_box : combo_boxes){
        combo_box->setFocus();
        combo_box->setCurrentIndex(0);
    }
    for(const Code::Settings::Update& update : settings){
        combo_boxes[update.setting_id]->setFocus();
        combo_boxes[update.setting_id]->setCurrentIndex( update.prev_value + 1 );
    }

    const auto user_response = instance->exec();
    if(user_response != QDialog::Accepted) return false;

    std::vector<Code::Settings::Update> altered_settings;
    populateSettingsFromForm(altered_settings);

    return (altered_settings.size() != settings.size()) ||
           !std::equal(settings.begin(), settings.end(), altered_settings.begin());
}

void SettingsDialog::populateSettingsFromForm(std::vector<Code::Settings::Update>& settings) noexcept {
    settings.clear();
    for(size_t i = 0; i < combo_boxes.size(); i++){
        auto index = combo_boxes[i]->currentIndex();
        if(index != 0) settings.push_back(
            Code::Settings::Update(static_cast<Code::SettingId>(i),
            static_cast<Code::Settings::SettingValue>(index-1))
        );
    }
}

void SettingsDialog::updateBackgroundColour() noexcept {
    QComboBox* combo_box = debug_cast<QComboBox*>(focusWidget());

    //DO THIS: why is this hack necessary?
    combo_box->setStyleSheet("QComboBox { background: " + background_colour[combo_box->currentIndex()].name() + "; }");

    QPalette palette = combo_box->palette();
    palette.setColor(combo_box->backgroundRole(), background_colour[combo_box->currentIndex()]);
    palette.setColor(QPalette::ColorRole::Text, text_colour[combo_box->currentIndex()]);
    combo_box->setPalette(palette);

    setFocus();
}

}
