#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <forscape_message.h>
#include <forscape_scanner.h>
#include <forscape_serial.h>
#include <forscape_serial_unicode.h>
#include <forscape_parser.h>
#include <forscape_program.h>
#include <forscape_symbol_lexical_pass.h>
#include <typeset_line.h>
#include <typeset_model.h>
#include <typeset_painter.h>
#include <typeset_settings.h>
#include <typeset_view.h>
#include <qt_compatability.h>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextStream>
#include <QToolBar>
#include <QVariant>
#include <QVBoxLayout>

#include "mathtoolbar.h"
#include "plot.h"
#include "preferences.h"
#include "projectbrowser.h"
#include "searchdialog.h"
#include "splitter.h"
#include "symboltreeview.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef NDEBUG
#include "qgraphvizcall.h"
#endif

#ifdef linux
#define FORSCAPE_WORKAROUND_QT_LINUX_FILETYPE_FILTER_BUG
#endif

#ifdef FORSCAPE_WORKAROUND_QT_LINUX_FILETYPE_FILTER_BUG
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#define FORSCAPE_FILE_TYPE_DESC "Forscape script (*)"
#else
#define FORSCAPE_FILE_TYPE_DESC "Forscape script (*.π)"
#endif

#define PROJECT_ROOT_FILE "project_root_file"
#define ZOOM_EDITOR "editor_zoom"
#define ZOOM_CONSOLE "console_zoom"
#define LINE_NUMBERS_VISIBLE "line_nums_shown"
#define WINDOW_TITLE_SUFFIX " - Forscape"
#define NEW_SCRIPT_TITLE "new script"
#define MATH_TOOLBAR_VISIBLE "math_tb_visible"
#define ACTION_TOOLBAR_VISIBLE "action_tb_visible"
#define PROJECT_BROWSER_VISIBLE "project_tb_visible"
#define WINDOW_STATE "win_state"
#define LAST_DIRECTORY "last_dir"
#define RECENT_PROJECTS "recent"

using namespace Forscape;

static QTimer* external_change_timer;

static constexpr int CHANGE_CHECK_PERIOD_MS = 500;

