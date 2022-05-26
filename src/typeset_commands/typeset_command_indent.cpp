#include "typeset_command_indent.h"

#include "typeset_controller.h"
#include <typeset_line.h>
#include <cassert>

namespace Hope {

namespace Typeset {

CommandIndent* CommandIndent::indent(Line* first, Line* last){
    std::vector<uint8_t> spaces;

    for(Line* l = first; l != last->next(); l = l->next()){
        size_t added = Controller::INDENT_SIZE;
        spaces.push_back( static_cast<uint8_t>(added) );
    }

    return new CommandIndent(true, first, last, spaces);
}

CommandIndent* CommandIndent::deindent(Line* first, Line* last){
    std::vector<uint8_t> spaces;
    bool no_change = true;

    for(Line* l = first; l != last->next(); l = l->next()){
        size_t leading = l->leadingSpaces();
        if(leading){
            size_t to_remove = std::min(leading, Controller::INDENT_SIZE);
            spaces.push_back( static_cast<uint8_t>(to_remove) );
            no_change = false;
        }else{
            spaces.push_back(0);
        }
    }

    if(no_change) return nullptr;
    else return new CommandIndent(false, first, last, spaces);
}

void CommandIndent::undo(Controller& c){
    if(is_insert) remove(c);
    else insert(c);
}

void CommandIndent::redo(Controller& c){
    if(is_insert) insert(c);
    else remove(c);
}

CommandIndent::CommandIndent(bool is_insert, Line* first, Line* last, const std::vector<uint8_t>& spaces)
    : spaces(spaces), is_insert(is_insert), first(first), last(last) {}

void CommandIndent::insert(Controller& c){
    size_t i = 0;
    for(Line* l = first; l != last->next(); l = l->next()){
        l->front()->prependSpaces(spaces[i++]);
        #ifndef HOPE_TYPESET_HEADLESS
        l->front()->resize();
        #endif
    }

    c.setBothToFrontOf(last->front());
}

void CommandIndent::remove(Controller& c){
    size_t i = 0;
    for(Line* l = first; l != last->next(); l = l->next()){
        l->front()->removeLeadingSpaces(spaces[i++]);
        #ifndef HOPE_TYPESET_HEADLESS
        l->front()->resize();
        #endif
    }

    c.setBothToFrontOf(last->front());
}



}

}
