#include "typeset_remove_chars.h"

#include <typeset_controller.h>
#include <typeset_text.h>
#include <hope_unicode.h>

#include <iostream>

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
    t->erase(index, additional.size());
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
}

void RemoveChars::removeCharLeft(){
    size_t glyph_size = graphemeSizeLeft(t->getString(), index);
    index -= glyph_size;
    removed.insert(0, t->view(index, glyph_size));
    t->erase(index, glyph_size);
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
}

void RemoveChars::undo(Controller& controller){
    t->insert(index, removed);
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index + removed.size();
}

void RemoveChars::redo(Controller& controller){
    t->erase(index, removed.size());
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index;
}

}

}