static constexpr int FILE_BROWSER_WIDTH = 200;
static bool program_control_of_hsplitter = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow){
    ui->setupUi(this);
    Typeset::Painter::init();
    #ifdef FORSCAPE_LOAD_WITHOUT_CACHE_DEBUG
    settings.clear(); // To check a fresh install boots; e.g. loading does not have a cache dependency
    #endif

    external_change_timer = new QTimer(this);
    connect(external_change_timer, &QTimer::timeout, this, &MainWindow::checkForChanges);
    external_change_timer->start(CHANGE_CHECK_PERIOD_MS);

    horizontal_splitter = new Splitter(Qt::Horizontal, this);
    project_browser = new ProjectBrowser(horizontal_splitter, this);

    ui->actionGoBack->setIcon(QIcon(":/fonts/arrow_back.svg"));
    ui->actionGoForward->setIcon(QIcon(":/fonts/arrow_forward.svg"));
    horizontal_splitter->addWidget(project_browser);
    connect(horizontal_splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(onSplitterResize(int, int)));
    connect(horizontal_splitter, SIGNAL(splitterDoubleClicked(int)), this, SLOT(setHSplitterDefaultWidth()));

    vertical_splitter = new Splitter(Qt::Vertical, horizontal_splitter);
    horizontal_splitter->addWidget(vertical_splitter);
    setCentralWidget(horizontal_splitter);

    editor = new Typeset::Editor();
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    vertical_splitter->addWidget(editor);

    group_box = new QGroupBox(this);
    group_box->setTitle("Console");

    QVBoxLayout* vbox = new QVBoxLayout();
    group_box->setLayout(vbox);
    group_box->resize(width(), height()*0.7);
    vertical_splitter->addWidget(group_box);
    vertical_splitter->setStretchFactor(0, 2);
    vertical_splitter->setStretchFactor(1, 1);
    connect(vertical_splitter, SIGNAL(splitterDoubleClicked(int)), this, SLOT(setVSplitterDefaultHeight()));

    console = new Typeset::Console();
    console->setMinimumHeight(40);
    vbox->addWidget(console);

    editor->console = console;

    QGuiApplication::setWindowIcon(QIcon(":/lambda.ico"));

    if(settings.contains(ZOOM_CONSOLE)){
        bool success;
        double old_zoom = settings.value(ZOOM_CONSOLE).toDouble(&success);
        if(success) console->zoom = old_zoom;
        //EVENTUALLY: log error if this fails
    }

    if(settings.contains(ZOOM_EDITOR)){
        bool success;
        double old_zoom = settings.value(ZOOM_EDITOR).toDouble(&success);
        if(success) editor->zoom = old_zoom;
    }

    if(settings.contains(LINE_NUMBERS_VISIBLE)){
        bool line_numbers_visible = settings.value(LINE_NUMBERS_VISIBLE).toBool();
        if(!line_numbers_visible) ui->actionShow_line_numbers->setChecked(false);
    }

    editor->setFocus();

    int id = QFontDatabase::addApplicationFont(":/fonts/toolbar_glyphs.otf");
    assert(id!=-1);
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont glyph_font = QFont(family);
    glyph_font.setPointSize(18);

    action_toolbar = addToolBar(tr("Action Toolbar"));
    action_toolbar->setObjectName("action_toolbar");
    QAction* run_act = new QAction("Œ", this);
    run_act->setToolTip(tr("Run script   Ctrl+R"));
    run_act->setFont(glyph_font);
    run_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(run_act, &QAction::triggered, this, &MainWindow::run);
    action_toolbar->addAction(run_act);

    QAction* stop_act = new QAction("Ŗ", this);
    stop_act->setToolTip(tr("Stop script"));
    stop_act->setFont(glyph_font);
    stop_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(stop_act, &QAction::triggered, this, &MainWindow::stop);
    action_toolbar->addAction(stop_act);

    #ifndef NDEBUG
    QAction* ast_act = new QAction("œ", this);
    ast_act->setToolTip(tr("Show AST"));
    ast_act->setFont(glyph_font);
    ast_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(ast_act, &QAction::triggered, this, &MainWindow::parseTree);
    action_toolbar->addAction(ast_act);

    QAction* sym_act = new QAction("Ŕ", this);
    sym_act->setToolTip(tr("Show symbol table"));
    sym_act->setFont(glyph_font);
    sym_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(sym_act, &QAction::triggered, this, &MainWindow::symbolTable);
    action_toolbar->addAction(sym_act);
    #endif

    QAction* github_act = new QAction("ŕ", this);
    github_act->setToolTip(tr("View on GitHub"));
    github_act->setFont(glyph_font);
    github_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(github_act, &QAction::triggered, this, &MainWindow::github);
    action_toolbar->addAction(github_act);

    action_toolbar->addSeparator();

    file_back = new QAction("Ř", this);
    file_back->setToolTip(tr("View the previous file"));
    file_back->setFont(glyph_font);
    file_back->setShortcuts(QKeySequence::InsertLineSeparator);
    file_back->setDisabled(true);
    connect(file_back, &QAction::triggered, this, &MainWindow::on_actionGoBack_triggered);
    action_toolbar->addAction(file_back);

    file_next = new QAction("ř", this);
    file_next->setToolTip(tr("View the next file"));
    file_next->setFont(glyph_font);
    file_next->setShortcuts(QKeySequence::InsertLineSeparator);
    file_next->setDisabled(true);
    connect(file_next, &QAction::triggered, this, &MainWindow::on_actionGoForward_triggered);
    action_toolbar->addAction(file_next);

    connect(editor, SIGNAL(goToModel(Forscape::Typeset::Model*,size_t)),
            this, SLOT(viewModel(Forscape::Typeset::Model*,size_t)));

    connect(editor, SIGNAL(goToSelection(Forscape::Typeset::Selection)),
            this, SLOT(viewSelection(Forscape::Typeset::Selection)));

    if(settings.contains(ACTION_TOOLBAR_VISIBLE)){
        bool visible = settings.value(ACTION_TOOLBAR_VISIBLE).toBool();
        if(!visible){
            ui->actionShow_action_toolbar->setChecked(false);
            action_toolbar->hide();
        }
    }

    math_toolbar = new MathToolbar(this);
    math_toolbar->setObjectName("math_toolbar");
    connect(math_toolbar, SIGNAL(insertFlatText(QString)), this, SLOT(insertFlatText(const QString&)));
    connect(math_toolbar, SIGNAL(insertSerial(QString)), this, SLOT(insertSerial(const QString&)));
    connect(math_toolbar, SIGNAL(insertSerialSelection(QString, QString)),
            this, SLOT(insertSerialSelection(QString, QString)));
    connect(math_toolbar, SIGNAL(insertSettings()), this, SLOT(insertSettings()));
    addToolBarBreak(Qt::ToolBarArea::TopToolBarArea);
    addToolBar(Qt::ToolBarArea::TopToolBarArea, math_toolbar);

    if(settings.contains(MATH_TOOLBAR_VISIBLE)){
        bool visible = settings.value(MATH_TOOLBAR_VISIBLE).toBool();
        if(!visible){
            ui->actionShow_typesetting_toolbar->setChecked(false);
            math_toolbar->hide();
        }
    }

    #ifndef NDEBUG
    auto action = ui->menuCode->addAction("Parse Tree");
    connect(action, SIGNAL(triggered(bool)), this, SLOT(parseTree()));
    action = ui->menuCode->addAction("Symbol Table");
    connect(action, SIGNAL(triggered(bool)), this, SLOT(symbolTable()));
    #endif

    //Don't apply these regardless of focus
    ui->actionSelect_all->setShortcutContext(Qt::WidgetShortcut);
    ui->actionCopy->setShortcutContext(Qt::WidgetShortcut);
    ui->actionCut->setShortcutContext(Qt::WidgetShortcut);
    ui->actionPaste->setShortcutContext(Qt::WidgetShortcut);
    ui->actionDelete->setShortcutContext(Qt::WidgetShortcut);

    connect(&interpreter_poll_timer, SIGNAL(timeout()), this, SLOT(pollInterpreterThread()));
    connect(editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    connect(editor, SIGNAL(modelChanged(Forscape::Typeset::Model*)),
            this, SLOT(onModelChanged(Forscape::Typeset::Model*)));

    preferences = new Preferences(settings);
    connect(preferences, SIGNAL(colourChanged()), this, SLOT(onColourChanged()));
    connect(editor, SIGNAL(integralPreferenceChanged(bool)), preferences, SLOT(on_integralCheckBox_toggled(bool)));
    connect(editor, SIGNAL(setCommasInLargeNumbers(bool)), preferences, SLOT(on_numberCommaCheckBox_toggled(bool)));
    onColourChanged();

    search = new SearchDialog(this, editor, console, settings);

    loadGeometry();

    horizontal_splitter->setStretchFactor(0, 0);
    horizontal_splitter->setStretchFactor(1, 1);

    if(settings.contains(PROJECT_BROWSER_VISIBLE) && !settings.value(PROJECT_BROWSER_VISIBLE).toBool()){
        ui->actionShow_project_browser->setChecked(false);
        on_actionShow_project_browser_toggled(false);
    }else{
        on_actionShow_project_browser_toggled(true);
    }

    connect(QGuiApplication::instance(), SIGNAL(commitDataRequest(QSessionManager&)), this, SLOT(onForcedExit()));
    connect(&shutdown_timer, &QTimer::timeout, this, &MainWindow::updateDuringForcedExit);

    loadRecentProjects();

    if(settings.contains(PROJECT_ROOT_FILE)){
        openProject(settings.value(PROJECT_ROOT_FILE).toString());
    }else{
        on_actionNew_triggered();
    }
    resetViewJumpPointElements();

    QAction* new_keyboard_shortcut = new QAction(tr("Create new..."), this);
    new_keyboard_shortcut->setShortcut(QKeySequence::New);
    connect(new_keyboard_shortcut, SIGNAL(triggered()), this, SLOT(onKeyboardNew()));
    addAction(new_keyboard_shortcut);

    // By default there are options to hide the toolbars, but they don't play nicely with custom show/hide options
    setContextMenuPolicy(Qt::NoContextMenu);
}

MainWindow::~MainWindow(){
    settings.setValue(ZOOM_CONSOLE, console->zoom);
    settings.setValue(ZOOM_EDITOR, editor->zoom);
    settings.setValue(LINE_NUMBERS_VISIBLE, editor->lineNumbersShown());
    settings.setValue(MATH_TOOLBAR_VISIBLE, ui->actionShow_typesetting_toolbar->isChecked());
    settings.setValue(ACTION_TOOLBAR_VISIBLE, ui->actionShow_action_toolbar->isChecked());
    settings.setValue(PROJECT_BROWSER_VISIBLE, ui->actionShow_project_browser->isChecked());
    settings.setValue(WINDOW_STATE, QList({QVariant(saveGeometry()), QVariant(saveState())}));
    settings.setValue(RECENT_PROJECTS, recent_projects);
    search->saveSettings(settings);
    delete preferences;
    delete ui;

    Forscape::Program::instance()->freeFileMemory();

    #ifdef TYPESET_MEMORY_DEBUG
    assert(Typeset::Model::all.empty());
    //assert(Typeset::Text::all.empty()); //EVENTUALLY: really tighten down on this
    //assert(Typeset::Construct::all.empty());
    #endif
}

void MainWindow::loadGeometry(){
    if(settings.contains(WINDOW_STATE)){
        auto win_state = settings.value(WINDOW_STATE).toList();
        restoreGeometry(win_state[0].toByteArray());
        restoreState(win_state[1].toByteArray());
    }

    if(!isMaximized())
        QTimer::singleShot(1, this, &MainWindow::resizeHackToFixScrollbars);
}

void MainWindow::resizeHackToFixScrollbars(){
    resize(width()+1, height()+1);
    resize(width()-1, height()-1);

    setVSplitterDefaultHeight();
}

void MainWindow::updateViewJumpPointElements() {
    assert(!viewing_chain.empty());
    bool can_go_forward = viewing_index < viewing_chain.size()-1;
    bool can_go_backward = viewing_index > 0;
    ui->actionGoForward->setEnabled(can_go_forward);
    file_next->setEnabled(can_go_forward);
    ui->actionGoBack->setEnabled(can_go_backward);
    file_back->setEnabled(can_go_backward);

    active_file_path = toQString(editor->getModel()->path);
    onTextChanged();
}

void MainWindow::resetViewJumpPointElements() {
    viewing_chain.clear();
    viewing_chain.push_back(JumpPoint(editor->getModel(), 0));
    viewing_index = 0;

    ui->actionGoForward->setEnabled(false);
    file_next->setEnabled(false);
    ui->actionGoBack->setEnabled(false);
    file_back->setEnabled(false);
}

void MainWindow::run(){
    if(editor->isRunning()) return;

    editor->runThread();
    if(Program::instance()->noErrors()){
        interpreter_poll_timer.start(INTERPETER_POLL_PERIOD);

        editor_had_focus = false;
        if(editor->hasFocus()){
            editor_had_focus = true;

            //setEnabled is a tab event. take focus before it fires.
            setFocus(Qt::FocusReason::NoFocusReason);
        }

        editor->setReadOnly(true);
    }
}

void MainWindow::stop(){
    if(!editor->isRunning()) return;
    Program::instance()->stop();
}

void MainWindow::pollInterpreterThread(){
    auto& interpreter = Program::instance()->interpreter;

    if(interpreter.status == Forscape::Code::Interpreter::FINISHED){
        checkOutput();

        auto output = console->getModel();
        switch(interpreter.error_code){
            case Forscape::Code::NO_ERROR_FOUND: break;
            case Forscape::Code::USER_STOP:
                checkOutput();
                console->getModel()->appendSerialToOutput("\nScript terminated by user");
                console->updateModel();
                console->scrollToBottom();
                break;
            default:
                auto model = editor->getModel();
                Typeset::Selection c = Program::instance()->parse_tree.getSelection(interpreter.error_node);
                Program::instance()->error_stream.fail(c, interpreter.error_code);
                Code::Error::writeErrors(model->errors, output, editor);
                output->calculateSizes();
                output->updateLayout();
                console->updateModel();
                console->scrollToBottom();
        }

        editor->reenable();
        interpreter_poll_timer.stop();
        if(editor_had_focus) editor->setFocus();
    }else{
        checkOutput();
    }
}

void MainWindow::parseTree() {
    #ifndef NDEBUG
    QString dot_src = toQString(editor->getModel()->parseTreeDot());
    dot_src.replace("\\n", "\\\\n");
    QGraphvizCall::show(dot_src);
    #endif
}

void MainWindow::symbolTable() {
    #ifndef NDEBUG
    Typeset::Model* m = editor->getModel();
    SymbolTreeView* view = new SymbolTreeView(m->symbol_builder.symbol_table, Forscape::Program::instance()->static_pass);
    view->show();
    #endif
}

void MainWindow::github() {
    QDesktopServices::openUrl(QUrl("https://github.com/JohnDTill/Forscape"));
}

void MainWindow::onKeyboardNew() {
    QInputDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle("Creation Prompt");
    dialog.setLabelText(tr("Create new..."));
    dialog.setComboBoxItems({
        "Project",
        "File",
    });

    if(dialog.exec() != QDialog::Accepted) return;
    switch( dialog.findChild<QComboBox*>()->currentIndex() ){
        case 0: on_actionNew_Project_triggered(); break;
        case 1: on_actionNew_triggered(); break;
        default: assert(false);
    }
}

void MainWindow::on_actionNew_Project_triggered() {
    if(!editor->isEnabled()) return;

    if(promptForUnsavedChanges("changing project")) return;

    Forscape::Program::instance()->freeFileMemory();

    modified_files.clear();
    project_browser->clear();

    ui->actionSave->setEnabled(false);
    ui->actionSave_All->setEnabled(false);

    console->clearModel();

    resetViewJumpPointElements();
    Typeset::Model* model = Typeset::Model::fromSerial("");
    Program::instance()->program_entry_point = model;
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    active_file_path.clear();
    project_path.clear();

    project_browser->populateWithNewProject(model);
    Forscape::Program::instance()->setProgramEntryPoint("", model);
    model->performSemanticFormatting();
    editor->setModel(model);

    if(recent_projects.empty()) recent_projects.push_back(project_path);

    recent_projects.push_front(project_path);
    if(recent_projects.size() > MAX_STORED_RECENT_PROJECTS) recent_projects.pop_back();
    updateRecentProjectsFromList();

    //onTextChanged();
}

void MainWindow::on_actionNew_triggered(){
    if(!editor->isEnabled()) return;
    Typeset::Model* model = Typeset::Model::fromSerial("");
    if(Program::instance()->program_entry_point == nullptr)
        Program::instance()->program_entry_point = model;
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    active_file_path.clear();
    project_browser->addFile(model);

    viewModel(model, 0);
}

void MainWindow::hideProjectBrowser() noexcept {
    horizontal_splitter->setSizes({0, width()});
    ui->actionShow_project_browser->setChecked(false);
}

void MainWindow::reparse() {
    editor->getModel()->performSemanticFormatting();
    editor->updateModel();
    onTextChanged();
}

#ifdef FORSCAPE_WORKAROUND_QT_LINUX_FILETYPE_FILTER_BUG
class FileFilterProxyModel : public QSortFilterProxyModel {
protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
        QModelIndex index0 = sourceModel()->index(source_row, 0, source_parent);
        QFileSystemModel* fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
        QString filename = fileModel->fileName(index0);
        if(!fileModel->isDir(index0)) return filename.endsWith(".π");
        else return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }
};

