#include <forscape_interpreter.h>
#include <forscape_parser.h>
#include <forscape_program.h>
#include <forscape_scanner.h>
#include "report.h"
#include "typeset.h"
#include <typeset_view.h>
#include <QCoreApplication>

#if !defined(__MINGW32__) || (__MINGW64_VERSION_MAJOR > 8)
#include <filesystem>
using std::filesystem::directory_iterator;
#else
#error std::filesystem is borked for MinGW versions before 9.0
#endif

using namespace Forscape;
using namespace Code;

inline bool ideCase(const std::filesystem::path& path){
    std::string in = readFile(path.u8string());

    Typeset::Editor* view = new Typeset::Editor;
    Typeset::Model* input = Typeset::Model::fromSerial(in);
    view->setModel(input);
    input->path = std::filesystem::canonical(path);
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();

    Typeset::Controller& controller = view->getController();
    while(!controller.atEnd()){ view->handleKey(Qt::Key_Right, Qt::NoModifier, ""); QCoreApplication::processEvents(); }
    while(!controller.atStart()){ view->handleKey(Qt::Key_Left, Qt::NoModifier, ""); QCoreApplication::processEvents(); }
    while(!controller.atEnd()){ view->handleKey(Qt::Key_Right, Qt::ShiftModifier, ""); QCoreApplication::processEvents(); }
    controller.deselect();
    while(!controller.atStart()){ view->handleKey(Qt::Key_Left, Qt::ShiftModifier, ""); QCoreApplication::processEvents(); }
    controller.deselect();

    //DO THIS: test hover
    /*
    do {
        const Typeset::Marker& anchor = controller.getAnchor();
        view->dispatchHover(anchor.x()+1, anchor.y()+1);
        for(size_t i = 0; i < 10; i++) QCoreApplication::processEvents();
        view->handleKey(Qt::Key_Right, Qt::NoModifier, "");
    } while(!controller.atEnd());
    */

    delete view;
    Program::instance()->freeFileMemory();

    return true;
}

inline bool testIdeInteraction(){
    bool passing = true;

    for(directory_iterator end, dir(BASE_TEST_DIR "/in"); dir != end; dir++)
        passing &= ideCase(dir->path());

    //DO THIS: test error cases
    //for(directory_iterator end, dir(BASE_TEST_DIR "/errors"); dir != end; dir++)
    //    passing &= ideCase(dir->path());

    report("IDE interaction", passing);
    return passing;
}
