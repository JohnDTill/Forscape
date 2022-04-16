#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <hope_logging.h>
#include <typeset_model.h>
#include <typeset_painter.h>
#include <typeset_view.h>
#include <hope_scanner.h>
#include <hope_serial.h>
#include <hope_serial_unicode.h>
#include <hope_parser.h>
#include <hope_symbol_build_pass.h>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSpinBox>
#include <QSplitter>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include "mathtoolbar.h"
#include "preferences.h"
#include "searchdialog.h"
#include "symboltreeview.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef NDEBUG
#include "qgraphvizcall.h"
#endif

#define ACTIVE_FILE "active_file"
#define ZOOM_EDITOR "editor_zoom"
#define ZOOM_CONSOLE "console_zoom"
#define LINE_NUMBERS_VISIBLE "line_nums_shown"
#define WINDOW_TITLE_SUFFIX " - Forscape"
#define NEW_SCRIPT_TITLE "new script"
#define MATH_TOOLBAR_VISIBLE "math_tb_visible"
#define ACTION_TOOLBAR_VISIBLE "action_tb_visible"
#define WINDOW_GEOMETRY "geometry"
#define WINDOW_STATE "window_state"
#define LAST_DIRECTORY "last_dir"

#define LOG_PREFIX "mainwindow->"

using namespace Hope;

static std::filesystem::file_time_type write_time;

static QTimer* external_change_timer;