class IconProvider : public QFileIconProvider {
    virtual QIcon icon(const QFileInfo& info) const override {
        if(info.fileName().endsWith(".π")) return QIcon(":/lambda.ico");
        else return QFileIconProvider::icon(info);
    }
};
#endif

void MainWindow::on_actionOpen_triggered(){
    if(!editor->isEnabled()) return;

    #ifdef FORSCAPE_WORKAROUND_QT_LINUX_FILETYPE_FILTER_BUG
    QFileDialog dialog(this);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setFileMode(QFileDialog::FileMode::ExistingFile);
    dialog.setNameFilter(tr(FORSCAPE_FILE_TYPE_DESC));
    dialog.setWindowTitle(tr("Open Project"));
    dialog.setDefaultSuffix("*.π");
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setDirectory(getLastDir());
    dialog.setProxyModel(new FileFilterProxyModel);
    dialog.setIconProvider(new IconProvider);
    if(!dialog.exec()) return;
    QStringList files = dialog.selectedFiles();
    QString path = files.front();
    #else
    QString path = QFileDialog::getOpenFileName(nullptr, tr("Open Project"), getLastDir(), tr(FORSCAPE_FILE_TYPE_DESC));
    if(path.isEmpty()) return;
    #endif

    openProject(path);
}

bool MainWindow::on_actionSave_triggered(){
    if(!editor->isEnabled()) return false;

    if(active_file_path.isEmpty()){
        return savePrompt();
    }else if(saveAs(active_file_path)){
        onTextChanged();
        return true;
    }else{
        assert(false);
        //EVENTUALLY - popup on failure?

        return false;
    }
}

