#include "code_interpreter.h"
#include "code_parser.h"
#include "code_scanner.h"
#include "serial.h"
#include "typeset_control.h"
#include "typeset_graphics.h"
#include "typeset_loadsave.h"
#include "typeset_mutability.h"
#include "typeset_qpainter.h"
#include "hope_benchmark.h"

#include <QApplication>

int main(int argc, char* argv[]){
    QApplication app_is_prequisite_to_using_QFontDatabase(argc, argv);
    QApplication::processEvents();

    bool passing = true;

    printf("\n");
    passing &= testSerial();
    passing &= testTypesetLoadSave();
    passing &= testTypesetController();
    passing &= testScanner();
    passing &= testParser();
    passing &= testInterpreter();
    passing &= testTypesetGraphics();
    passing &= testTypesetMutability();
    passing &= testTypesetQPainter();

    if(passing) printf("\nAll passing\n\n");
    else printf("\nTEST(S) FAILED\n\n");

    if(passing) runBenchmark();

    return passing == false;
}
