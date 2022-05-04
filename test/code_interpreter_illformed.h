//Load invalid Forscape scripts and make sure the editor doesn't crash

#include <hope_interpreter.h>
#include <hope_parser.h>
#include <hope_scanner.h>
#include "report.h"
#include "typeset.h"

#include <filesystem>

#ifndef HOPE_TYPESET_HEADLESS
#include "../example/exampleWidgetQt/symboltreeview.h"
#endif

using namespace Hope;
using namespace Code;

inline bool testErrorAndNoCrash(const std::string& name){
    std::string in = readFile(BASE_TEST_DIR "/errors/" + name + ".txt");

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    bool had_error = !input->errors.empty();
    if(!had_error){
        input->run();
        had_error = !input->errors.empty();
    }

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef HOPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->static_pass); //Make sure symbol table view doesn't crash
    #endif
    #endif

    delete input;

    assert(had_error);

    return had_error;
}

inline bool testIllFormedPrograms(){
    bool passing = true;

    for(std::filesystem::directory_iterator end, dir(BASE_TEST_DIR "/errors");
         dir != end;
         dir++) {
        const std::filesystem::path& path = dir->path();
        passing &= testErrorAndNoCrash(path.stem().string());
    }

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Ill-formed programs", passing);
    return passing;
}