void MainWindow::on_actionSave_As_triggered(){
    if(!editor->isEnabled()) return;

    savePrompt();
}

void MainWindow::on_actionExit_triggered(){
    close();
}

void MainWindow::checkOutput(){
    auto& interpreter = Program::instance()->interpreter;
    auto& message_queue = interpreter.message_queue;

    static std::string print_buffer;

    InterpreterOutput* msg;
    while(message_queue.try_dequeue(msg))
        switch(msg->getType()){
            case Forscape::InterpreterOutput::Print:
                print_buffer += static_cast<PrintMessage*>(msg)->msg;
                delete msg;
                break;
            case Forscape::InterpreterOutput::CreatePlot:{
                const PlotCreate& plt = *static_cast<PlotCreate*>(msg);
                addPlot(plt.title, plt.x_label, plt.y_label);
                delete msg;
                break;
            }
            case Forscape::InterpreterOutput::AddDiscreteSeries:{
                const auto& data = static_cast<PlotDiscreteSeries*>(msg)->data;
                addSeries(data);
                delete msg;
                break;
            }
            default: assert(false);
        }

    if(!print_buffer.empty()){
        bool at_bottom = console->scrolledToBottom();
        console->getModel()->appendSerialToOutput(print_buffer);
        console->updateModel();
        if(at_bottom) console->scrollToBottom();
        print_buffer.clear();
    }
}

bool MainWindow::savePrompt(){
    if(!editor->isEnabled()) return false;

    #ifdef FORSCAPE_WORKAROUND_QT_LINUX_FILETYPE_FILTER_BUG
    QFileDialog dialog(this);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::FileMode::AnyFile);
    dialog.setNameFilter(tr(FORSCAPE_FILE_TYPE_DESC));
    dialog.setWindowTitle(tr("Save File"));
    dialog.setDefaultSuffix("*.π");
    dialog.setViewMode(QFileDialog::Detail);
    dialog.selectFile(active_file_path.isEmpty() ? getLastDir() + "/untitled.π" : active_file_path);
    dialog.setProxyModel(new FileFilterProxyModel);
    dialog.setIconProvider(new IconProvider);
    if(!dialog.exec()) return false;
    QStringList files = dialog.selectedFiles();
    QString file_name = files.front();
    #else
    QString prompt_name = active_file_path.isEmpty() ? getLastDir() + "/untitled.π" : active_file_path;
    QString file_name = QFileDialog::getSaveFileName(nullptr, tr("Save File"), prompt_name, tr(FORSCAPE_FILE_TYPE_DESC));
    #endif

    if(file_name.isEmpty()) return false;

    bool anchor_file = active_file_path == project_path;
    bool success = saveAs(file_name);
    if(success && anchor_file){
        project_path = file_name;
        updateRecentProjectsFromCurrent();
        settings.setValue(PROJECT_ROOT_FILE, project_path);
    }
    return success;
}

