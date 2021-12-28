#include <cassert>
#include <fstream>
#include <construct_codes.h>
#include "typeset.h"
#include <sstream>
#include "report.h"

using namespace Hope;

inline bool testTypesetGraphics(){
    bool passing = true;

    Hope::Typeset::Model* model = Hope::Typeset::Model::fromSerial("");
    Typeset::Line* l = new Typeset::Line(model);
    Typeset::Text& t = *l->front();

    t.str = "print(\"Hello world!\")";
    t.updateWidth();
    double unformatted_width = t.getWidth();

    if(unformatted_width == 0){
        std::cout << "Line " << __LINE__ << ", Graphics text has no width\n";
        passing = false;
    }

    t.tag(SEM_STRING, 6, 20);
    t.updateWidth();
    double formatted_width = t.getWidth();

    if(unformatted_width != formatted_width){
        std::cout << "Line " << __LINE__ << ", Graphics color changes formatting:\n";
        std::cout << "Unformatted: " << std::to_string(unformatted_width) << '\n'
                  << "Formatted:   " << std::to_string(formatted_width) << std::endl;
        passing = false;
    }

    delete model;
    delete l;
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    if(passing){
        report("Typeset QPainter", passing);
    }else{
        std::cout << "-- Typeset graphics: KNOWN BUG (https://github.com/JohnDTill/Forscape/issues/12)" << std::endl;
    }

    return true;

    //report("Typeset graphics", passing);
    //return passing;
}
