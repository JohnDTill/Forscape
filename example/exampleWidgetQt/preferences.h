#ifndef PREFERENCES_H
#define PREFERENCES_H

class QTableWidgetItem;
#include <QWidget>

namespace Ui {
class Preferences;
}

class Preferences : public QWidget{
    Q_OBJECT

public:
    explicit Preferences(QWidget* parent = nullptr);
    ~Preferences();

private slots:
    void onPresetSelect(int index);
    void onColourSelect(QTableWidgetItem* item);

private:
    void addCustomDropdownIfNotPresent();
    void removeCustomDropdownIfPresent();
    void updateWindows() const;
    Ui::Preferences* ui;
};

#endif // PREFERENCES_H
