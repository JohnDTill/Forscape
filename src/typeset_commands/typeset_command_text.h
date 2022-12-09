#ifndef TYPESET_COMMAND_TEXT_H
#define TYPESET_COMMAND_TEXT_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>

namespace Forscape {

namespace Typeset {

class Text;

class CommandText : public Command{
public:
    static CommandText* insert(Text* t, size_t index, const std::string& str);
    static CommandText* remove(Text* t, size_t index, size_t sze);
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

public:
    Text* t;
    size_t index;
    std::string removed;

private:
    CommandText(Text* t, size_t index, const std::string& str, bool is_insertion);
    void insert(Controller& controller);
    void remove(Controller& controller);
    const bool is_insertion;
};

}

}

#endif // TYPESET_COMMAND_TEXT_H