static constexpr int CHANGE_CHECK_PERIOD_MS = 100;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow){
    ui->setupUi(this);
    Typeset::Painter::init();

    external_change_timer = new QTimer(this);
    connect(external_change_timer, &QTimer::timeout, this, &MainWindow::checkForChanges);
    external_change_timer->start(CHANGE_CHECK_PERIOD_MS);

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    setCentralWidget(splitter);

    editor = new Typeset::View();
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    if(settings.contains(ACTIVE_FILE))
        open(settings.value(ACTIVE_FILE).toString());
    splitter->addWidget(editor);

    group_box = new QGroupBox(this);
    group_box->setTitle("Console");

    QVBoxLayout* vbox = new QVBoxLayout();
    group_box->setLayout(vbox);
    group_box->resize(width(), height()*0.7);
    splitter->addWidget(group_box);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    console = new Typeset::View();
    console->setLineNumbersVisible(false);
    console->setReadOnly(true);
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

    int id = QFontDatabase::addApplicationFont(":/toolbar_glyphs.otf");
    assert(id!=-1);
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont glyph_font = QFont(family);
    glyph_font.setPointSize(18);

    action_toolbar = addToolBar(tr("File"));
    action_toolbar->setObjectName("action_toolbar");
    QAction* run_act = new QAction(tr("Œ"), this);
    run_act->setToolTip("Run script   Ctrl+R");
    run_act->setFont(glyph_font);
    run_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(run_act, &QAction::triggered, this, &MainWindow::run);
    action_toolbar->addAction(run_act);

    QAction* stop_act = new QAction(tr("Ŗ"), this);
    stop_act->setToolTip("Stop script");
    stop_act->setFont(glyph_font);
    stop_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(stop_act, &QAction::triggered, this, &MainWindow::stop);
    action_toolbar->addAction(stop_act);

    #ifndef NDEBUG
    QAction* ast_act = new QAction(tr("œ"), this);
    ast_act->setToolTip("Show AST");
    ast_act->setFont(glyph_font);
    ast_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(ast_act, &QAction::triggered, this, &MainWindow::parseTree);
    action_toolbar->addAction(ast_act);

    QAction* sym_act = new QAction(tr("Ŕ"), this);
    sym_act->setToolTip("Show symbol table");
    sym_act->setFont(glyph_font);
    sym_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(sym_act, &QAction::triggered, this, &MainWindow::symbolTable);
    action_toolbar->addAction(sym_act);
    #endif

    QAction* github_act = new QAction(tr("ŕ"), this);
    github_act->setToolTip("View on GitHub");
    github_act->setFont(glyph_font);
    github_act->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(github_act, &QAction::triggered, this, &MainWindow::github);
    action_toolbar->addAction(github_act);

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

    if(settings.contains(WINDOW_GEOMETRY))
        restoreGeometry(settings.value(WINDOW_GEOMETRY).toByteArray());

    if(settings.contains(WINDOW_STATE))
        restoreState(settings.value(WINDOW_STATE).toByteArray());

    connect(&interpreter_poll_timer, SIGNAL(timeout()), this, SLOT(pollInterpreterThread()));
    connect(editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

    preferences = new Preferences(settings);
    connect(preferences, SIGNAL(colourChanged()), this, SLOT(onColourChanged()));
    onColourChanged();
}

MainWindow::~MainWindow(){
    settings.setValue(ZOOM_CONSOLE, console->zoom);
    settings.setValue(ZOOM_EDITOR, editor->zoom);
    settings.setValue(LINE_NUMBERS_VISIBLE, editor->lineNumbersShown());
    settings.setValue(MATH_TOOLBAR_VISIBLE, ui->actionShow_typesetting_toolbar->isChecked());
    settings.setValue(ACTION_TOOLBAR_VISIBLE, ui->actionShow_action_toolbar->isChecked());
    settings.setValue(WINDOW_GEOMETRY, saveGeometry());
    settings.setValue(WINDOW_STATE, saveState());
    delete preferences;
    delete ui;
}

bool MainWindow::isSavedDeepComparison() const{
    if(path.isEmpty()) return editor->getModel()->empty();

    //Avoid a deep comparison if size from file meta data doesn't match
    std::string filename = settings.value(ACTIVE_FILE).toString().toStdString();
    if(std::filesystem::file_size(filename) != editor->getModel()->serialChars()) return false;

    std::ifstream in(filename);
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

void MainWindow::run(){
    logger->info(LOG_PREFIX "run();");

    if(editor->isRunning()) return;

    console->setModel(Typeset::Model::fromSerial("", true));
    editor->runThread(console);
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
    logger->info(LOG_PREFIX "stop(); //Note: potential for non-repeatable timing dependent behaviour.");

    if(!editor->isRunning()) return;
    editor->getModel()->stop();
}

void MainWindow::pollInterpreterThread(){
    auto& interpreter = editor->getModel()->interpreter;

    if(interpreter.status == Hope::Code::Interpreter::FINISHED){
        checkOutput();

        auto output = console->getModel();
        switch(interpreter.error_code){
            case Hope::Code::NO_ERROR_FOUND: break;
            case Hope::Code::USER_STOP:
                console->getController().insertSerial("\nScript terminated by user");
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

        out.clear();
        editor->reenable();
        interpreter_poll_timer.stop();
        if(editor_had_focus) editor->setFocus();
    }else{
        checkOutput();
    }
}

void MainWindow::parseTree(){
    #ifndef NDEBUG
    logger->info(LOG_PREFIX "parseTree();");
    QString dot_src = QString::fromStdString(editor->getModel()->parseTreeDot());
    dot_src.replace("\\n", "\\\\n");
    QGraphvizCall::show(dot_src);
    #endif
}

void MainWindow::symbolTable(){
    #ifndef NDEBUG
    logger->info(LOG_PREFIX "symbolTable();");
    Typeset::Model* m = editor->getModel();
    SymbolTreeView* view = new SymbolTreeView(m->symbol_builder.symbol_table, m->static_pass);
    view->show();
    #endif
}

void MainWindow::github(){
    QDesktopServices::openUrl(QUrl("https://github.com/JohnDTill/Forscape"));
}

void MainWindow::on_actionNew_triggered(){
    logger->info(LOG_PREFIX "on_actionNew_triggered();");
    if(!editor->isEnabled()) return;
    editor->setFromSerial("");
    setWindowTitle(NEW_SCRIPT_TITLE WINDOW_TITLE_SUFFIX);
    settings.remove(ACTIVE_FILE);
    path.clear();
}


void MainWindow::on_actionOpen_triggered(){
    if(!editor->isEnabled()) return;

    QString path = QFileDialog::getOpenFileName(nullptr, tr("Load File"), getLastDir(), tr("Text (*.txt)"));
    if(path.isEmpty()) return;

    open(path);
}


bool MainWindow::on_actionSave_triggered(){
    if(!editor->isEnabled()) return false;

    if(path.isEmpty()) return savePrompt();
    else return saveAs(path);
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
    char ch;
    while(message_queue.try_dequeue(ch))
        if(ch == '\0'){
            print_buffer += out;
            out.clear();
        }else{
            out += ch;
        }

    if(!print_buffer.empty()){
        bool at_bottom = console->scrolledToBottom();
        console->getController().insertSerial(print_buffer);
        console->updateModel();
        if(at_bottom) console->scrollToBottom();
        print_buffer.clear();
    }
}

bool MainWindow::savePrompt(){
    if(!editor->isEnabled()) return false;

    QString prompt_name = path.isEmpty() ? getLastDir() + "/untitled.txt" : path;
    QString file_name = QFileDialog::getSaveFileName(nullptr, tr("Save File"),
                                prompt_name,
                                tr("Text (*.txt)"));

    if(!file_name.isEmpty()) return saveAs(file_name);
    return false;
}

bool MainWindow::saveAs(QString path){
    logger->info(LOG_PREFIX "saveAs({});", cStr(path.toStdString()));

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
    settings.setValue(ACTIVE_FILE, path);
    this->path = path;
    unsaved_changes = false;
    out.flush();
    write_time = std::filesystem::last_write_time(path.toStdString());
    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());

    return true;
}

void MainWindow::open(QString path){
    logger->info("//" LOG_PREFIX "open({})", cStr(path.toStdString()));

    QFile file(path);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Could not open \"" + path + "\" to read.");
        messageBox.setFixedSize(500,200);
        return;
    }

    QTextStream in(&file);
    #ifdef QT5
    in.setCodec("UTF-8");
    #endif

    std::string src = in.readAll().toStdString();
    logger->info("editor->setFromSerial({});", cStr(src));
    if(!Hope::isValidSerial(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + path + "\" is corrupted.");
        messageBox.setFixedSize(500,200);
        return;
    }

    editor->setFromSerial(src);
    setWindowTitle(file.fileName() + WINDOW_TITLE_SUFFIX);
    settings.setValue(ACTIVE_FILE, path);
    this->path = path;
    unsaved_changes = false;
    write_time = std::filesystem::last_write_time(path.toStdString());

    settings.setValue(LAST_DIRECTORY, QFileInfo(path).absoluteDir().absolutePath());
}

