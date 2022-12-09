#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MathToolbar;
class Plot;
class Preferences;
class SearchDialog;
class QGroupBox;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class Splitter;

namespace Forscape{
namespace Typeset {
class Console;
class Editor;
}
}

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    virtual ~MainWindow();
    QSettings settings;
    Forscape::Typeset::Editor* editor;

private:
    SearchDialog* search;
    Ui::MainWindow* ui;
    Forscape::Typeset::Console* console;
    QGroupBox* group_box;
    MathToolbar* math_toolbar;
    QToolBar* action_toolbar;
    QToolBar* project_toolbar;
    QTreeWidget* project_browser;
    Splitter* horizontal_splitter;
    Splitter* vertical_splitter;
    Preferences* preferences;
    Plot* active_plot = nullptr;
    QString path;
    QTimer interpreter_poll_timer;
    static constexpr std::chrono::milliseconds INTERPETER_POLL_PERIOD = std::chrono::milliseconds(15);
    bool editor_had_focus;
    void loadGeometry();
    void resizeHackToFixScrollbars();
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
    void on_actionShow_project_browser_toggled(bool show);
    void checkForChanges();
    void on_actionPreferences_triggered();
    void onColourChanged();
    void on_actionGo_to_line_triggered();
    void onSplitterResize(int pos, int index);
    void anchor();
    void onFileClicked(QTreeWidgetItem* item, int column);
    void onFileRightClicked(const QPoint& pos);
    void setHSplitterDefaultWidth();
    void setVSplitterDefaultHeight();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    void checkOutput();
    bool savePrompt();
    bool saveAs(QString name);
    void open(QString path);
    void addPlot(const std::string& title, const std::string& x_label, const std::string& y_label);
    void addSeries(const std::vector<std::pair<double, double>>& data) const alloc_except;
    QString getLastDir();
};
#endif // MAINWINDOW_H
