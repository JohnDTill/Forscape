#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <typeset_model.h>
#include <typeset_painter.h>
#include <typeset_view.h>
#include <forscape_scanner.h>
#include <forscape_serial.h>
#include <forscape_serial_unicode.h>
#include <forscape_parser.h>
#include <forscape_program.h>
#include <forscape_symbol_build_pass.h>
#include <forscape_message.h>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>

#include "mathtoolbar.h"
#include "plot.h"
#include "preferences.h"
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
#define FORSCAPE_FILE_TYPE_DESC "Forscape script (*.π)"

using namespace Forscape;

static std::filesystem::file_time_type write_time;

static QTimer* external_change_timer;

static constexpr int CHANGE_CHECK_PERIOD_MS = 100;

static constexpr int FILE_BROWSER_WIDTH = 200;
static bool program_control_of_hsplitter = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow){
    ui->setupUi(this);
    Typeset::Painter::init();

    external_change_timer = new QTimer(this);
    connect(external_change_timer, &QTimer::timeout, this, &MainWindow::checkForChanges);
    external_change_timer->start(CHANGE_CHECK_PERIOD_MS);

    horizontal_splitter = new Splitter(Qt::Horizontal, this);
    project_browser = new QTreeWidget(horizontal_splitter);
    project_browser->setHeaderHidden(true);
    project_browser->setIndentation(10);
    project_browser->setMinimumWidth(120);
    connect(project_browser, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(onFileClicked(QTreeWidgetItem*, int)));
    project_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(project_browser, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onFileRightClicked(const QPoint&)));
    QTreeWidgetItem* root = new QTreeWidgetItem(project_browser);
    root->setText(0, "NOT IMPLEMENTED");
    root->setIcon(0, QFileIconProvider().icon(QFileIconProvider::Folder));
    QTreeWidgetItem* leaf = new QTreeWidgetItem(root);
    leaf->setText(0, "Wire up project browser");
    leaf->setIcon(0, QFileIconProvider().icon(QFileIconProvider::File));
    QTreeWidgetItem* anchor_leaf = new QTreeWidgetItem(root);
    anchor_leaf->setText(0, "Anchor leaf");
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
    if(settings.contains(PROJECT_ROOT_FILE))
        openProject(settings.value(PROJECT_ROOT_FILE).toString());
    vertical_splitter->addWidget(editor);
    resetViewJumpPointElements();

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

    action_toolbar = addToolBar(tr("File"));
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

    connect(editor, SIGNAL(goToModel(Forscape::Typeset::Model*, size_t)),
            this, SLOT(viewModel(Forscape::Typeset::Model*, size_t)));

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

    preferences = new Preferences(settings);
    connect(preferences, SIGNAL(colourChanged()), this, SLOT(onColourChanged()));
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
}

