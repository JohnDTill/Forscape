#ifndef PREFERENCES_H
#define PREFERENCES_H

class KeywordSubtitutionEditor;
class QSettings;
class QTableWidgetItem;
#include <QWidget>

namespace Ui {
class Preferences;
}

class Preferences : public QWidget{
    Q_OBJECT

public:
    Preferences(QSettings& settings, QWidget* parent = nullptr);
    ~Preferences();

private slots:
    void onPresetSelect(int index);
    void onColourSelect(QTableWidgetItem* item);
    void on_keywordDefaultsButton_clicked();

    void on_keywordAddButton_clicked();

signals:
    void colourChanged();

private:
    void addCustomDropdownIfNotPresent();
    void removeCustomDropdownIfPresent();
    void updateWindows() const;
    Ui::Preferences* ui;
    QSettings& settings;
    KeywordSubtitutionEditor* keyword_editor;
};

#endif // PREFERENCES_H
