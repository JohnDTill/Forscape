#ifndef PREFERENCES_H
#define PREFERENCES_H

class KeywordSubstitutionEditor;
class QSettings;
class QTableWidgetItem;
class SymbolSubstitutionEditor;
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
    void on_symbolsDefaultsButton_clicked();
    void on_symbolsAddButton_clicked();

signals:
    void colourChanged();

private:
    void addCustomDropdownIfNotPresent();
    void removeCustomDropdownIfPresent();
    void updateWindows() const;
    Ui::Preferences* ui;
    QSettings& settings;
    KeywordSubstitutionEditor* keyword_editor;
    SymbolSubstitutionEditor* symbol_editor;
};

#endif // PREFERENCES_H
