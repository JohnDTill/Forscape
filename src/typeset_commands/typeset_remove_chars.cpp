#include "typeset_remove_chars.h"

#include <hope_unicode.h>
#include <typeset_controller.h>
#include <typeset_text.h>

namespace Hope {

namespace Typeset {

RemoveChars::RemoveChars(Text* t, size_t index)
    : t(t), index(index) {
    removed = t->graphemeAt(index);
}

bool RemoveChars::isCharacterDeletion() const noexcept {
    return true;
}

void RemoveChars::removeAdditionalChar(){
    std::string_view additional = t->graphemeAt(index);
    removed += additional;
    t->erase(index, std::string(additional));
}

void RemoveChars::removeCharLeft(){
    size_t glyph_size = graphemeSizeLeft(t->getString(), index);
    index -= glyph_size;
    removed.insert(0, t->view(index, glyph_size));
    t->erase(index, std::string(t->view(index, glyph_size)));
}

void RemoveChars::undo(Controller& controller){
    t->insert(index, removed);
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index + removed.size();
}

void RemoveChars::redo(Controller& controller){
    t->erase(index, removed);
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index;
}

}

}

