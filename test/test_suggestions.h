#include <forscape_program.h>
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

static void reportFailure(
        const std::string& test,
        const std::vector<std::string>& expected,
        const std::vector<std::string>& actual){
    std::cout << test << " suggestions failing:\n";
    std::cout << "---EXPECTED---\n";
    for(const std::string& str : expected) std::cout << str << "\n";
    std::cout << "---ACTUAL---\n";
    for(const std::string& str : actual) std::cout << str << "\n";
    std::cout.flush();
}

inline bool testLocalSuggestions(){
    bool passing = true;

    Typeset::Model* input = Typeset::Model::fromSerial(
        "who = 1\n"
        "what = 2\n"
        "when = 3\n"
        "why = 4\n"
        "how = 5\n"
    );
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();
    Typeset::Editor* view = new Typeset::Editor;
    view->setModel(input);

    view->handleKey(Qt::Key_W, Qt::NoModifier, "w"); QCoreApplication::processEvents();

    std::vector<std::string> expected = {
        "what",
        "when",
        "who",
        "why",
        "where",
        "while",
    };

    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Local", expected, view->suggestions);
    }

    view->handleKey(Qt::Key_H, Qt::NoModifier, "h"); QCoreApplication::processEvents();

    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Local", expected, view->suggestions);
    }

    view->handleKey(Qt::Key_E, Qt::NoModifier, "e"); QCoreApplication::processEvents();

    expected = {
        "when",
        "where",
    };

    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Local", expected, view->suggestions);
    }

    Program::instance()->freeFileMemory();
    delete view;

    return passing;
}

inline bool testFileSuggestions(){
    Typeset::Model* input = Typeset::Model::fromSerial("import");
    std::filesystem::path mocked_file = std::filesystem::u8path(BASE_TEST_DIR "/in/hello_world.π");
    input->path = std::filesystem::canonical(mocked_file); //Just get suggestions from "in"
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();
    Typeset::Editor* view = new Typeset::Editor;
    view->setModel(input);

    view->handleKey(Qt::Key_Space, Qt::NoModifier, " "); QCoreApplication::processEvents();

    std::vector<std::string> expected;
    for(directory_iterator end, dir(BASE_TEST_DIR "/in"); dir != end; dir++)
        if(std::filesystem::is_regular_file(dir->path()))
            expected.push_back(dir->path().filename().u8string());
    std::sort(expected.begin(), expected.end());

    if(view->suggestions != expected){
        reportFailure("File", expected, view->suggestions);
        return false;
    }

    view->handleKey(Qt::Key_Period, Qt::NoModifier, "."); QCoreApplication::processEvents();
    view->handleKey(Qt::Key_Period, Qt::NoModifier, "."); QCoreApplication::processEvents();
    view->handleKey(Qt::Key_Slash, Qt::NoModifier, "/"); QCoreApplication::processEvents();

    #ifdef __linux__
    #define SLASH "/"
    #else
    #define SLASH "\\"
    #endif

    expected.clear();
    for(directory_iterator end, dir(BASE_TEST_DIR); dir != end; dir++)
        if(std::filesystem::is_regular_file(dir->path()) && dir->path() != mocked_file)
            expected.push_back(".." SLASH + dir->path().filename().u8string());
    std::sort(expected.begin(), expected.end());

    if(view->suggestions != expected){
        reportFailure("File", expected, view->suggestions);
        return false;
    }

    //EVENTUALLY: recommend files in "from _"

    Program::instance()->freeFileMemory();
    delete view;

    return true;
}

inline bool testModuleSuggestions(){
    bool passing = true;

    Typeset::Model* input = Typeset::Model::fromSerial(
        "import bob_closure_a.π as module\n"
        "module");
    std::filesystem::path mocked_file = std::filesystem::u8path(BASE_TEST_DIR "/in/hello_world.π");
    input->path = std::filesystem::canonical(mocked_file); //Just get suggestions from "in"
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();
    Typeset::Editor* view = new Typeset::Editor;
    view->setModel(input);

    view->handleKey(Qt::Key_Period, Qt::NoModifier, "."); QCoreApplication::processEvents();

    std::vector<std::string> expected = {
        "bagel",
        "doughnut",
        "makeClosure",
    };

    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Module", expected, view->suggestions);
    }

    view->handleKey(Qt::Key_D, Qt::NoModifier, "d"); QCoreApplication::processEvents();

    expected = {
        "doughnut",
    };

    view->handleKey(Qt::Key_Home, Qt::ShiftModifier, ""); QCoreApplication::processEvents();
    view->paste("from bob_closure_a import");
    view->handleKey(Qt::Key_Space, Qt::NoModifier, " "); QCoreApplication::processEvents();

    expected = {
        "bagel",
        "doughnut",
        "makeClosure",
    };

    /* EVENTUALLY: recommend "from _ import" without first letter of variable
    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Module", expected, view->suggestions);
    }
    */

    view->handleKey(Qt::Key_D, Qt::NoModifier, "d"); QCoreApplication::processEvents();

    expected = {
        "doughnut",
    };

    if(passing && view->suggestions != expected){
        passing = false;
        reportFailure("Module", expected, view->suggestions);
    }

    return passing;
}

inline bool testSuggestions(){
    bool passing = true;

    passing &= testLocalSuggestions();
    passing &= testFileSuggestions();
    passing &= testModuleSuggestions();

    report("IDE suggestions", passing);

    return passing;
}
