#ifndef TYPESET_SETTINGS_DIALOG_H
#define TYPESET_SETTINGS_DIALOG_H

#include <QDialog>
#include "forscape_dynamic_settings.h"

namespace Ui {
class SettingsDialog;
}

namespace Forscape {

class SettingsDialog : public QDialog {
    Q_OBJECT
    static SettingsDialog* instance;
    explicit SettingsDialog(QWidget *parent = nullptr);
    static SettingsDialog* get() noexcept;

public:
    ~SettingsDialog();
    static bool execSettingsForm(const std::vector<Code::Settings::Update>& settings) noexcept;
    static void populateSettingsFromForm(std::vector<Code::Settings::Update>& settings) noexcept;

private slots:
    void updateBackgroundColour() noexcept;

private:
    Ui::SettingsDialog* ui;
};

}

#endif // TYPESET_SETTINGS_DIALOG_H
