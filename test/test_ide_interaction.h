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

    static Typeset::Editor* view = new Typeset::Editor;
    QEvent leave(QEvent::Type::Leave);
    view->leaveEvent(&leave);

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    view->setModel(input);
    //view->show(); //This slows the test down CONSIDERABLY
    input->path = std::filesystem::canonical(path);
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();

    Typeset::Controller& controller = view->getController();
    controller.moveToStartOfDocument();
    while(!controller.atEnd()){ view->handleKey(Qt::Key_Right, Qt::NoModifier, ""); QCoreApplication::processEvents(); }
    while(!controller.atStart()){ view->handleKey(Qt::Key_Left, Qt::NoModifier, ""); QCoreApplication::processEvents(); }
    while(!controller.atEnd()){ view->handleKey(Qt::Key_Right, Qt::ShiftModifier, ""); QCoreApplication::processEvents(); }
    controller.deselect();
    while(!controller.atStart()){ view->handleKey(Qt::Key_Left, Qt::ShiftModifier, ""); QCoreApplication::processEvents(); }
    controller.deselect();

    while(!controller.atEnd()){
        const Typeset::Marker& anchor = controller.getAnchor();
        view->resolveTooltip(anchor.x()+1, anchor.y()+1);
        for(size_t i = 0; i < 10; i++) QCoreApplication::processEvents();

        view->resolveRightClick(anchor.x()+1, anchor.y()+1, 0, 0);
        for(size_t i = 0; i < 10; i++) QCoreApplication::processEvents();

        view->handleKey(Qt::Key_Right, Qt::NoModifier, "");
    }

    Program::instance()->freeFileMemory();

    return true;
}

inline bool testIdeInteraction(){
    bool passing = true;

    for(directory_iterator end, dir(BASE_TEST_DIR "/in"); dir != end; dir++)
        passing &= ideCase(dir->path());

    for(directory_iterator end, dir(BASE_TEST_DIR "/errors"); dir != end; dir++)
        passing &= ideCase(dir->path());

    report("IDE interaction", passing);
    return passing;
}
