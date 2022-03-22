#include "code_interpreter.h"
#include "code_parser.h"
#include "code_scanner.h"
#include "serial.h"
#include "typeset_control.h"
#include "typeset_loadsave.h"
#include "typeset_mutability.h"
#include "hope_benchmark.h"
#include <hope_logging.h>
#include <typeset_themes.h>

#ifdef TEST_QT
#include <QApplication>
#include "typeset_graphics.h"
#endif

int main(int argc, char* argv[]){
    #ifdef TEST_QT
    QApplication app_is_prequisite_to_using_QFontDatabase(argc, argv);
    QApplication::processEvents();
    Typeset::Painter::init();
    #endif

    Hope::initLogging();
    Hope::logger->info("TEST_START");
    #ifndef HOPE_TYPESET_HEADLESS
    Hope::Typeset::setDefaultTheme();
    #endif

    bool passing = true;

    printf("\n");
    passing &= testSerial();
    passing &= testTypesetLoadSave();
    passing &= testTypesetController();
    passing &= testScanner();
    passing &= testParser();
    passing &= testInterpreter();
    passing &= testTypesetMutability();
    #ifdef TEST_QT
    passing &= testTypesetGraphics();
    #endif

    if(passing) printf("\nAll passing\n\n");
    else printf("\nTEST(S) FAILED\n\n");

    if(passing) runBenchmark();

    Hope::logger->info("TEST_END");

    return passing == false;
}
