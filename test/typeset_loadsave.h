#include <cassert>
#include <fstream>
#include "typeset.h"
#include <sstream>
#include <string>
#include "report.h"

using namespace Hope;

inline bool testTypesetLoadSave(){
    bool passing = true;

    std::string input = readFile("serial_valid.txt");
    Hope::Typeset::Model* model = Hope::Typeset::Model::fromSerial(input);
    std::string serial = model->toSerial();

    if(serial != input){
        printf("Serial=>Typeset=>Serial inconsistent\nIn:  %s\n\nOut: %s\n", input.c_str(), serial.c_str());
        passing = false;
    }

    delete model;
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Typeset load/save", passing);
    return passing;
}
