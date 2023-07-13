#include "typeset_settings_dialog.h"
#include "ui_typeset_settings_dialog.h"

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

    // DO THIS: codegen
    combo_boxes[0] = ui->comboBox;
    combo_boxes[1] = ui->comboBox_2;
}

SettingsDialog* SettingsDialog::get() noexcept {
    if(instance == nullptr) instance = new SettingsDialog;
    return instance;
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

bool SettingsDialog::execSettingsForm(const std::vector<Code::Settings::Update>& settings) noexcept {
    if(instance == nullptr) instance = new SettingsDialog;

    for(QComboBox* combo_box : combo_boxes) combo_box->setCurrentIndex(0);
    for(const Code::Settings::Update& update : settings)
        combo_boxes[update.setting_id]->setCurrentIndex( update.prev_value + 1 );

    const auto user_action = get()->exec();

    return user_action == QDialog::Accepted;
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

}
