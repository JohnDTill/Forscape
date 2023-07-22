#include "typeset_settings_dialog.h"
#include "ui_typeset_settings_dialog.h"

#include "forscape_common.h"
#include <typeset_themes.h>
#include <QComboBox>
#include <QLabel>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

static std::array<QComboBox*, NUM_CODE_SETTINGS> combo_boxes = { 0 };

SettingsDialog* SettingsDialog::instance = nullptr;

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog) {
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QFormLayout* layout = debug_cast<QFormLayout*>(ui->scrollAreaWidgetContents_2->layout());

    for(size_t i = 0; i < NUM_CODE_SETTINGS; i++){
        QComboBox* combo_box = new QComboBox(ui->scrollArea);
        combo_box->addItems({"", COMMA_SEPARATED_WARNING_LABELS});
        combo_box->setItemData(0, "Maintain the previous setting", Qt::ToolTipRole);
        for(int i = 0; i < NUM_WARNING_LEVELS; i++)
            combo_box->setItemData(i+1, warning_descriptions[i].data(), Qt::ToolTipRole);

        connect(combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(updateBackgroundColour()));
        QLabel* label = new QLabel(setting_id_labels[i].data() + QString(':'), ui->scrollArea);
        label->setToolTip(setting_descriptions[i].data());
        layout->addRow(label, combo_box);

        combo_boxes[i] = combo_box;
    }
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::updateInherited(const std::array<SettingValue, NUM_CODE_SETTINGS>& inherited) alloc_except {
    if(instance == nullptr) instance = new SettingsDialog;

    for(size_t i = 0; i < NUM_CODE_SETTINGS; i++) {
        QComboBox* combo_box = combo_boxes[i];
        const SettingValue value = inherited[i];
        combo_box->setItemText(0, QString("Inherited (") + warning_labels[value].data() + ")");
    }
}

static QColor background_colour[NUM_WARNING_LEVELS+1];
static QColor text_colour[NUM_WARNING_LEVELS+1];

bool SettingsDialog::execSettingsForm(const std::vector<Code::Settings::Update>& settings) noexcept {
    if(instance == nullptr) instance = new SettingsDialog;

    //Set colour coding
    background_colour[0] = instance->palette().color(QPalette::ColorRole::Base);
    background_colour[1+NO_WARNING] = Typeset::getColour(Typeset::COLOUR_HIGHLIGHTSECONDARY);
    background_colour[1+WARN] = Typeset::getColour(Typeset::COLOUR_WARNINGBACKGROUND);
    background_colour[1+ERROR] = Typeset::getColour(Typeset::COLOUR_ERRORBACKGROUND);
    text_colour[0] = instance->palette().color(QPalette::ColorRole::Text);
    text_colour[1+NO_WARNING] = Typeset::getColour(Typeset::COLOUR_TEXT);
    text_colour[1+WARN] = Typeset::getColour(Typeset::COLOUR_ID);
    text_colour[1+ERROR] = Typeset::getColour(Typeset::COLOUR_ID);
    for(QComboBox* combo_box : combo_boxes){
        for(int i = 0; i < NUM_WARNING_LEVELS+1; i++){
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
            Code::Settings::Update(static_cast<SettingId>(i),
            static_cast<SettingValue>(index-1))
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
