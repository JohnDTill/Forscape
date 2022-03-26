#ifndef PREFERENCES_H
#define PREFERENCES_H

class QSettings;
class QTableWidgetItem;
#include <QWidget>

namespace Ui {
class Preferences;
}

class Preferences : public QWidget{
    Q_OBJECT

public:
    explicit Preferences(QSettings& settings, QWidget* parent = nullptr);
    ~Preferences();

private slots:
    void onPresetSelect(int index);
    void onColourSelect(QTableWidgetItem* item);

signals:
    void colourChanged();

private:
    void addCustomDropdownIfNotPresent();
    void removeCustomDropdownIfPresent();
    void updateWindows() const;
    Ui::Preferences* ui;
    QSettings& settings;
};

#endif // PREFERENCES_H
