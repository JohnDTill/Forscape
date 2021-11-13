#include "typeset_command_list.h"

namespace Hope {

namespace Typeset {


CommandList::~CommandList(){
    for(Command* cmd : cmds) delete cmd;
}

void CommandList::undo(Controller& controller){
    for(auto it = cmds.rbegin(); it != cmds.rend(); it++){
        Command* cmd = *it;
        cmd->undo(controller);
    }
}

void CommandList::redo(Controller& controller){
    for(Command* cmd : cmds) cmd->redo(controller);
}

}

}
