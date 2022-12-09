#ifndef TYPESET_COMMAND_LINE_H
#define TYPESET_COMMAND_LINE_H

#include <typeset_command.h>
#include <inttypes.h>
#include <string>
#include <vector>

namespace Forscape {

namespace Typeset {

class Construct;
class Line;
class Model;
class Text;

class CommandLine : public Command{
public:
    static CommandLine* insert(Text* tL, size_t iL, std::vector<Line*> lines);
    static CommandLine* remove(Text* tL, size_t iL, Text* tR, size_t iR);
    virtual ~CommandLine();
    virtual void undo(Controller& controller) override;
    virtual void redo(Controller& controller) override;

private:
    void insert(Controller& controller);
    void remove(Controller& controller);
    const bool is_insertion;

    struct PhraseRight{
        const std::string str;
        std::vector<Construct*> constructs;
        std::vector<Text*> texts;

        PhraseRight(Text* t, size_t index);
        void writeTo(Text* t, size_t index);
        void free();
    };

    bool active;

    Text* t;
    const size_t iL;
    PhraseRight source_fragment;
    PhraseRight insert_fragment;
    std::vector<Line*> insert_lines;
    Text* tR;
    const size_t iR;

    Line* baseLine();
    Model* model();

    CommandLine(bool is_insertion,
                Text* t,
                const size_t iL,
                const PhraseRight& source_fragment,
                const PhraseRight& insert_fragment,
                const std::vector<Line*>& insert_lines,
                Text* tR,
                const size_t iR);
};

}

}

#endif // TYPESET_COMMAND_LINE_H
