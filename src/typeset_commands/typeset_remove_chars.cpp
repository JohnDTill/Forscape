#include "typeset_remove_chars.h"

#include <typeset_controller.h>
#include <typeset_text.h>
#include <hope_unicode.h>

namespace Hope {

namespace Typeset {

RemoveChars::RemoveChars(Text* t, size_t index)
    : t(t), index(index) {
    removed = t->glyphAt(index);
}

bool RemoveChars::isCharacterDeletion() const noexcept {
    return true;
}

void RemoveChars::removeAdditionalChar(){
    size_t glyph_size = glyphSize(t->at(index));
    removed += t->str.substr(index, glyph_size);
    t->str.erase(index, glyph_size);
    t->resize();
}

void RemoveChars::removeCharLeft(){
    size_t old_index = index;
    while(isContinuationCharacter(t->at(--index)));
    size_t glyph_size = old_index - index;
    removed.insert(0, t->str.substr(index, glyph_size));
    t->str.erase(index, glyph_size);
    t->resize();
}

void RemoveChars::undo(Controller& controller){
    t->str.insert(index, removed);
    t->resize();
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index + removed.size();
}

void RemoveChars::redo(Controller& controller){
    t->str.erase(index, removed.size());
    t->resize();
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index;
}

}

}

