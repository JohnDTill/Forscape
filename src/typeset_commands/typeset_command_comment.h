#ifndef TYPESET_COMMAND_COMMENT_H
#define TYPESET_COMMAND_COMMENT_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>
#include <vector>

namespace Forscape {

namespace Typeset {

class Line;

class CommandComment : public Command{
public:
    static CommandComment* comment(Line* first, Line* last);
    bool isCommented() const noexcept;

protected:
    virtual void undo(Controller& c) override final;
    virtual void redo(Controller& c) override final;

private:
    CommandComment(Line* first, Line* last);
    void insert(Controller& c);
    void remove(Controller& c);

    bool is_commented;
    Line* first;
    Line* last;
};

}

}

#endif // TYPESET_COMMAND_COMMENT_H