QString MainWindow::getLastDir(){
    if(settings.contains(LAST_DIRECTORY)){
        QString last_dir = settings.value(LAST_DIRECTORY).toString();
        if(std::filesystem::is_directory(last_dir.toStdString())) return last_dir;
    }

    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::on_actionFind_Replace_triggered(){
    if(!editor->isEnabled()) return;

    Typeset::Controller& c = editor->getController();
    std::string simple_word = c.isTextSelection() ? c.selectedText() : "";
    SearchDialog search(this, editor, console, QString::fromStdString(simple_word));
    search.exec();
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
    logger->info(LOG_PREFIX "on_actionUnicode_triggered();");

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
    bool changed_from_save = !isSavedDeepComparison();

    if(changed_from_save && !unsaved_changes)
        setWindowTitle(windowTitle().insert(0, '*'));
    else if(unsaved_changes && !changed_from_save)
        setWindowTitle(windowTitle().remove(0, 1));

    unsaved_changes = changed_from_save;
}

void MainWindow::closeEvent(QCloseEvent* event){
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

    logger->info("assert(editor->toSerial() == {});", cStr(editor->toSerial()));

    QMainWindow::closeEvent(event);
}

    auto& interpreter = editor->getModel()->interpreter;
    auto& message_queue = interpreter.message_queue;
    char ch;
    while(message_queue.try_dequeue(ch))
        if(ch == '\0'){
            print_buffer += out;
            out.clear();
        }else{
            out += ch;
        }

    if(!print_buffer.empty()){
        bool at_bottom = console->scrolledToBottom();
        console->getController().insertSerial(print_buffer);
        console->updateModel();
        if(at_bottom) console->scrollToBottom();
        print_buffer.clear();
    }
}

