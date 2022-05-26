#include "typeset_insert_chars.h"

#include <typeset_controller.h>
#include <typeset_text.h>

namespace Hope {

namespace Typeset {

InsertChars::InsertChars(Text* t, size_t index, const std::string& inserted)
    : t(t), index(index), inserted(inserted){}

bool InsertChars::isCharacterInsertion() const noexcept{
    return true;
}

void InsertChars::insertAdditionalChar(const std::string& str){
    t->insert(index+inserted.size(), str);
    inserted += str;
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
}

void InsertChars::undo(Controller& controller){
    t->erase(index, inserted.size());
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index;
}

void InsertChars::redo(Controller& controller){
    t->insert(index, inserted);
    #ifndef HOPE_TYPESET_HEADLESS
    t->resize();
    #endif
    controller.active.text = controller.anchor.text = t;
    controller.active.index = controller.anchor.index = index+inserted.size();
}

}

}
