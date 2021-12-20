#ifndef TYPESET_MODEL_H
#define TYPESET_MODEL_H

#include "hope_error.h"
#include "typeset_command.h"
#include <string>
#include <vector>

#include "hope_scanner.h"
#include "hope_parser.h"
#include "hope_symbol_build_pass.h"
#include "hope_interpreter.h"

namespace Hope {

namespace Typeset {

class CommandLine;
class Construct;
class Controller;
class Line;
class Painter;
class Text;
class View;

class Model {   
public:
    Code::Scanner scanner = Code::Scanner(this);
    Code::Parser parser = Code::Parser(scanner, this);
    Code::Interpreter interpreter;
    std::vector<Code::Error> errors;

    Model();
    ~Model();
    static Model* fromSerial(const std::string& src, bool is_output = false);
    std::string toSerial() const;
    std::string run();
    void runThread();
    void stop();

    #ifdef HOPE_SEMANTIC_DEBUGGING
    std::string toSerialWithSemanticTags() const;
    #endif

    #ifndef HOPE_TYPESET_HEADLESS
    void calculateSizes();
    void updateLayout();
    void paint(Painter& painter) const;
    void paint(Painter& painter, double xL, double yT, double xR, double yB) const;
    #endif

    #ifndef NDEBUG
    std::string parseTreeDot() const;
    #endif

public:
    void undo(Controller& controller);
    void redo(Controller& controller);
    bool undoAvailable() const noexcept;
    bool redoAvailable() const noexcept;
    void mutate(Command* cmd, Controller& controller);
    void clearFormatting() noexcept;
    bool is_output = false;

    Text* firstText() const noexcept;
    Text* lastText() const noexcept;
    Line* appendLine();
    Line* lastLine() const noexcept;
    std::vector<Selection> findCaseInsensitive(const std::string& str) const;

    static constexpr double LINE_VERTICAL_PADDING = 5;

private:
    Model(const std::string& src, bool is_output = false);
    static std::vector<Line*> linesFromSerial(const std::string& src);
    size_t serialChars() const noexcept;
    void writeString(std::string& out) const noexcept;
    Line* nextLine(const Line* l) const noexcept;
    Line* prevLine(const Line* l) const noexcept;
    Line* nextLineAsserted(const Line* l) const noexcept;
    #ifndef HOPE_TYPESET_HEADLESS
    Line* nearestLine(double y) const noexcept;
    Line* nearestAbove(double y) const noexcept;
    Construct* constructAt(double x, double y) const noexcept;
    double width() const noexcept;
    double height() const noexcept;
    Selection idAt(double x, double y) noexcept;
    #endif
    void clearRedo();
    void remove(size_t start, size_t stop) noexcept;
    void insert(const std::vector<Line*>& l);

    std::vector<Line*> lines;
    friend CommandLine;
    friend Controller;
    friend Line;
    friend View;
    friend Code::Scanner;
    friend Code::Parser;
    friend Code::SymbolTableBuilder;
    friend Code::Interpreter;

    Code::IdMap symbol_table;
    Code::SymbolTableBuilder symbol_builder = Code::SymbolTableBuilder(parser.parse_tree, this);
    Code::ParseNode root;

    std::vector<Command*> undo_stack;
    std::vector<Command*> redo_stack;

    Selection idAt(const Marker& marker) noexcept;
    Selection find(const std::string& str) noexcept;
    void performSemanticFormatting();
    void premutate() noexcept;
    void postmutate();

    void rename(const std::vector<Selection>& targets, const std::string& name, Controller& c);
};

}

}

#endif // TYPESET_MODEL_H
