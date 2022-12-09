#ifndef TYPESET_COMMAND_PHRASE_H
#define TYPESET_COMMAND_PHRASE_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>
#include <vector>

namespace Forscape {

namespace Typeset {

class Construct;
class Line;
class Text;

class CommandPhrase : public Command{
public:
    static CommandPhrase* insert(Text* tL, size_t iL, Line* l);
    static CommandPhrase* remove(Text* tL, size_t iL, Text* tR, size_t iR);
    virtual ~CommandPhrase();
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

public:
    Text* tL;
    std::string removed;
    size_t iR;

    std::vector<Construct*> constructs;
    std::vector<Text*> texts;

    bool active;

private:
    CommandPhrase(Text* tL, const std::string& removed, size_t iR, const std::vector<Construct*>& c, const std::vector<Text*>& t, bool is_insertion);
    void insert(Controller& controller);
    void remove(Controller& controller);
    const bool is_insertion;
};

}

}

#endif // TYPESET_COMMAND_PHRASE_H
