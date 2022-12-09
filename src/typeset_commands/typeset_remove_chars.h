#ifndef TYPESET_REMOVE_CHARS_H
#define TYPESET_REMOVE_CHARS_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>

namespace Forscape {

namespace Typeset {

class Text;

class RemoveChars : public Command{
public:
    RemoveChars(Text* t, size_t index);
    virtual bool isCharacterDeletion() const noexcept override;
    void removeAdditionalChar();
    void removeCharLeft();
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

public:
    Text* t;
    size_t index;
    std::string removed;
};

}

}

#endif // TYPESET_REMOVE_CHARS_H
