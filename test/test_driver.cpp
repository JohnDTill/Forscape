#include "code_interpreter.h"
#include "code_interpreter_illformed.h"
#include "code_parser.h"
#include "code_scanner.h"
#include "serial.h"
#include "test_convert_to_unicode.h"
#include "test_keywords.h"
#include "test_unicode.h"
#include "typeset_loadsave.h"
#include "typeset_control.h"
#include "typeset_mutability.h"
#include "forscape_benchmark.h"
#include <typeset_themes.h>

#ifdef TEST_QT
#include "test_highlighting.h"
#include "test_ide_interaction.h"
#include "test_suggestions.h"
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

    #ifndef FORSCAPE_TYPESET_HEADLESS
    Forscape::Typeset::setPreset(0);
    #endif

    bool passing = true;

    printf("\n");

    passing &= testUnicode();
    passing &= testSerial();
    passing &= testTypesetLoadSave();
    passing &= testTypesetController();
    passing &= testConvertToUnicode();
    passing &= testKeywords();
    passing &= testScanner();
    passing &= testParser();
    passing &= testInterpreter();
    passing &= testIllFormedPrograms();
    passing &= testTypesetMutability();

    #ifdef TEST_QT
    passing &= testIdeFeatures();
    passing &= testSuggestions();
    passing &= testIdeDoesNotCrashOnVariousInteractions();
    #endif

    if(passing) printf("\nAll passing\n\n");
    else printf("\nTEST(S) FAILED\n\n");

    if(passing) runBenchmark();

    return passing == false;
}
