#include "typeset_command_phrase.h"

#include <typeset_construct.h>
#include <typeset_controller.h>
#include <typeset_line.h>
#include <typeset_phrase.h>
#include <typeset_text.h>

namespace Forscape {

namespace Typeset {

CommandPhrase* CommandPhrase::insert(Text* tL, size_t iL, Line* l){
    Text* tF = l->front();
    Text* tB = l->back();
    size_t iR = tB->numChars();
    std::string removed = tF->getString();
    tB->append( tL->from(iL) );
    std::vector<Construct*> constructs;
    std::vector<Text*> texts;
    for(Construct* c = tF->nextConstructInPhrase(); c != nullptr; c = c->next()->nextConstructInPhrase()){
        constructs.push_back(c);
        texts.push_back(c->next());
    }

    delete tF;
    l->giveUpOwnership();
    delete l;

    return new CommandPhrase(tL, removed, iR, constructs, texts, true);
}

CommandPhrase* CommandPhrase::remove(Text* tL, size_t iL, Text* tR, size_t iR){
    std::string removed(tL->from(iL));

    std::vector<Construct*> constructs;
    std::vector<Text*> texts;

    Phrase* parent = tL->getParent();
    for(size_t i = tL->id; i < tR->id; i++){
        constructs.push_back(parent->construct(i));
        texts.push_back(parent->text(i+1));
    }

    return new CommandPhrase(tL, removed, iR, constructs, texts, false);
}

CommandPhrase::~CommandPhrase(){
    if(active){
        for(Construct* c : constructs) delete c;
        for(Text* t : texts) delete t;
    }
}

void CommandPhrase::undo(Controller& controller){
    if(is_insertion) remove(controller);
    else insert(controller);
}

void CommandPhrase::redo(Controller& controller){
    if(is_insertion) insert(controller);
    else remove(controller);
}

CommandPhrase::CommandPhrase(Text* tL, const std::string& removed, size_t iR, const std::vector<Construct*>& c, const std::vector<Text*>& t, bool is_insertion)
    : tL(tL), removed(removed), iR(iR), constructs(c), texts(t), is_insertion(is_insertion) {}

void CommandPhrase::insert(Controller& controller){
    size_t iL = tL->numChars() - (texts.back()->numChars()-iR);
    tL->overwrite(iL, removed);

    tL->getParent()->insert(tL->id, constructs, texts);

    controller.active.text = controller.anchor.text = texts.back();
    controller.active.index = controller.anchor.index = iR;

    active = false;
}

void CommandPhrase::remove(Controller& controller){
    size_t iL = tL->numChars()-removed.size();
    tL->overwrite(iL, texts.back()->from(iR));

    tL->getParent()->remove(tL->id, texts.back()->id);

    controller.active.text = controller.anchor.text = tL;
    controller.active.index = controller.anchor.index = iL;

    active = true;
}

}

}
