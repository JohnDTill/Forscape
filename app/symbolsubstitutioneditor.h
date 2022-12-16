#ifndef SYMBOLSUBSTITUTIONEDITOR_H
#define SYMBOLSUBSTITUTIONEDITOR_H

#include <QTableWidget>

class QSettings;

class SymbolSubstitutionEditor : public QTableWidget{
    Q_OBJECT

public:
    SymbolSubstitutionEditor(QSettings& settings, QWidget* parent = nullptr);
    ~SymbolSubstitutionEditor();
    void resetDefaults();
    void addRow();

private slots:
    void removeRow(int row);
    void onItemEdited(int row, int column);

private:
    void load();
    void populateFromMap();
    void updateRow(int row);
    uint32_t getCode(int row, int col) const;
    QSettings& settings;

signals:

};

#endif // SYMBOLSUBSTITUTIONEDITOR_H
