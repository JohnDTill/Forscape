#include "code_interpreter.h"
#include "code_parser.h"
#include "code_scanner.h"
#include "serial.h"
#include "typeset_control.h"
#include "typeset_loadsave.h"
#include "typeset_mutability.h"
#include "hope_benchmark.h"

#ifdef TEST_QT
#include <QApplication>
#include "typeset_graphics.h"
#include "typeset_qpainter.h"
#endif

int main(int argc, char* argv[]){
    #ifdef TEST_QT
    QApplication app_is_prequisite_to_using_QFontDatabase(argc, argv);
    QApplication::processEvents();
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
    passing &= testTypesetQPainter();
    #endif

    if(passing) printf("\nAll passing\n\n");
    else printf("\nTEST(S) FAILED\n\n");

    if(passing) runBenchmark();

    return passing == false;
}
