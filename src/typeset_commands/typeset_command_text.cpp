#include "typeset_command_text.h"

#include <typeset_controller.h>
#include <typeset_text.h>

namespace Hope {

namespace Typeset {

CommandText* CommandText::insert(Text* t, size_t index, const std::string& str){
    return new CommandText(t, index, str, true);
}

CommandText* CommandText::remove(Text* t, size_t index, size_t sze){
    std::string removed(t->view(index, sze));
    return new CommandText(t, index, removed, false);
}

void CommandText::undo(Controller& controller){
    if(is_insertion) remove(controller);
    else insert(controller);
}

void CommandText::redo(Controller& controller){
    if(is_insertion) insert(controller);
    else remove(controller);
}

CommandText::CommandText(Text* t, size_t index, const std::string& str, bool is_insertion)
    : t(t), index(index), removed(str), is_insertion(is_insertion) {}

void CommandText::insert(Controller& controller){
    t->insert(index, removed);
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index + removed.size();
}

void CommandText::remove(Controller& controller){
    t->erase(index, removed.size());
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index;
}

}

}