bool MainWindow::saveAs(QString path){
    return saveAs(path, editor->getModel());
}

bool MainWindow::saveAs(QString path, Forscape::Typeset::Model* saved_model) {
    if(!editor->isEnabled()) return false;

    QFile file(path);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Could not open \"" + path + "\" to write.");
        messageBox.setFixedSize(500,200);
        return false;
    }

    file.setTextModeEnabled(false);
    QTextStream out(&file);
    #ifdef QT5
    out.setCodec("UTF-8");
    #endif
    out << QByteArray::fromStdString(saved_model->toSerial());

    setWindowTitle(file.fileName() + WINDOW_TITLE_SUFFIX);
    if(project_path.isEmpty()){
        settings.setValue(PROJECT_ROOT_FILE, path);
        project_path = path;
    }
    active_file_path = path;
    out.flush();
    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());

    //Re-jig the project browser
    std::filesystem::path std_path = std::filesystem::canonical(toCppPath(active_file_path));
    project_browser->saveModel(saved_model, std_path);

    saved_model->write_time = std::filesystem::file_time_type::clock::now();

    updateRecentProjectsFromCurrent();

    onTextChanged();

    return true;
}

void MainWindow::openProject(QString path){
    if(promptForUnsavedChanges("changing project")) return;

    std::filesystem::path std_path = toCppPath(path);
    std::ifstream in(std_path);
    if(!in.is_open()){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Could not open \"" + path + "\" to read.");
        messageBox.setFixedSize(500,200);
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    if(isIllFormedUtf8(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + path + "\" is not UTF-8 encoded.");
        messageBox.setFixedSize(500,200);
        return;
    }

    if(!Forscape::isValidSerial(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + path + "\" does not conform to Forscape encoding.\n"
            "This is caused by corruption, or erroneous typesetting specifiers.");
        messageBox.setFixedSize(500,200);
        return;
    }

    Typeset::Model* model = Typeset::Model::fromSerial(src);
    std_path = std::filesystem::canonical(std_path);
    model->path = std_path;
    Forscape::Program::instance()->freeFileMemory();
    Forscape::Program::instance()->setProgramEntryPoint(std_path, model);
    model->performSemanticFormatting();
    editor->setModel(model);

    modified_files.clear();
    project_browser->setProject(model);

    setWindowTitle(QFile(path).fileName() + WINDOW_TITLE_SUFFIX);
    settings.setValue(PROJECT_ROOT_FILE, path);
    project_path = path;
    active_file_path = path;
    model->write_time = std::filesystem::file_time_type::clock::now();

    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());

    resetViewJumpPointElements();

    ui->actionSave->setEnabled(false);
    ui->actionSave_All->setEnabled(false);

    updateRecentProjectsFromCurrent();
    console->clearModel();

    onTextChanged();
}

void MainWindow::removeFile(Forscape::Typeset::Model* model) noexcept {
    Forscape::Program::instance()->removeFile(model);

    auto removed = std::remove_if(viewing_chain.begin(), viewing_chain.end(),
        [model](const JumpPoint& v) noexcept { return v.model == model; });

    while(removed != viewing_chain.end()){
        viewing_chain.erase(removed);

        for(size_t i = viewing_chain.size(); i-->1;)
            if(viewing_chain[i] == viewing_chain[i-1])
                viewing_chain[i].model = nullptr;

        removed = std::remove_if(viewing_chain.begin(), viewing_chain.end(),
            [](const JumpPoint& v) noexcept { return v.model == nullptr; });
    }

    updateViewJumpPointElements();
}

void MainWindow::addPlot(const std::string& title, const std::string& x_label, const std::string& y_label){
    active_plot = new Plot(title, x_label, y_label);
    active_plot->show();
    connect(this, SIGNAL(destroyed()), active_plot, SLOT(deleteLater()));
}

void MainWindow::addSeries(const std::vector<std::pair<double, double>>& data) const alloc_except {
    assert(active_plot);
    active_plot->addSeries(data);
    active_plot->update();
}

