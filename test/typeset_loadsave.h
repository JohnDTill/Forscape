#include <cassert>
#include <fstream>
#include "typeset.h"
#include <sstream>
#include <string>
#include "report.h"

using namespace Forscape;

inline bool testTypesetLoadSave(){
    bool passing = true;

    std::string input = readFile(BASE_TEST_DIR "/serial_valid.π");
    Forscape::Typeset::Model* model = Forscape::Typeset::Model::fromSerial(input);
    std::string serial = model->toSerial();

    if(serial != input){
        printf("Serial=>Typeset=>Serial inconsistent\nIn:  %s\n\nOut: %s\n", input.c_str(), serial.c_str());
        passing = false;
    }

    delete model;

    #ifndef NDEBUG
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }
    #endif

    report("Typeset load/save", passing);
    return passing;
}
