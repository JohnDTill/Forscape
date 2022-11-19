#include "code_interpreter.h"
#include "code_interpreter_illformed.h"
#include "code_parser.h"
#include "code_scanner.h"
#include "serial.h"
#include "typeset_control.h"
#include "typeset_loadsave.h"
#include "typeset_mutability.h"
#include "hope_benchmark.h"
#include <typeset_themes.h>

#ifdef TEST_QT
#include <QApplication>
#endif

int main(int argc, char* argv[]){
    #ifdef TEST_QT
    QApplication app_is_prequisite_to_using_QFontDatabase(argc, argv);
    QApplication::processEvents();
    Typeset::Painter::init();
    #else
    (void)argc;
    (void)argv;
    #endif

    #ifndef HOPE_TYPESET_HEADLESS
    Hope::Typeset::setPreset(0);
    #endif

    bool passing = true;

    printf("\n");
    passing &= testSerial();
    passing &= testTypesetLoadSave();
    passing &= testTypesetController();
    passing &= testScanner();
    passing &= testParser();
    passing &= testInterpreter();
    passing &= testIllFormedPrograms();
    passing &= testTypesetMutability();

    if(passing) printf("\nAll passing\n\n");
    else printf("\nTEST(S) FAILED\n\n");

    #ifdef NDEBUG
    if(passing) runBenchmark();
    #endif

    return passing == false;
}
