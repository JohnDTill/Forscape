#ifndef TYPESET_MODEL_H
#define TYPESET_MODEL_H

#include "forscape_error.h"
#include "typeset_command.h"
#include <string>
#include <vector>

#include "forscape_scanner.h"
#include "forscape_parser.h"
#include "forscape_symbol_build_pass.h"
#include "forscape_static_pass.h"
#include "forscape_interpreter.h"

namespace Forscape {

namespace Typeset {

class CommandLine;
class Construct;
class Controller;
class Line;
class Painter;
class Text;
class View;
class Editor;

class Model {
public:
    Code::Scanner scanner = Code::Scanner(this);
    Code::Parser parser = Code::Parser(scanner, this);
    Code::SymbolTableBuilder symbol_builder = Code::SymbolTableBuilder(parser.parse_tree, this);
    Code::StaticPass static_pass = Code::StaticPass(parser.parse_tree, symbol_builder.symbol_table, errors, warnings);
    Code::Interpreter interpreter;
    std::vector<Code::Error> errors;
    std::vector<Code::Error> warnings;

    Model();
    ~Model();
    void clear() noexcept;
    static Model* fromSerial(const std::string& src, bool is_output = false);
    std::string toSerial() const;
    std::string run();
    void runThread();
    void stop();
    void updateWidth() noexcept;
    double getWidth() noexcept;
    void updateHeight() noexcept;
    double getHeight() noexcept;
    size_t numLines() const noexcept;
    void appendSerialToOutput(const std::string& src);
    double width  DEBUG_INIT_STALE;
    double height  DEBUG_INIT_STALE;

    #ifdef FORSCAPE_SEMANTIC_DEBUGGING
    std::string toSerialWithSemanticTags() const;
    #endif

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void calculateSizes();
    void updateLayout();
    void paint(Painter& painter, double xL, double yT, double xR, double yB) const;
    void paintGroupings(Painter& painter, const Typeset::Marker& loc) const;
    void paintNumberCommas(Painter& painter, double xL, double yT, double xR, double yB, const Selection& sel) const;
    #ifndef NDEBUG
    void populateDocMapParseNodes(std::unordered_set<ParseNode>& nodes) const noexcept;
    #endif
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
    void search(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const;
    bool empty() const noexcept;
    size_t serialChars() const noexcept;
    Line* nearestLine(double y) const noexcept;
    void registerCommaSeparatedNumber(const Typeset::Selection& sel) alloc_except;

    static constexpr double LINE_VERTICAL_PADDING = 5;

private:
    Model(const std::string& src, bool is_output = false);
    static std::vector<Line*> linesFromSerial(const std::string& src);
    void writeString(std::string& out) const noexcept;
    Line* nextLine(const Line* l) const noexcept;
    Line* prevLine(const Line* l) const noexcept;
    Line* nextLineAsserted(const Line* l) const noexcept;
    Line* prevLineAsserted(const Line* l) const noexcept;
    #ifndef FORSCAPE_TYPESET_HEADLESS
    Line* nearestAbove(double y) const noexcept;
    Construct* constructAt(double x, double y) const noexcept;
    ParseNode parseNodeAt(double x, double y) const noexcept;
    #endif
    void clearRedo();
    void remove(size_t start, size_t stop) noexcept;
    void insert(const std::vector<Line*>& l);
    Typeset::Marker begin() const noexcept;

    std::vector<Line*> lines;
    friend CommandLine;
    friend Controller;
    friend Line;
    friend View;
    friend Editor;

    std::vector<Command*> undo_stack;
    std::vector<Command*> redo_stack;

    Selection find(const std::string& str) noexcept;
    void performSemanticFormatting();
    void premutate() noexcept;
    void postmutate();

    void rename(const std::vector<Selection>& targets, const std::string& name, Controller& c);

    Code::ParseTree& parseTree() noexcept;

    std::vector<Typeset::Selection> comma_separated_numbers;
};

}

}

#endif // TYPESET_MODEL_H
