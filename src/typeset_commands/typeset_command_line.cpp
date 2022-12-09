#include "typeset_command_line.h"

#include <typeset_construct.h>
#include <typeset_controller.h>
#include <typeset_line.h>
#include <typeset_model.h>
#include <typeset_text.h>

#include <iostream>

namespace Forscape {

namespace Typeset {

CommandLine* CommandLine::insert(Text* tL, size_t iL, std::vector<Line*> lines){
    PhraseRight source_fragment(tL, iL);
    PhraseRight insert_fragment(lines.front()->front(), 0);
    delete lines.front()->front();
    lines.front()->giveUpOwnership();
    delete lines.front();
    lines.erase(lines.begin(), lines.begin()+1);

    Line* lL = tL->getParent()->asLine();
    for(size_t i = 0; i < lines.size(); i++){
        lines[i]->parent = lL->parent;
        lines[i]->id = lL->id + 1 + i;
    }

    Text* tR = lines.back()->back();
    size_t iR = tR->numChars();
    source_fragment.writeTo(tR, iR);

    return new CommandLine(true, tL, iL, source_fragment, insert_fragment, lines, tR, iR);
}

CommandLine* CommandLine::remove(Text* tL, size_t iL, Text* tR, size_t iR){
    PhraseRight source_fragment(tR, iR);
    PhraseRight insert_fragment(tL, iL);

    std::vector<Line*> lines;
    Line* lL = tL->getParent()->asLine();
    Line* lR = tR->getParent()->asLine();
    for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted())
        lines.push_back(l);
    lines.push_back(lR);

    return new CommandLine(false, tL, iL, source_fragment, insert_fragment, lines, tR, iR);
}

CommandLine::~CommandLine(){
    if(active){
        insert_fragment.free();
        insert_lines.back()->remove(tR->id);
        for(Line* l : insert_lines) delete l;
    }
}

void CommandLine::undo(Controller& controller){
    if(is_insertion) remove(controller);
    else insert(controller);
}

void CommandLine::redo(Controller& controller){
    if(is_insertion) insert(controller);
    else remove(controller);
}

void CommandLine::insert(Controller& controller){
    //Show subsequent lines
    model()->insert(insert_lines);

    insert_fragment.writeTo(t, iL);
    source_fragment.writeTo(tR, iR);

    controller.active.text = controller.anchor.text = tR;
    controller.active.index = controller.anchor.index = iR;

    active = false;
}

void CommandLine::remove(Controller& controller){
    //Hide subsequent lines
    model()->remove(insert_lines.front()->id, insert_lines.back()->id+1);

    source_fragment.writeTo(t, iL);

    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = iL;

    active = true;
}

Line* CommandLine::baseLine(){
    return t->getParent()->asLine();
}

Model* CommandLine::model(){
    return baseLine()->parent;
}

CommandLine::PhraseRight::PhraseRight(Text* t, size_t index)
    : str(t->view(index, t->numChars()-index)) {
    Line* l = t->getParent()->asLine();
    for(size_t i = t->id+1; i < l->numTexts(); i++){
        constructs.push_back( l->construct(i-1) );
        texts.push_back( l->text(i) );
    }
}

void CommandLine::PhraseRight::writeTo(Text* t, size_t index){
    t->overwrite(index, str);

    Phrase* p = t->getParent();
    p->remove(t->id);
    p->insert(t->id, constructs, texts);
}

void CommandLine::PhraseRight::free(){
    for(Construct* c : constructs) delete c;
    for(Text* t : texts) delete t;
}

CommandLine::CommandLine(bool is_insertion,
                         Text* t,
                         const size_t iL,
                         const PhraseRight& source_fragment,
                         const PhraseRight& insert_fragment,
                         const std::vector<Line*>& insert_lines,
                         Text* tR,
                         const size_t iR)
    : is_insertion(is_insertion),
      t(t),
      iL(iL),
      source_fragment(source_fragment),
      insert_fragment(insert_fragment),
      insert_lines(insert_lines),
      tR(tR),
      iR(iR) {}

}

}
