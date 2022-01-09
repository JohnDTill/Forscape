#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <typeset_model.h>
#include <typeset_view.h>
#include <hope_scanner.h>
#include <hope_serial.h>
#include <hope_serial_unicode.h>
#include <hope_parser.h>
#include <hope_symbol_build_pass.h>
#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QGroupBox>
#include <QMessageBox>
#include <QMimeData>
#include <QSplitter>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include "mathtoolbar.h"
#include "searchdialog.h"
#include "symboltreeview.h"

#include <iostream>

#ifndef NDEBUG
#include "qgraphvizcall.h"
#endif

using namespace Hope;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow){
    ui->setupUi(this);

    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    setCentralWidget(splitter);

    editor = new Typeset::View();
    splitter->addWidget(editor);

    QGroupBox* group_box = new QGroupBox(this);
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

    setWindowTitle("Forscape - new script");

    QGuiApplication::setWindowIcon(QIcon("lambda.ico"));

    editor->setFocus();

    QToolBar* fileToolBar = addToolBar(tr("File"));
    QAction* newAct = new QAction(tr("⏵ Run"), this);
    newAct->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(newAct, &QAction::triggered, this, &MainWindow::run);
    fileToolBar->addAction(newAct);

    newAct = new QAction(tr("■ Stop"), this);
    newAct->setShortcuts(QKeySequence::InsertLineSeparator);
    connect(newAct, &QAction::triggered, this, &MainWindow::stop);
    fileToolBar->addAction(newAct);

    resize(width()+1, height()+1);

    MathToolbar* toolbar = new MathToolbar(this);
    connect(toolbar, SIGNAL(insertFlatText(QString)), this, SLOT(insertFlatText(const QString&)));
    connect(toolbar, SIGNAL(insertSerial(QString)), this, SLOT(insertSerial(const QString&)));
    connect(toolbar, SIGNAL(insertSerialSelection(QString, QString)),
            this, SLOT(insertSerialSelection(QString, QString)));
    addToolBarBreak(Qt::ToolBarArea::TopToolBarArea);
    addToolBar(Qt::ToolBarArea::TopToolBarArea, toolbar);

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
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::run(){
    if(!editor->isEnabled()) return;

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

        editor->setEnabled(false);
        editor->setReadOnly(true);
    }
}

void MainWindow::stop(){
    if(editor->isEnabled()) return;
    editor->getModel()->stop();
    if(editor_had_focus) editor->setFocus();
}

void MainWindow::pollInterpreterThread(){
    auto& interpreter = editor->getModel()->interpreter;
    auto& message_queue = interpreter.message_queue;
    std::string out;
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

    if(interpreter.status == Hope::Code::Interpreter::FINISHED){
        auto output = console->getModel();
        switch(interpreter.error_code){
            case Hope::Code::NO_ERROR: break;
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

        editor->setEnabled(true);
        editor->setReadOnly(false);
        interpreter_poll_timer.stop();
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
    SymbolTreeView* view = new SymbolTreeView(m->parser.parse_tree, m->symbol_builder.symbol_table);
    view->show();
    #endif
}

void MainWindow::on_actionNew_triggered(){
    if(!editor->isEnabled()) return;
    editor->setFromSerial("");
    setWindowTitle("Forscape - new script");
    path.clear();
}


void MainWindow::on_actionOpen_triggered(){
    if(!editor->isEnabled()) return;

    QString path = QFileDialog::getOpenFileName(nullptr, tr("Load File"), "./", tr("Text (*.txt)"));
    if(path.isEmpty()) return;

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
    if(!Hope::isValidSerial(src)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "\"" + path + "\" is corrupted.");
        messageBox.setFixedSize(500,200);
        return;
    }

    editor->setFromSerial(src);
    setWindowTitle("Forscape - " + file.fileName());
    this->path = path;
}


void MainWindow::on_actionSave_triggered(){
    if(!editor->isEnabled()) return;

    if(path.isEmpty()) savePrompt();
    else saveAs(path);
}


void MainWindow::on_actionSave_As_triggered(){
    if(!editor->isEnabled()) return;

    savePrompt();
}


void MainWindow::on_actionExit_triggered(){
    exit(0);
}

void MainWindow::savePrompt(){
    if(!editor->isEnabled()) return;

    QString prompt_name = "untitled.txt";
    QString file_name = QFileDialog::getSaveFileName(nullptr, tr("Save File"),
                                prompt_name,
                                tr("Text (*.txt)"));

    if(!file_name.isEmpty()) saveAs(file_name);
}

void MainWindow::saveAs(QString path){
    if(!editor->isEnabled()) return;

    QFile file(path);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Could not open \"" + path + "\" to write.");
        messageBox.setFixedSize(500,200);
        return;
    }

    QTextStream out(&file);
    #ifdef QT5
    out.setCodec("UTF-8");
    #endif
    out << QString::fromStdString(editor->getModel()->toSerial());

    setWindowTitle("Forscape - " + file.fileName());
    this->path = path;
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

    editor->getController().del();
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

    editor->getController().insertText(str.toStdString());
    editor->update();
}

void MainWindow::insertSerial(const QString& str){
    if(!editor->isEnabled()) return;

    editor->getController().insertSerial(str.toStdString());
    editor->update();
}

void MainWindow::insertSerialSelection(const QString& A, const QString& B){
    if(!editor->isEnabled()) return;

    Typeset::Controller& c = editor->getController();
    c.insertSerial(A.toStdString() + c.selectedText() + B.toStdString());
    editor->update();
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
