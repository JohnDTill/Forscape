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

inline bool testExpression(const std::string& in, const std::string& expect){
    Typeset::Model* input = Typeset::Model::fromSerial("print(" + in + ")");
    std::string str = input->run();

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef HOPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->type_resolver.ts);
    #endif
    #endif

    delete input;

    if(str != expect){
        std::cout << "Interpretation case failed.\n"
                     "Expression:    " << in << "\n"
                     "Eval expected: " << expect << "\n"
                     "Eval actual:   " << str << "\n" << std::endl;

        return false;
    }else{
        return true;
    }
}

static const std::string base_test_path = "../test/interpreter_scripts";

inline bool testCase(const std::string& name){
    std::string in = readFile(base_test_path + "/in/" + name + ".txt");
    std::string out = readFile(base_test_path + "/out/" + name + ".txt");

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    std::string str = input->run();

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef HOPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->type_resolver.ts); //Make sure symbol table view doesn't crash
    #endif
    #endif

    delete input;

    if(str != out){
        std::cout << "Interpretation case \"" << name << "\" failed.\n"
                     "Source:    " << in << "\n"
                     "Eval expected: " << out << "\n"
                     "Eval actual:   " << str << "\n" << std::endl;

        return false;
    }else{
        return true;
    }
}

inline bool testInterpreter(){
    bool passing = true;

    passing &= testExpression("\"Hello world!\"", "Hello world!");
    passing &= testExpression("2", "2");
    passing &= testExpression("1001", "1001");
    passing &= testExpression("1 + 1", "2");
    passing &= testExpression("1001 + 1001", "2002");
    passing &= testExpression("2 * 2", "4");
    passing &= testExpression("1001 1001", "1001");
    passing &= testExpression("2 1001", "2002");
    passing &= testExpression("(2)", "2");
    passing &= testExpression("2(1 - 3)", "-4");
    passing &= testExpression("1212", "5");
    passing &= testExpression("12(1 + 3)", "2");
    passing &= testExpression("2^2", "4");
    passing &= testExpression("4^0.5", "2");

    for(std::filesystem::directory_iterator end, dir(base_test_path + "/in");
         dir != end;
         dir++) {
        const std::filesystem::path& path = dir->path();
        passing &= testCase(path.stem().string());
    }

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Code interpreter", passing);
    return passing;
}
