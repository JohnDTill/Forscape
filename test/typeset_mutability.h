#include <cassert>
#include "typeset.h"
#include <string>
#include "report.h"

using namespace Hope;

inline bool testTypesetMutability(){
    bool passing = true;

    Typeset::Model* model = Typeset::Model::fromSerial("");
    Typeset::Controller controller(model);

    controller.insertText("c");
    controller.insertText("a");
    controller.insertText("t");

    if(model->toSerial() != "cat"){
        printf("Expected \"cat\"\n");
        passing = false;
    }

    model->undo(controller);

    if(model->toSerial() != ""){
        printf("Text insertion not undone\n");
        passing = false;
    }

    model->redo(controller);

    if(model->toSerial() != "cat"){
        printf("Text insertion not redone\n");
        passing = false;
    }

    controller.backspace();
    controller.insertText("n");

    if(model->toSerial() != "can"){
        printf("Modification not successful\n");
        passing = false;
    }

    controller.backspaceWord();

    if(model->toSerial() != ""){
        printf("Backspace word failed\n");
        passing = false;
    }

    model->undo(controller);

    if(model->toSerial() != "can"){
        printf("Undo backspace word failed\n");
        passing = false;
    }

    controller.moveToPrevChar();
    controller.newline();

    if(model->toSerial() != "ca\nn"){
        printf("Newline failed\n");
        passing = false;
    }

    model->undo(controller);

    if(model->toSerial() != "can"){
        printf("Undo newline failed\n");
        passing = false;
    }

    controller.selectAll();
    controller.insertText("The delta function is: ");

    if(model->toSerial() != "The delta function is: "){
        printf("Input over selection failed\n");
        passing = false;
    }

    model->undo(controller);

    if(model->toSerial() != "can"){
        printf("Undo input over selection failed\n");
        passing = false;
    }

    model->redo(controller);

    if(model->toSerial() != "The delta function is: "){
        printf("Redo input over selection failed\n");
        passing = false;
    }

    controller.insertSerial("δ(x) = 1x = 00x ≠ 0");

    if(model->toSerial() != "The delta function is: δ(x) = 1x = 00x ≠ 0"){
        printf("Insert serial failed\n");
        passing = false;
    }

    model->undo(controller);
    model->undo(controller);

    if(model->toSerial() != "can"){
        printf("Undo insert serial failed\n");
        passing = false;
    }

    delete model;
    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Typeset mutability", passing);
    return passing;
}
