#ifndef MATHTOOLBAR_H
#define MATHTOOLBAR_H

#include <QToolBar>
class QTableWidget;
class QTableWidgetItem;

class MathToolbar : public QToolBar{
    Q_OBJECT

private:
    QTableWidget* symbol_table; //EVENTUALLY: symbols can change based on keywords

public:
    MathToolbar(QWidget* parent = nullptr);

signals:
    void insertSerial(QString);
    void insertSerialSelection(QString, QString);
    void insertFlatText(QString);

private:
    void setupSymbolTable();
    void setupScripts();
    void setupAccents();
    void setupMisc();
    void setupBigSymbols();
    void setupIntegrals();
    void setupGroupings();

private slots:
    void symbolTableTriggered(QTableWidgetItem* item);
    void insertText();
    void insertWithSelection();

private:
    class TypesetAction : public QAction{
    public:
        const QString command;
        TypesetAction(const QString& text,
                      const QString& tooltip,
                      const QString& command,
                      MathToolbar* parent);
    };

    class EnclosedTypesetAction : public QAction{
    public:
        const QString command_start;
        const QString command_end;
        EnclosedTypesetAction(const QString& text,
                              const QString& tooltip,
                              const QString& command_start,
                              const QString& command_end,
                              MathToolbar* parent);
    };
};

#endif // MATHTOOLBAR_H
