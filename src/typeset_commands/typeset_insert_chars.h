#ifndef TYPESET_INSERT_CHARS_H
#define TYPESET_INSERT_CHARS_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>

namespace Hope {

namespace Typeset {

class Text;

class InsertChars : public Command{
public:
    InsertChars(Text* t, size_t index, const std::string& inserted);
    virtual bool isCharacterInsertion() const noexcept override;
    void insertAdditionalChar(const std::string& str);
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

public:
    Text* t;
    size_t index;
    std::string inserted;
};

}

}

#endif // TYPESET_INSERT_CHARS_H
