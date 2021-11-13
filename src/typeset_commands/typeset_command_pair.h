#ifndef TYPESET_COMMAND_PAIR_H
#define TYPESET_COMMAND_PAIR_H

#include <typeset_command.h>

namespace Hope {

namespace Typeset {

class CommandPair : public Command{
public:
    CommandPair(Command* a, Command* b);
    virtual ~CommandPair();
    virtual bool isPairInsertion() const noexcept override;
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

    Command* a;
    Command* b;
};

}

}

#endif // TYPESET_COMMAND_PAIR_H
