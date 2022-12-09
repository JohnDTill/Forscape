#include "typeset_command_pair.h"

namespace Forscape {

namespace Typeset {

CommandPair::CommandPair(Command* a, Command* b)
    : a(a), b(b) {}

CommandPair::~CommandPair(){
    delete a;
    delete b;
}

bool CommandPair::isPairInsertion() const noexcept{
    return b->isCharacterInsertion();
}

void CommandPair::undo(Controller& controller){
    b->undo(controller);
    a->undo(controller);
}

void CommandPair::redo(Controller& controller){
    a->redo(controller);
    b->redo(controller);
}

}

}
