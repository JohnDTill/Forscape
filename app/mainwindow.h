#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <forscape_common.h>
#include <filesystem>
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
class Splitter;

namespace Forscape{
class ProjectBrowser;

namespace Typeset {
class Console;
class Editor;
class Model;
}
}

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    virtual ~MainWindow();
    void openProject(QString path);
    QSettings settings;
    Forscape::Typeset::Editor* editor;

public slots:
    void viewModel(Forscape::Typeset::Model* model, size_t line);
    void viewModel(Forscape::Typeset::Model* model);
    void on_actionNew_triggered();
    void hideProjectBrowser() noexcept;

private:
    SearchDialog* search;
    Ui::MainWindow* ui;
    Forscape::Typeset::Console* console;
    QGroupBox* group_box;
    MathToolbar* math_toolbar;
    QToolBar* action_toolbar;
    QToolBar* project_toolbar;
    Forscape::ProjectBrowser* project_browser;
    Splitter* horizontal_splitter;
    Splitter* vertical_splitter;
    Preferences* preferences;
    Plot* active_plot = nullptr;
    QString project_path;
    QString active_file_path;
    QTimer interpreter_poll_timer;
    QTimer shutdown_timer;
    static constexpr std::chrono::milliseconds INTERPETER_POLL_PERIOD = std::chrono::milliseconds(15);
    bool editor_had_focus;
    QAction* file_back;
    QAction* file_next;
    void loadGeometry();
    void resizeHackToFixScrollbars();
    void updateViewJumpPointElements();
    void resetViewJumpPointElements();

    struct JumpPoint {
        Forscape::Typeset::Model* model;
        size_t line;

        JumpPoint() noexcept = default;
        JumpPoint(Forscape::Typeset::Model* model, size_t line) noexcept :
            model(model), line(line){}
    };
    std::vector<JumpPoint> viewing_chain;
    size_t viewing_index = 0;

private slots:
    void run();
    void stop();
    void pollInterpreterThread();
    void parseTree();
    void symbolTable();
    void github();
    void on_actionNew_Project_triggered();
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
    void insertSettings();
    void on_actionStop_triggered();
    void on_actionPNG_triggered();
    void on_actionSVG_Image_triggered();
    void on_actionTeX_triggered();
    void on_actionUnicode_triggered();
    void onTextChanged();
    void onModelChanged(Forscape::Typeset::Model* model);
    void on_actionShow_action_toolbar_toggled(bool show);
    void on_actionShow_typesetting_toolbar_toggled(bool show);
    void on_actionShow_project_browser_toggled(bool show);
    void checkForChanges();
    void on_actionPreferences_triggered();
    void onColourChanged();
    void on_actionGo_to_line_triggered();
    void onSplitterResize(int pos, int index);
    void setHSplitterDefaultWidth();
    void setVSplitterDefaultHeight();
    void on_actionGoBack_triggered();
    void on_actionGoForward_triggered();
    void viewSelection(const Forscape::Typeset::Selection& sel);
    bool on_actionSave_All_triggered();
    void on_actionReload_triggered();
    void onForcedExit();
    void updateDuringForcedExit();
    void on_actionClearRecentProjects_triggered();
    void openRecent(QAction* action);
    void on_actionGo_to_main_file_triggered();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    bool promptForUnsavedChanges(const QString& action);
    void checkOutput();
    bool savePrompt();
    bool saveAs(QString name);
    bool saveAs(QString path, Forscape::Typeset::Model* model);
    void addPlot(const std::string& title, const std::string& x_label, const std::string& y_label);
    void addSeries(const std::vector<std::pair<double, double>>& data) const alloc_except;
    QString getLastDir();
    void setEditorToModelAndLine(Forscape::Typeset::Model* model, size_t line);
    void loadRecentProjects();
    void updateRecentProjectsFromList();
    void updateRecentProjectsFromCurrent();

    FORSCAPE_UNORDERED_SET<Forscape::Typeset::Model*> modified_files;
    QStringList recent_projects;
    static constexpr int MAX_DISPLAYED_RECENT_PROJECTS = 20;
    static constexpr int MAX_STORED_RECENT_PROJECTS = 50;
};
#endif // MAINWINDOW_H
