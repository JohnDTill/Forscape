#ifndef TYPESET_COMMAND_H
#define TYPESET_COMMAND_H

namespace Hope {

namespace Typeset {

class Controller;

class Command{
public:
    virtual ~Command(){}
    virtual void undo(Controller& controller) = 0;
    virtual void redo(Controller& controller) = 0;
    virtual bool isCharacterDeletion() const noexcept {return false;}
    virtual bool isCharacterInsertion() const noexcept {return false;}
    virtual bool isPairInsertion() const noexcept {return false;}
};

}

}

#endif // TYPESET_COMMAND_H
