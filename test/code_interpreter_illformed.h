//Load invalid Forscape scripts and make sure the editor doesn't crash

#include <forscape_interpreter.h>
#include <forscape_parser.h>
#include <forscape_scanner.h>
#include "report.h"
#include "typeset.h"

#if !defined(__MINGW32__) || (__MINGW64_VERSION_MAJOR > 8)
#include <filesystem>
using std::filesystem::directory_iterator;
#else
#error std::filesystem is borked for MinGW versions before 9.0
#endif

#ifndef FORSCAPE_TYPESET_HEADLESS
#include "../example/exampleWidgetQt/symboltreeview.h"
#endif

using namespace Forscape;
using namespace Code;

inline bool testErrorAndNoCrash(const std::string& name){
    std::string in = readFile(BASE_TEST_DIR "/errors/" + name + ".π");

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    bool had_error = !input->errors.empty();
    if(!had_error){
        input->run();
        had_error = !input->errors.empty();
    }

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef FORSCAPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->static_pass); //Make sure symbol table view doesn't crash
    #endif
    #endif

    delete input;

    assert(had_error);

    return had_error;
}

inline bool testIllFormedPrograms(){
    bool passing = true;

    for(directory_iterator end, dir(BASE_TEST_DIR "/errors"); dir != end; dir++)
        passing &= testErrorAndNoCrash(dir->path().stem().string());

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Ill-formed programs", passing);
    return passing;
}
