#include <forscape_interpreter.h>
#include <forscape_parser.h>
#include <forscape_program.h>
#include <forscape_scanner.h>
#include "report.h"
#include "typeset.h"
#include <typeset_view.h>
#include <QCoreApplication>

static bool testHighlighting(Typeset::Editor* view, Typeset::Model* m){
    bool passing = true;

    view->setModel(m);

    Typeset::Controller& controller = view->getController();
    controller.moveToEndOfDocument();
    view->handleKey(Qt::Key_Left, Qt::NoModifier, ""); QCoreApplication::processEvents();
    passing |= (view->highlighted_words.empty());
    view->handleKey(Qt::Key_Left, Qt::NoModifier, ""); QCoreApplication::processEvents();

    passing |= (view->highlighted_words.size() == 2);
    passing |= (view->highlighted_words.front().strView() == "helloWorld");

    view->handleKey(Qt::Key_Home, Qt::NoModifier, ""); QCoreApplication::processEvents();

    passing |= (view->highlighted_words.size() == 2);
    passing |= (view->highlighted_words.front().strView() == "helloWorld");

    return passing;
}

static bool testHover(Typeset::Editor* view, Typeset::Model* m){
    bool passing = true;

    view->setModel(m);

    Typeset::Controller& controller = view->getController();
    controller.moveToEndOfDocument();
    view->handleKey(Qt::Key_Home, Qt::NoModifier, ""); QCoreApplication::processEvents();
    view->dispatchHover(controller.getAnchor().x(), controller.getAnchor().y());

    const ParseNode hover_node = view->hover_node + m->parse_node_offset;

    assert(view->hover_node != NONE);
    passing |= (Forscape::Program::instance()->parse_tree.str(hover_node) == "helloWorld");

    return passing;
}

inline bool testIdeFeatures(){
    #define HIGHLIGHTING_TEST_FILE_PATH \
        std::filesystem::u8path(BASE_TEST_DIR "/in/hello_world_from.π")
    std::string in = readFile(HIGHLIGHTING_TEST_FILE_PATH.u8string());

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    input->path = std::filesystem::canonical(HIGHLIGHTING_TEST_FILE_PATH);
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();
    const auto& all_files = Forscape::Program::instance()->allFiles();
    assert(all_files.size() == 2);
    Typeset::Model* imported_model = (all_files.front() != input) ? all_files.front() : all_files.back();
    Typeset::Editor* view = new Typeset::Editor;

    bool passing = true;

    const bool highlighting_passing = testHighlighting(view, input) && testHighlighting(view, imported_model);
    passing |= highlighting_passing;
    report("IDE highlighting", highlighting_passing);

    const bool hover_passing = testHover(view, input) && testHover(view, imported_model);
    passing |= hover_passing;
    report("IDE tooltips", hover_passing);

    Program::instance()->freeFileMemory();
    delete view;

    return passing;
}
