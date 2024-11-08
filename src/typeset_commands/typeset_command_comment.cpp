#include "typeset_command_comment.h"

#include "typeset_controller.h"
#include "typeset_line.h"

namespace Forscape {

namespace Typeset {

CommandComment* CommandComment::comment(Line* first, Line* last) {
    return new CommandComment(first, last);
}

bool CommandComment::isCommented() const noexcept {
    return is_commented;
}

CommandComment::CommandComment(Line* first, Line* last) : first(first), last(last) {
    is_commented = true;
    for(Line* l = first; is_commented && l != last->next(); l = l->next())
        is_commented &= (l->front()->getString().substr(0, 2) == "//");
}

void CommandComment::undo(Controller& c) {
    if(is_commented) remove(c);
    else insert(c);
}

void CommandComment::redo(Controller& c) {
    undo(c);
}

void CommandComment::insert(Controller& c) {
    is_commented = true;
    for(Line* l = first; l != last->next(); l = l->next())
        l->front()->prepend("//");

    c.setBothToFrontOf(last->front());
}

void CommandComment::remove(Controller& c) {
    is_commented = false;
    for(Line* l = first; l != last->next(); l = l->next())
        l->front()->removeComment();

    c.setBothToFrontOf(last->front());
}

}

}
