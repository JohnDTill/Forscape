#include <cassert>
#include <fstream>
#include <construct_codes.h>
#include "typeset.h"
#include <sstream>
#include "report.h"

static size_t glyphSize(char ch) noexcept{
    if(ch >> 7 == 0) return 1;
    assert((ch & (1 << 6)) != 0);
    if((ch & (1 << 5)) == 0) return 2;
    if((ch & (1 << 4)) == 0) return 3;
    return 4;
}

using namespace Hope;

inline bool testTypesetController(){
    bool passing = true;

    std::string input = readFile("serial_valid.txt");
    Hope::Typeset::Model* model = Hope::Typeset::Model::fromSerial(input);
    Typeset::Controller controller(model);

    controller.selectAll();
    while(!controller.atEnd()) controller.selectNextChar();
    if(controller.selectedText() != input){
        printf("Select all does not match source");
        passing = false;
    }

    controller.moveToStartOfDocument();
    size_t curr = 0;

    while(!controller.atEnd()){
        char ch = input[curr];

        if(ch == OPEN){
            curr++;
            ch = input[curr++];
            if(ch == CASES) curr++;
            else if(ch == MATRIX) curr += 2;
            controller.moveToNextChar();
        }else if(ch == CLOSE || ch == '\n'){
            curr++;
            controller.moveToNextChar();
        }else{
            curr += glyphSize(ch);
            controller.selectNextChar();

            if(controller.selectedText().front() != ch){
                printf("Character comparison did not match");
                passing = false;
                break;
            }

            controller.moveToNextChar();
        }
    }

    delete model;
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Typeset controller", passing);
    return passing;
}
