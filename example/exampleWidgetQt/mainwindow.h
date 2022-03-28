#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MathToolbar;
class Preferences;
class QGroupBox;

namespace Hope{ namespace Typeset { class View; } }

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    QSettings settings;
    Ui::MainWindow* ui;
    Hope::Typeset::View* editor;
    Hope::Typeset::View* console;
    QGroupBox* group_box;
    MathToolbar* math_toolbar;
    QToolBar* action_toolbar;
    Preferences* preferences;
    QString path;
    QTimer interpreter_poll_timer;
    static constexpr std::chrono::milliseconds INTERPETER_POLL_PERIOD = std::chrono::milliseconds(15);
    std::string print_buffer; //Need an extra layer of buffering so we don't try to parse incomplete constructs
    bool editor_had_focus;
    bool unsaved_changes = false;
    bool isSavedDeepComparison() const;

private slots:
    void run();
    void stop();
    void pollInterpreterThread();
    void parseTree();
    void symbolTable();
    void github();
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    bool on_actionSave_triggered();
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
    void onTextChanged();
    void on_actionShow_action_toolbar_toggled(bool show);
    void on_actionShow_typesetting_toolbar_toggled(bool show);
    void checkForChanges();
    void on_actionSee_log_triggered();
    void on_actionPreferences_triggered();
    void onColourChanged();
    void on_actionGo_to_line_triggered();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    bool savePrompt();
    bool saveAs(QString name);
    void open(QString path);
    QString getLastDir();
};
#endif // MAINWINDOW_H
