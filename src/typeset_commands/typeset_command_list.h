#ifndef TYPESET_COMMAND_LIST_H
#define TYPESET_COMMAND_LIST_H

#include <typeset_command.h>
#include <vector>

namespace Hope {

namespace Typeset {

class CommandList : public Command{
public:
    virtual ~CommandList();
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

    std::vector<Command*> cmds;
};

}

}

#endif // TYPESET_COMMAND_LIST_H
