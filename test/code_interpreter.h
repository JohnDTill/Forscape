#include <hope_interpreter.h>
#include <hope_parser.h>
#include <hope_scanner.h>
#include "report.h"
#include "typeset.h"

#ifndef __MINGW32__
#include <filesystem>
#else
#include <QDirIterator>
//std::filesystem isn't working with mingw
//It is the preferred solution
#endif

using namespace Hope;
using namespace Code;

inline bool testExpression(const std::string& in, const std::string& expect){
    Typeset::Model* input = Typeset::Model::fromSerial("print(" + in + ")");
    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #endif
    Typeset::Model* output = input->run(nullptr);
    std::string str = output->toSerial();

    delete input;
    delete output;

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
    #ifndef NDEBUG
    input->parseTreeDot(); //Make sure dot generation doesn't crash
    #endif
    Typeset::Model* output = input->run(nullptr);
    std::string str = output->toSerial();

    delete input;
    delete output;

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

    #ifndef __MINGW32__
    for(std::filesystem::directory_iterator end, dir(base_test_path + "/in");
         dir != end;
         dir++) {
        const std::filesystem::path& path = dir->path();
        passing &= testCase(path.stem().string());
    }
    #else
    QString path = QString::fromStdString(base_test_path + "/in");
    QDirIterator it(path);
    while (it.hasNext()) {
         QFileInfo info(it.next());
         if(!info.isFile()) continue;
         passing &= testCase(info.baseName().toStdString());
    }
    #endif

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Code interpreter", passing);
    return passing;
}
