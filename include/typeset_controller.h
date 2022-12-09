#ifndef TYPESET_CONTROLLER_H
#define TYPESET_CONTROLLER_H

#include <inttypes.h>
#include <string>

#include "typeset_marker.h"
#include "typeset_selection.h"
#include <semantic_tags.h>

namespace Forscape {

namespace Code {
class Parser;
class Scanner;
class SymbolTableBuilder;
class Interpreter;
}

namespace Typeset {

class Command;
class CommandLine;
class CommandPhrase;
class CommandText;
class Construct;
class InsertChars;
class Line;
class Model;
class Painter;
class Phrase;
class RemoveChars;
class Subphrase;
class Text;
class View;
class Editor;

class Controller {
public:
    Controller();
    Controller(Model* model);
    Controller(const Marker& anchor, const Marker& active);
    Controller(const Controller& other) = default;
    Controller(const Selection& sel) noexcept;
    Controller& operator=(const Controller& other) noexcept = default;
    Controller& operator=(const Marker& marker) noexcept;
    const Marker& getAnchor() const noexcept;
    const Marker& getActive() const noexcept;
    bool hasSelection() const noexcept;
    void deselect() noexcept;
    void moveToNextChar() noexcept;
    void moveToPrevChar() noexcept;
    void moveToNextWord() noexcept;
    void moveToPrevWord() noexcept;
    void moveToStartOfLine() noexcept;
    void moveToEndOfLine() noexcept;
    void moveToStartOfDocument() noexcept;
    void moveToEndOfDocument() noexcept;
    void selectNextChar() noexcept;
    void selectPrevChar() noexcept;
    void selectNextWord() noexcept;
    void selectPrevWord() noexcept;
    void selectStartOfLine() noexcept;
    void selectEndOfLine() noexcept;
    void selectStartOfDocument() noexcept;
    void selectEndOfDocument() noexcept;
    void selectAll() noexcept;
    Model* getModel() const noexcept;
    bool atStart() const noexcept;
    bool atEnd() const noexcept;
    bool atLineStart() const noexcept;
    void del() noexcept;
    void backspace() noexcept;
    void delWord() noexcept;
    void backspaceWord() noexcept;
    void newline() noexcept;
    void tab();
    void detab() noexcept;
    void keystroke(const std::string& str);
    void insertText(const std::string& str);
    void insertSerial(const std::string& src);
    void setBothToFrontOf(Text* t) noexcept;
    void setBothToBackOf(Text* t) noexcept;
    std::string selectedText() const;
    Selection selection() const noexcept;
    bool isTextSelection() const noexcept;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void moveToNextLine(double setpoint) noexcept;
    void moveToPrevLine(double setpoint) noexcept;
    void moveToNextPage(double setpoint, double page_height) noexcept;
    void moveToPrevPage(double setpoint, double page_height) noexcept;
    void selectNextLine(double setpoint) noexcept;
    void selectPrevLine(double setpoint) noexcept;
    void selectNextPage(double setpoint, double page_height) noexcept;
    void selectPrevPage(double setpoint, double page_height) noexcept;
    bool contains(double x, double y) const;
    void paintSelection(Painter& painter, double xL, double yT, double xR, double yB) const;
    void paintCursor(Painter& painter) const;
    void paintInsertCursor(Painter& painter, double xL, double yT, double xR, double yB) const;
    #endif

    static constexpr size_t INDENT_SIZE = 4;

private:
    void consolidateToActive() noexcept;
    void consolidateToAnchor() noexcept;
    void consolidateRight() noexcept;
    void consolidateLeft() noexcept;
    bool isForward() const noexcept;
    bool isTopLevel() const noexcept;
    bool isNested() const noexcept;
    bool isPhraseSelection() const noexcept;
    char charRight() const noexcept;
    char charLeft() const noexcept;
    Phrase* phrase() const noexcept;
    Subphrase* subphrase() const noexcept;
    Line* activeLine() const noexcept;
    Line* anchorLine() const noexcept;
    Line* nextLine() const noexcept;
    Line* prevLine() const noexcept;
    bool atTextEnd() const noexcept;
    bool atTextStart() const noexcept;
    bool notAtTextEnd() const noexcept;
    bool notAtTextStart() const noexcept;
    void incrementActiveCodepoint() noexcept;
    void decrementActiveCodepoint() noexcept;
    void incrementActiveGrapheme() noexcept;
    void decrementActiveGrapheme() noexcept;
    void incrementToNextWord() noexcept;
    void decrementToPrevWord() noexcept;
    void selectLine(const Line* l) noexcept;
    void selectConstruct(const Construct* c) noexcept;
    void deleteChar();
    void deleteAdditionalChar(Command* cmd);
    void deleteFirstChar();
    Command* deleteSelection();
    Command* deleteSelectionText();
    Command* deleteSelectionPhrase();
    Command* deleteSelectionPhrase(Text* tL, size_t iL, Text* tR, size_t iR);
    Command* deleteSelectionLine();
    Command* deleteSelectionLine(Text* tL, size_t iL, Text* tR, size_t iR);
    void insertAdditionalChar(Command* cmd, const std::string& str);
    Command* insertFirstChar(const std::string& str);
    Command* getInsertSerial(const std::string& str);
    Command* insertSerialNoSelection(const std::string& str);

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void clickTo(const Phrase* p, double x, double y) noexcept;
    void clickTo(const Construct* c, double x, double y) noexcept;
    void shiftClick(const Phrase* p, double x) noexcept;
    double xActive() const;
    double xAnchor() const;
    #endif

    Marker active;
    Marker anchor;

    //Scanner / Parser
    uint32_t advance() noexcept;
    bool peek(char ch) const noexcept;
    void skipWhitespace() noexcept;
    uint32_t scan() noexcept;
    uint32_t scanGlyph() noexcept;
    void selectToIdentifierEnd() noexcept;
    void selectToNumberEnd() noexcept;
    bool selectToStringEnd() noexcept;
    void selectToPathEnd() noexcept;
    std::string_view selectedFlatText() const noexcept;
    Controller(const Controller& lhs, const Controller& rhs);
    void formatBasicIdentifier() const noexcept;
    void formatComment() const noexcept;
    void formatKeyword() const noexcept;
    void formatNumber() const noexcept;
    void formatMutableId() const noexcept;
    void formatString() const noexcept;
    void format(SemanticType type) const noexcept;
    void formatSimple(SemanticType type) const noexcept;

    friend Code::Scanner;
    friend Code::Parser;
    friend Code::SymbolTableBuilder;
    friend Code::Interpreter;
    friend CommandLine;
    friend CommandPhrase;
    friend CommandText;
    friend InsertChars;
    friend Model;
    friend RemoveChars;
    friend View;
    friend Editor;
};

}

}

#endif // TYPESET_CONTROLLER_H