void MainWindow::on_actionShow_action_toolbar_toggled(bool show){
    action_toolbar->setVisible(show);
}


void MainWindow::on_actionShow_typesetting_toolbar_toggled(bool show){
    math_toolbar->setVisible(show);
}

void MainWindow::checkForChanges(){
    if(path.isEmpty()) return;

    assert(settings.contains(ACTIVE_FILE));
    std::string filename = settings.value(ACTIVE_FILE).toString().toStdString();

    const std::filesystem::file_time_type modified_time = std::filesystem::last_write_time(filename);
    if(modified_time <= write_time) return;
    write_time = modified_time;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "File modified externally",
        "This file has been modified externally.\nWould you like to reload it?",
        QMessageBox::Yes|QMessageBox::No
    );

    if(reply == QMessageBox::Yes) open(path);
    else onTextChanged();
}

void MainWindow::on_actionSee_log_triggered(){
    openLogFile();
}

void MainWindow::on_actionPreferences_triggered(){
    preferences->show();
    preferences->raise();  // for MacOS
    preferences->activateWindow(); // for Windows
}

void MainWindow::onColourChanged(){
    QPalette p = QGuiApplication::palette();
    p.setColor(QPalette::Text, Hope::Typeset::getColour(Hope::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::WindowText, Hope::Typeset::getColour(Hope::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::PlaceholderText, Hope::Typeset::getColour(Hope::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::Dark, Hope::Typeset::getColour(Hope::Typeset::COLOUR_TEXT));
    p.setColor(QPalette::Highlight, Hope::Typeset::getColour(Hope::Typeset::COLOUR_SELECTION));
    p.setColor(QPalette::HighlightedText, Hope::Typeset::getColour(Hope::Typeset::COLOUR_SELECTEDTEXT));
    p.setColor(QPalette::Window, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    p.setColor(QPalette::Button, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    p.setColor(QPalette::Base, Hope::Typeset::getColour(Hope::Typeset::COLOUR_BACKGROUND));
    p.setColor(QPalette::Light, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LIGHT));
    p.setColor(QPalette::Link, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINK));
    p.setColor(QPalette::LinkVisited, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINK));
    setPalette(p);

    //Set colours which should not affect filebar
    p.setColor(QPalette::ButtonText, Hope::Typeset::getColour(Hope::Typeset::COLOUR_TEXT));
    //p.setColor(QPalette::Mid, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::Midlight, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::Shadow, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    //p.setColor(QPalette::AlternateBase, Hope::Typeset::getColour(Hope::Typeset::COLOUR_LINEBOXFILL));
    group_box->setPalette(p);
    action_toolbar->setPalette(p);
    math_toolbar->setPalette(p);
    //preferences->setPalette(p); //EVENTUALLY: get themes to work with popup windows
}

void MainWindow::on_actionGo_to_line_triggered(){
    QInputDialog dialog(this);
    dialog.setWindowTitle("Go to line...");
    dialog.setLabelText("Line:");
    dialog.setIntRange(1, editor->numLines());
    dialog.setIntValue(editor->currentLine()+1);
    dialog.setIntStep(1);
    QSpinBox* spinbox = dialog.findChild<QSpinBox*>();

    if(dialog.exec() == QDialog::Accepted)
        editor->goToLine(spinbox->value() - 1);
}
