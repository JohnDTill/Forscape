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

inline bool testModel(Typeset::Model* input) {
    static Typeset::Editor* view = new Typeset::Editor;
    QEvent leave(QEvent::Type::Leave);
    view->leaveEvent(&leave);
    view->setModel(input);
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

    controller.moveToStartOfDocument();
    size_t n_commands = 0;
    while(!controller.atEnd()) {
        n_commands++;
        view->handleKey(Qt::Key_Delete, Qt::NoModifier, "");
        QCoreApplication::processEvents();
    }

    for(size_t i = 0; i < n_commands; i++){
        view->undo();
        QCoreApplication::processEvents();
    }

    for(size_t i = 0; i < 5; i++){
        view->redo();
        QCoreApplication::processEvents();
    }

    controller.moveToEndOfDocument();
    while(!controller.atStart()){
        view->handleKey(Qt::Key_Backspace, Qt::NoModifier, "");
        QCoreApplication::processEvents();
    }

    return true;
}

inline bool ideCase(const std::filesystem::path& path){
    std::string in = readFile(path.u8string());

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    //view->show(); //This slows the test down CONSIDERABLY
    input->path = std::filesystem::canonical(path);
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();

    bool passing = true;

    for(Typeset::Model* m : Forscape::Program::instance()->allFiles())
        passing &= testModel(m);

    Program::instance()->freeFileMemory();

    return passing;
}

inline bool testIdeDoesNotCrashOnVariousInteractions(){
    bool passing = true;

    for(directory_iterator end, dir(BASE_TEST_DIR "/in"); dir != end; dir++)
        if(std::filesystem::is_regular_file(dir->path()))
            passing &= ideCase(dir->path());

    for(directory_iterator end, dir(BASE_TEST_DIR "/errors"); dir != end; dir++)
        if(std::filesystem::is_regular_file(dir->path()))
            passing &= ideCase(dir->path());

    report("IDE crash test", passing);
    return passing;
}
