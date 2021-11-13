#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

namespace Hope{ namespace Typeset { class View; } }

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow* ui;
    Hope::Typeset::View* editor;
    Hope::Typeset::View* console;
    QString path;

private slots:
    void run();
    void stop();
    void parseTree();
    void symbolTable();
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSave_As_triggered();
    void on_actionExit_triggered();
    void on_actionFind_Replace_triggered();
    void on_actionUndo_triggered();
    void on_actionRedo_triggered();
    void on_actionCut_triggered();
    void on_actionCopy_triggered();
    void on_actionPaste_triggered();
    void on_actionDelete_triggered();
    void on_actionSelect_all_triggered();
    void on_actionRun_triggered();
    void on_actionZoom_in_triggered();
    void on_actionZoom_out_triggered();
    void on_actionReset_zoom_triggered();
    void on_actionShow_line_numbers_toggled(bool checked);
    void insertFlatText(const QString& str);
    void insertSerial(const QString& str);
    void insertSerialSelection(const QString& A, const QString& B);
    void on_actionStop_triggered();
    void on_actionPNG_triggered();
    void on_actionSVG_Image_triggered();
    void on_actionTeX_triggered();
    void on_actionUnicode_triggered();

private:
    void savePrompt();
    void saveAs(QString name);
};
#endif // MAINWINDOW_H