QString MainWindow::getLastDir(){
    if(settings.contains(LAST_DIRECTORY)){
        QString last_dir = settings.value(LAST_DIRECTORY).toString();
        if(std::filesystem::is_directory(toCppString(last_dir))) return last_dir;
    }

    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::setEditorToModelAndLine(Forscape::Typeset::Model* model, size_t line){
    editor->setModel(model);
    editor->goToLine(line);
    project_browser->setCurrentlyViewed(model);

    if(search->isVisible()) search->updateSelection();
}

void MainWindow::on_actionFind_Replace_triggered(){
    if(!editor->isEnabled()) return;

    search->updateSelection();
    Typeset::Controller& c = editor->getController();
    std::string simple_word = c.isTextSelection() ? c.selectedText() : "";
    search->setWord(toQString(simple_word));
    search->show();
}

void MainWindow::on_actionUndo_triggered(){
    if(!editor->isEnabled()) return;

    editor->undo();
    editor->updateModel();
}

void MainWindow::on_actionRedo_triggered(){
    if(!editor->isEnabled()) return;

    editor->redo();
    editor->updateModel();
}

void MainWindow::on_actionCut_triggered(){
    if(!editor->isEnabled()) return;

    editor->cut();
    editor->updateModel();
}

void MainWindow::on_actionCopy_triggered(){
    if(!editor->isEnabled()) return;

    editor->copy();
    editor->updateModel();
}

void MainWindow::on_actionPaste_triggered(){
    if(!editor->isEnabled()) return;

    editor->paste();
    editor->updateModel();
}

void MainWindow::on_actionDelete_triggered(){
    if(!editor->isEnabled()) return;

    editor->del();
    editor->updateModel();
}

void MainWindow::on_actionSelect_all_triggered(){
    if(!editor->isEnabled()) return;

    editor->selectAll();
    editor->updateModel();
}

void MainWindow::on_actionRun_triggered(){
    run();
}

void MainWindow::on_actionZoom_in_triggered(){
    editor->zoomIn();
    console->zoomIn();
    editor->update();
    console->update();
}

void MainWindow::on_actionZoom_out_triggered(){
    editor->zoomOut();
    console->zoomOut();
    editor->update();
    console->update();
}

void MainWindow::on_actionReset_zoom_triggered(){
    editor->resetZoom();
    console->resetZoom();
    editor->update();
    console->update();
}

void MainWindow::on_actionShow_line_numbers_toggled(bool checked){
    editor->setLineNumbersVisible(checked);
}

void MainWindow::insertFlatText(const QString& str){
    if(!editor->isEnabled()) return;

    editor->insertText(toCppString(str));
    editor->update();

    onTextChanged();
}

void MainWindow::insertSerial(const QString& str){
    if(!editor->isEnabled()) return;

    editor->insertSerial(toCppString(str));
    editor->update();

    onTextChanged();
}

void MainWindow::insertSerialSelection(const QString& A, const QString& B){
    if(!editor->isEnabled()) return;

    Typeset::Controller& c = editor->getController();
    editor->insertSerial(toCppString(A) + c.selectedText() + toCppString(B));
    editor->update();

    onTextChanged();
}

void MainWindow::insertSettings() {
    insertSerial(OPEN_STR SETTINGS_STR "");
    Typeset::Construct* settings = editor->getController().getActive().text->prevConstructAsserted();
    Typeset::Settings::changeSettings(settings, editor->getController());
    Typeset::Settings::expandCollapse(settings, editor->getController());
    editor->update();

    onTextChanged();
}

void MainWindow::on_actionStop_triggered(){
    stop();
}

void MainWindow::on_actionPNG_triggered(){
    Typeset::Controller c = editor->getController();
    if(!c.hasSelection()) c.selectAll();

    QImage img = c.selection().toPng();
    QGuiApplication::clipboard()->setImage(img, QClipboard::Clipboard);
}

void MainWindow::on_actionSVG_Image_triggered(){
    Typeset::Controller c = editor->getController();
    if(!c.hasSelection()) c.selectAll();

    QBuffer b;
    QSvgGenerator generator;
    static constexpr double QSVG_SCALING_CORRECTION_FACTOR = 1.25;
    generator.setResolution(QSVG_SCALING_CORRECTION_FACTOR*generator.resolution());
    generator.setDescription(toQString(c.selectedText()));
    generator.setTitle("ForscapeSvg");
    generator.setOutputDevice(&b);
    c.selection().toSvg(generator);
    QMimeData* d = new QMimeData();
    d->setData("image/svg+xml", b.buffer());
    QApplication::clipboard()->setMimeData(d, QClipboard::Clipboard);
}

void MainWindow::on_actionTeX_triggered(){
    std::cout << "Implement this (MainWindow, Line " << __LINE__ << ")" << std::endl;
}

void MainWindow::on_actionUnicode_triggered(){
    std::string str = editor->getController().selectedText();
    if(UnicodeConverter::canConvert(str)){
        std::string uni = UnicodeConverter::convert(str);
        QApplication::clipboard()->setText(toQString(uni));
    }else{
        QMessageBox messageBox;
        messageBox.warning(nullptr, "Warning", "Selected text cannot be converted to unicode.");
        messageBox.setFixedSize(500, 200);
    }
}

void MainWindow::onTextChanged(){
    //EVENTUALLY: doing a deep comparison is terrible. Need a more efficient way to determine if the document is saved
    auto model = editor->getModel();
    const bool changed_from_save = !model->isSavedDeepComparison();
    const bool never_saved = active_file_path.isEmpty();
    const bool saveable = changed_from_save || never_saved;
    setWindowTitle(
        (changed_from_save ? "*" : "") +
        (active_file_path.isEmpty() ? NEW_SCRIPT_TITLE : QFile(active_file_path).fileName()) +
        WINDOW_TITLE_SUFFIX);

    ui->actionSave->setEnabled(saveable);

    project_browser->updateProjectBrowser();

    if(changed_from_save) modified_files.insert(model);
    else modified_files.erase(model);
    ui->actionSave_All->setDisabled(modified_files.empty());

    ui->actionReload->setDisabled(never_saved);

    //A selection search region is invalidated by an edit
    if(search->isVisible()){
        search->updateSelection();
        editor->updateAfterHighlightChange(); //EVENTUALLY: this is poor scheduling
    }
}

void MainWindow::onModelChanged(Forscape::Typeset::Model* model) {
    const bool changed_from_save = !model->isSavedDeepComparison();
    if(changed_from_save) modified_files.insert(model);
    else modified_files.erase(model);
    ui->actionSave_All->setDisabled(modified_files.empty());
}

int seconds_to_shutdown = -1;

void MainWindow::closeEvent(QCloseEvent* event){
    while(seconds_to_shutdown != -1) QGuiApplication::processEvents();

    if(promptForUnsavedChanges("closing")){
        event->ignore();
    }else{
        preferences->close();
        QMainWindow::closeEvent(event);
        emit destroyed();
    }
}

bool MainWindow::promptForUnsavedChanges(const QString& action) {
    if(modified_files.empty()) return false;

    const bool multiple_files = (modified_files.size() > 1);

    size_t num_files_not_on_disk = 0;

    QList<QString> files;
    for(Forscape::Typeset::Model* model : modified_files){
        QString qpath = model->path.empty() ? "untitled" : toQString(model->path.filename());
        files.push_back(qpath);
        num_files_not_on_disk += model->notOnDisk();
    }
    files.sort();

    static constexpr int NUM_PRINT = 7;

    QString msg = files.front();
    for(int i = 1; i < std::min<int>(files.size(), NUM_PRINT); i++){
        msg += '\n';
        msg += files[i];
    }
    if(files.size() > NUM_PRINT)
        msg += "\n... and " + QString::number(files.size()-NUM_PRINT) + " more";

    QMessageBox msg_box;

    QString prompt;
    if(num_files_not_on_disk == 1){
        viewModel(*modified_files.begin(), 0);
        prompt = "Save new file before " + action + "?";
    }else if(num_files_not_on_disk > 1){
        prompt = "Discard multiple new files?";
    }else{
        prompt = "Save file";
        if(multiple_files) prompt += 's';
        prompt += " before " + action + "?";
        msg_box.setInformativeText(msg);
    }

    msg_box.setWindowTitle("Unsaved changes");
    msg_box.setText(prompt);
    auto save_btn = num_files_not_on_disk ?
                        (multiple_files ? QMessageBox::StandardButton::NoButton : QMessageBox::StandardButton::Save) :
                        (multiple_files ? QMessageBox::SaveAll : QMessageBox::Save);
    msg_box.setStandardButtons(save_btn | QMessageBox::Discard | QMessageBox::Cancel);
    msg_box.setDefaultButton(save_btn);
    msg_box.setEscapeButton(QMessageBox::Cancel);
    msg_box.setIcon(QMessageBox::Icon::Question);

    switch (msg_box.exec()) {
        case QMessageBox::Save:
            if(num_files_not_on_disk) return !savePrompt();
            else return !on_actionSave_All_triggered();
        case QMessageBox::SaveAll:
            return !on_actionSave_All_triggered();
        case QMessageBox::Discard: return false;
        case QMessageBox::Cancel: return true;
    }

    assert(false);
}

void MainWindow::on_actionShow_action_toolbar_toggled(bool show){
    action_toolbar->setVisible(show);
}

void MainWindow::on_actionShow_typesetting_toolbar_toggled(bool show){
    math_toolbar->setVisible(show);
}

void MainWindow::on_actionShow_project_browser_toggled(bool show){
    if(program_control_of_hsplitter) return;
    else if(show) setHSplitterDefaultWidth();
    else hideProjectBrowser();
}

void MainWindow::checkForChanges(){
    if(active_file_path.isEmpty()) return;

    Forscape::Typeset::Model* model = editor->getModel();
    //last_write_time takes on the order of ~10us, so probably okay to run in main GUI thread every ~0.5s
    const std::filesystem::file_time_type modified_time = std::filesystem::last_write_time(model->path);
    if(modified_time <= model->write_time) return;
    model->write_time = modified_time;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "File modified externally",
        "This file has been modified externally.\nWould you like to reload it?",
        QMessageBox::Yes|QMessageBox::No
    );

    if(reply == QMessageBox::Yes) on_actionReload_triggered();
    else onTextChanged();
}

