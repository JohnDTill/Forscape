#ifndef TYPESET_COMMAND_INDENT_H
#define TYPESET_COMMAND_INDENT_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>
#include <vector>

namespace Hope {

namespace Typeset {

class Line;

class CommandIndent : public Command{
public:
    static CommandIndent* indent(Line* first, Line* last);
    static CommandIndent* deindent(Line* first, Line* last);
    const std::vector<uint8_t> spaces;

protected:
    virtual void undo(Controller& c) override final;
    virtual void redo(Controller& c) override final;

private:
    CommandIndent(bool is_insert, Line* first, Line* last, const std::vector<uint8_t>& spaces);
    void insert(Controller& c);
    void remove(Controller& c);

    const bool is_insert;
    Line* first;
    Line* last;
};

}

}

#endif // TYPESET_COMMAND_INDENT_H
