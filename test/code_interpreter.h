#include <forscape_interpreter.h>
#include <forscape_parser.h>
#include <forscape_program.h>
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
#include <symboltreeview.h>
#endif

using namespace Forscape;
using namespace Code;

inline bool testExpression(const std::string& in, const std::string& expect){
    Typeset::Model* input = Typeset::Model::fromSerial("print(" + in + ")");
    input->postmutate();
    std::string str = input->run();

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef FORSCAPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->static_pass);
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

inline bool testCase(const std::string& name){
    std::string file_name = BASE_TEST_DIR "/in/" + name + ".π";
    std::string in = readFile(file_name);
    std::string out = readFile(BASE_TEST_DIR "/out/" + name + ".π");

    Typeset::Model* input = Typeset::Model::fromSerial(in);
    input->path = std::filesystem::canonical(std::filesystem::u8path(file_name));
    Forscape::Program::instance()->setProgramEntryPoint(input->path, input);
    input->postmutate();
    std::string str = input->run();

    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #ifndef FORSCAPE_TYPESET_HEADLESS
    SymbolTreeView view(input->symbol_builder.symbol_table, input->static_pass); //Make sure symbol table view doesn't crash
    #endif
    #endif

    Program::instance()->freeFileMemory();

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

    for(directory_iterator end, dir(BASE_TEST_DIR "/in"); dir != end; dir++)
        passing &= testCase(dir->path().stem().string());

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Code interpreter", passing);
    return passing;
}