MainWindow::~MainWindow(){
    settings.setValue(ZOOM_CONSOLE, console->zoom);
    settings.setValue(ZOOM_EDITOR, editor->zoom);
    settings.setValue(LINE_NUMBERS_VISIBLE, editor->lineNumbersShown());
    settings.setValue(MATH_TOOLBAR_VISIBLE, ui->actionShow_typesetting_toolbar->isChecked());
    settings.setValue(ACTION_TOOLBAR_VISIBLE, ui->actionShow_action_toolbar->isChecked());
    settings.setValue(PROJECT_BROWSER_VISIBLE, ui->actionShow_project_browser->isChecked());
    settings.setValue(WINDOW_STATE, QList({QVariant(saveGeometry()), QVariant(saveState())}));
    search->saveSettings(settings);
    delete preferences;
    delete ui;
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

bool MainWindow::isSavedDeepComparison() const {
    if(active_file_path.isEmpty()) return editor->getModel()->empty();

    //Avoid a deep comparison if size from file meta data doesn't match
    auto filename = active_file_path.toStdString();
    auto path = std::filesystem::u8path(filename);
    if(std::filesystem::file_size(path) != editor->getModel()->serialChars()) return false;

    std::ifstream in(path);
    if(!in.is_open()) std::cout << "Failed to open " << filename << std::endl;
    assert(in.is_open());

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string saved_src = buffer.str();
    saved_src.erase( std::remove(saved_src.begin(), saved_src.end(), '\r'), saved_src.end() );

    //MAYDO: no need to convert model to serial, but cost is probably negligible compared to I/O
    std::string curr_src = editor->getModel()->toSerial();

    return saved_src == curr_src;
}

void MainWindow::updateViewJumpPointElements() {
    bool can_go_forward = viewing_index < viewing_chain.size()-1;
    bool can_go_backward = viewing_index > 0;
    ui->actionGoForward->setEnabled(can_go_forward);
    file_next->setEnabled(can_go_forward);
    ui->actionGoBack->setEnabled(can_go_backward);
    file_back->setEnabled(can_go_backward);

    active_file_path = QString::fromStdString(editor->getModel()->path.string());
    onTextChanged();
}

void MainWindow::resetViewJumpPointElements() {
    viewing_chain.clear();
    viewing_chain.push_back(JumpPoint(editor->getModel(), 0));
    viewing_index = 0;
}

void MainWindow::run(){
    if(editor->isRunning()) return;

    editor->runThread();
    if(editor->getModel()->errors.empty()){
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
    editor->getModel()->stop();
}

void MainWindow::pollInterpreterThread(){
    auto& interpreter = editor->getModel()->interpreter;

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
                Typeset::Selection c = model->parser.parse_tree.getSelection(interpreter.error_node);
                model->errors.push_back(Code::Error(c, interpreter.error_code));
                output->appendLine();
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

void MainWindow::parseTree(){
    #ifndef NDEBUG
    QString dot_src = QString::fromStdString(editor->getModel()->parseTreeDot());
    dot_src.replace("\\n", "\\\\n");
    QGraphvizCall::show(dot_src);
    #endif
}

void MainWindow::symbolTable(){
    #ifndef NDEBUG
    Typeset::Model* m = editor->getModel();
    SymbolTreeView* view = new SymbolTreeView(m->symbol_builder.symbol_table, m->static_pass);
    view->show();
    #endif
}

void MainWindow::github(){
    QDesktopServices::openUrl(QUrl("https://github.com/JohnDTill/Forscape"));
}

void MainWindow::on_actionNew_triggered(){
    if(!editor->isEnabled()) return;
    Typeset::Model* model = Typeset::Model::fromSerial("");
    viewModel(model, 0);
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    active_file_path.clear();
}

void MainWindow::on_actionOpen_triggered(){
    if(!editor->isEnabled()) return;

    QString path = QFileDialog::getOpenFileName(nullptr, tr("Open Project"), getLastDir(), tr(FORSCAPE_FILE_TYPE_DESC));
    if(path.isEmpty()) return;

    openProject(path);
}

bool MainWindow::on_actionSave_triggered(){
    if(!editor->isEnabled()) return false;

    if(active_file_path.isEmpty()) return savePrompt();
    else return saveAs(active_file_path);
}

void MainWindow::on_actionSave_As_triggered(){
    if(!editor->isEnabled()) return;

    savePrompt();
}

void MainWindow::on_actionExit_triggered(){
    close();
}

void MainWindow::checkOutput(){
    auto& interpreter = editor->getModel()->interpreter;
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

    QString prompt_name = active_file_path.isEmpty() ? getLastDir() + "/untitled.π" : active_file_path;
    QString file_name = QFileDialog::getSaveFileName(nullptr, tr("Save File"),
                                prompt_name,
                                tr(FORSCAPE_FILE_TYPE_DESC));

    if(!file_name.isEmpty()) return saveAs(file_name);
    return false;
}

bool MainWindow::saveAs(QString path){
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
    out << QByteArray::fromStdString(editor->getModel()->toSerial());

    setWindowTitle(file.fileName() + WINDOW_TITLE_SUFFIX);
    if(project_path.isEmpty()){
        settings.setValue(PROJECT_ROOT_FILE, path);
        project_path = path;
    }
    active_file_path = path;
    out.flush();
    write_time = std::filesystem::last_write_time(std::filesystem::u8path(path.toStdString()));
    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());

    editor->getModel()->path = std::filesystem::u8path(active_file_path.toStdString());

    return true;
}

void MainWindow::openProject(QString path){
    std::filesystem::path std_path = std::filesystem::u8path(path.toStdString());
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

    assert(Forscape::isValidSerial(src));
    if(!Forscape::isValidSerial(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + path + "\" is corrupted.");
        messageBox.setFixedSize(500,200);
        return;
    }

    Typeset::Model* model = Typeset::Model::fromSerial(src);
    std_path = std::filesystem::canonical(std_path);
    model->path = std_path;
    Forscape::Program::instance()->setProgramEntryPoint(std_path, model);
    model->postmutate();
    editor->setModel(model);

    setWindowTitle(QFile(path).fileName() + WINDOW_TITLE_SUFFIX);
    settings.setValue(PROJECT_ROOT_FILE, path);
    project_path = path;
    active_file_path = path;
    write_time = std::filesystem::last_write_time(path.toStdU16String());

    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());

    resetViewJumpPointElements();
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
        if(std::filesystem::is_directory(last_dir.toStdString())) return last_dir;
    }

    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::setEditorToModelAndLine(Forscape::Typeset::Model* model, size_t line){
    editor->setModel(model);
    editor->goToLine(line);
}

void MainWindow::on_actionFind_Replace_triggered(){
    if(!editor->isEnabled()) return;

    search->updateSelection();
    Typeset::Controller& c = editor->getController();
    std::string simple_word = c.isTextSelection() ? c.selectedText() : "";
    search->setWord(QString::fromStdString(simple_word));
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

void MainWindow::insertFlatText(const QString &str){
    if(!editor->isEnabled()) return;

    editor->insertText(str.toStdString());
    editor->update();

    onTextChanged();
}

void MainWindow::insertSerial(const QString& str){
    if(!editor->isEnabled()) return;

    editor->insertSerial(str.toStdString());
    editor->update();

    onTextChanged();
}

void MainWindow::insertSerialSelection(const QString& A, const QString& B){
    if(!editor->isEnabled()) return;

    Typeset::Controller& c = editor->getController();
    editor->insertSerial(A.toStdString() + c.selectedText() + B.toStdString());
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
    generator.setDescription(QString::fromStdString(c.selectedText()));
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
        QApplication::clipboard()->setText(QString::fromStdString(uni));
    }else{
        QMessageBox messageBox;
        messageBox.warning(nullptr, "Warning", "Selected text cannot be converted to unicode.");
        messageBox.setFixedSize(500, 200);
    }
}

void MainWindow::onTextChanged(){
    //EVENTUALLY: doing a deep comparison is terrible. Need a more efficient way to determine if the document is saved
    bool changed_from_save = !editor->getModel()->isSavedDeepComparison();
    setWindowTitle(
        (changed_from_save ? "*" : "") +
        (active_file_path.isEmpty() ? NEW_SCRIPT_TITLE : QFile(active_file_path).fileName()) +
        WINDOW_TITLE_SUFFIX);
}

void MainWindow::closeEvent(QCloseEvent* event){
    //DO THIS: check for unsaved changes to all project files
    if(!isSavedDeepComparison()){
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Unsaved changes", "Save file before closing?",
        QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
        if(reply == QMessageBox::Cancel){
            event->ignore();
            return;
        }else if(reply == QMessageBox::Yes){
            if(!on_actionSave_triggered()){
                event->ignore();
                return;
            }
        }
    }

    preferences->close();
    QMainWindow::closeEvent(event);
    emit destroyed();
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
    else horizontal_splitter->setSizes({0, width()});
}

void MainWindow::checkForChanges(){
    if(active_file_path.isEmpty()) return;

    auto filename = active_file_path.toStdU16String();
    const std::filesystem::file_time_type modified_time = std::filesystem::last_write_time(filename);
    if(modified_time <= write_time) return;
    write_time = modified_time;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "File modified externally",
        "This file has been modified externally.\nWould you like to reload it?",
        QMessageBox::Yes|QMessageBox::No
    );

    if(reply == QMessageBox::Yes) openProject(active_file_path);
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
    QInputDialog dialog(this);
    dialog.setWindowTitle("Go to line...");
    dialog.setLabelText("Line:");
    dialog.setIntRange(1, static_cast<int>(editor->numLines()));
    dialog.setIntValue(static_cast<int>(editor->currentLine()+1));
    dialog.setIntStep(1);
    QSpinBox* spinbox = dialog.findChild<QSpinBox*>();

    if(dialog.exec() == QDialog::Accepted)
        editor->goToLine(spinbox->value() - 1);
}