void MainWindow::on_actionPreferences_triggered(){
    preferences->show();
    preferences->raise();  // for MacOS
    preferences->activateWindow(); // for Windows
}

void MainWindow::onColourChanged(){
    QPalette p = QGuiApplication::palette();
    p.setColor(QPalette::Text, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::WindowText, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::PlaceholderText, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::Dark, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::Highlight, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_SELECTION));
    p.setColor(QPalette::HighlightedText, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_SELECTEDTEXT));
    p.setColor(QPalette::Window, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    p.setColor(QPalette::Button, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    p.setColor(QPalette::Base, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_BACKGROUND));
    p.setColor(QPalette::Light, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LIGHT));
    p.setColor(QPalette::Link, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINK));
    p.setColor(QPalette::LinkVisited, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINK));
    setPalette(p);
    ui->menubar->setPalette(QGuiApplication::palette()); //Reset the menubar palette

    //Set colours which should not affect filebar
    p.setColor(QPalette::ButtonText, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_TEXT));
    //p.setColor(QPalette::Mid, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::Midlight, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::Shadow, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::AlternateBase, Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_LINEBOXFILL));
    group_box->setPalette(p);
    action_toolbar->setPalette(p);
    math_toolbar->setPalette(p);
    //preferences->setPalette(p); //EVENTUALLY: get themes to work with popup windows

    editor->updateBackgroundColour();
    console->updateBackgroundColour();

    //EVENTUALLY: a bit hacky, but auto drawing the background from QPalette is much faster than manual
    for(Forscape::Typeset::View* view : Forscape::Typeset::View::all_views)
        view->updateBackgroundColour();
}

void MainWindow::on_actionGo_to_line_triggered(){
    int num_lines = static_cast<int>(editor->numLines());
    QInputDialog dialog(this);
    dialog.setWindowTitle("Go to line...");
    dialog.setLabelText("Line: (max " + QString::number(num_lines) + ")");
    dialog.setIntRange(1, num_lines);
    dialog.setIntValue(static_cast<int>(editor->currentLine()+1));
    dialog.setIntStep(1);
    QSpinBox* spinbox = dialog.findChild<QSpinBox*>();
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if(dialog.exec() == QDialog::Accepted)
        editor->goToLine(spinbox->value() - 1);
}

void MainWindow::onSplitterResize(int pos, int index) {
    program_control_of_hsplitter = true;
    ui->actionShow_project_browser->setChecked(pos != 0);
    program_control_of_hsplitter = false;
}

void MainWindow::setHSplitterDefaultWidth() {
    horizontal_splitter->setSizes({FILE_BROWSER_WIDTH, width()-FILE_BROWSER_WIDTH});
    ui->actionShow_project_browser->setChecked(true);
}

void MainWindow::setVSplitterDefaultHeight() {
    const int h = vertical_splitter->height();
    const int console_height = h/3;
    vertical_splitter->setSizes({h-console_height, console_height});
}

void MainWindow::on_actionGoBack_triggered() {
    assert(viewing_index > 0);
    const JumpPoint& jump_point = viewing_chain[--viewing_index];
    setEditorToModelAndLine(jump_point.model, jump_point.line);
    updateViewJumpPointElements();
}