void MainWindow::onSplitterResize(int pos, int index) {
    program_control_of_hsplitter = true;
    ui->actionShow_project_browser->setChecked(pos != 0);
    program_control_of_hsplitter = false;
}

void MainWindow::onFileClicked(QTreeWidgetItem* item, int column) {
    std::cout << item->text(0).toStdString() << " clicked" << std::endl;
    //EVENTUALLY: go to the file
}

void MainWindow::onFileRightClicked(const QPoint& pos) {
    QTreeWidgetItem* item = project_browser->itemAt(pos);
    if(item == nullptr) return;
    std::cout << item->text(0).toStdString() << " right clicked" << std::endl;

    QMenu menu(this);
    const bool item_is_file = (item->childCount() == 0);
    if(item_is_file){
        menu.addAction("Open File")->setStatusTip("EVENTUALLY");
        menu.addAction("Rename File")->setStatusTip("EVENTUALLY");
    }else{
        menu.addAction("Rename Folder")->setStatusTip("EVENTUALLY");
        menu.addAction("Add New File")->setStatusTip("EVENTUALLY");
    }
    menu.addAction("Show in Explorer")->setStatusTip("EVENTUALLY");
    //connect(newAct, SIGNAL(triggered()), this, SLOT(something()));

    menu.exec(project_browser->mapToGlobal(pos));
}

void MainWindow::setHSplitterDefaultWidth() {
    horizontal_splitter->setSizes({FILE_BROWSER_WIDTH, width()-FILE_BROWSER_WIDTH});
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
    model->postmutate();
    setEditorToModelAndLine(model, line);
    updateViewJumpPointElements();
}