void MainWindow::on_actionGoForward_triggered() {
    assert(viewing_index < viewing_chain.size()-1);
    const JumpPoint& jump_point = viewing_chain[++viewing_index];
    setEditorToModelAndLine(jump_point.model, jump_point.line);
    updateViewJumpPointElements();
}

void MainWindow::viewModel(Forscape::Typeset::Model* model, size_t line) {
    viewing_chain.erase(viewing_chain.begin() + viewing_index, viewing_chain.end());
    viewing_chain.push_back(JumpPoint(editor->getModel(), editor->currentLine()));
    viewing_chain.push_back(JumpPoint(model, line));
    viewing_index = viewing_chain.size()-1;
    setEditorToModelAndLine(model, line);
    updateViewJumpPointElements();
    project_browser->setCurrentlyViewed(model);
}

void MainWindow::viewModel(Forscape::Typeset::Model* model) {
    if(model != editor->getModel()) viewModel(model, 0);
}

void MainWindow::viewSelection(const Forscape::Typeset::Selection& sel) {
    viewModel(sel.getModel(), sel.left.getLine()->id);
    editor->getController() = sel;
}

bool MainWindow::on_actionSave_All_triggered() {
    static std::vector<Forscape::Typeset::Model*> failed;

    for(Forscape::Typeset::Model* model : modified_files){
        bool success = saveAs(toQString(model->path), model);
        if(!success) failed.push_back(model);
    }

    modified_files.clear();

    bool success = failed.empty();
    for(Forscape::Typeset::Model* model : failed)
        modified_files.insert(model);
    failed.clear();

    onTextChanged();

    return success;
}

void MainWindow::on_actionReload_triggered() {
    Forscape::Typeset::Model* model = editor->getModel();
    assert(!model->path.empty());
    std::ifstream in(model->path);
    if(!in.is_open()){
        QMessageBox messageBox;
        QString str = toQString(model->path);
        messageBox.critical(nullptr, "Error", "Could not open \"" + str + "\" to read.");
        messageBox.setFixedSize(500,200);
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    if(isIllFormedUtf8(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + toQString(model->path) + "\" is not UTF-8 encoded.");
        messageBox.setFixedSize(500,200);
        return;
    }

    if(!Forscape::isValidSerial(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + toQString(model->path) + "\" does not conform to Forscape encoding.\n"
            "This is caused by corruption, or erroneous typesetting specifiers.");
        messageBox.setFixedSize(500,200);
        return;
    }

    Forscape::Typeset::Controller& controller = editor->getController();
    controller.selectAll();
    controller.insertSerial(src);
    //EVENTUALLY: leave the controller at the same place as before, if possible
    model->resetUndoRedo();

    onTextChanged();
    editor->updateModel();
}

void MainWindow::onForcedExit() {
    if(!modified_files.empty()){
        seconds_to_shutdown = 20;
        shutdown_timer.start(1000);
    }
}

void MainWindow::updateDuringForcedExit() {
    if(QGuiApplication::focusWindow() != nullptr){
        shutdown_timer.stop();
        seconds_to_shutdown = -1;
        onTextChanged(); //Need to reset the window title
        return;
    }

    //EVENTUALLY: this doesn't dynamically update. Other apps can dynamically update this.
    setWindowTitle("Unsaved changes, automatically save and close in " +
                   QString::number(seconds_to_shutdown) + " seconds");
    seconds_to_shutdown--;

    if(seconds_to_shutdown == 0) {
        if(on_actionSave_All_triggered()){ //EVENTUALLY: this isn't possible for previously unsaved files
            seconds_to_shutdown = -1;
            close();
        }else{
            exit(0); //EVENTUALLY: should probably do something better here
        }
    }
}

void MainWindow::on_actionClearRecentProjects_triggered() {
    recent_projects.clear();
    if(!project_path.isEmpty()) recent_projects.push_back(project_path);
    ui->menuRecent_Projects->clear();
    ui->menuRecent_Projects->setEnabled(false);
}

void MainWindow::openRecent(QAction* action) {
    QString path = action->data().toString();
    openProject(path);
}

void MainWindow::loadRecentProjects() {
    connect(ui->menuRecent_Projects, SIGNAL(triggered(QAction*)), this, SLOT(openRecent(QAction*)));
    if(!settings.contains(RECENT_PROJECTS)) return;
    recent_projects = settings.value(RECENT_PROJECTS).toStringList();
    updateRecentProjectsFromList();
}

void MainWindow::updateRecentProjectsFromList() {
    ui->menuRecent_Projects->clear();
    if(recent_projects.size() < 2){
        ui->menuRecent_Projects->setDisabled(true);
        return;
    }

    size_t entries = 0;
    for(int i = 1; i < recent_projects.size(); i++){
        const QString& recent_project = recent_projects[i];
        if(QFileInfo::exists(recent_project)){
            QFileInfo info(recent_project);
            QAction* open_action = new QAction(info.fileName());
            open_action->setData(recent_project);
            open_action->setToolTip(recent_project);  //EVENTUALLY: parse a project description in the tooltip also
            ui->menuRecent_Projects->addAction(open_action);

            if(++entries == MAX_DISPLAYED_RECENT_PROJECTS) break;
        }
    }

    ui->menuRecent_Projects->addSeparator();
    ui->menuRecent_Projects->addAction(ui->actionClearRecentProjects);
    ui->menuRecent_Projects->setEnabled(true);
}

void MainWindow::updateRecentProjectsFromCurrent() {
    if(project_path.isEmpty()) return;
    if(recent_projects.empty()) recent_projects.push_back(project_path);

    if(recent_projects.front() != project_path){
        recent_projects.removeOne(project_path);
        recent_projects.push_front(project_path);
        if(recent_projects.size() > MAX_STORED_RECENT_PROJECTS) recent_projects.pop_back();
        updateRecentProjectsFromList();
    }
}

void MainWindow::on_actionGo_to_main_file_triggered() {
    Forscape::Typeset::Model* entry_point = Forscape::Program::instance()->program_entry_point;
    if(entry_point != editor->getModel()) viewModel(entry_point, 0);
}
