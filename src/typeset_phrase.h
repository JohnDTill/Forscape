#ifndef TYPESET_PHRASE_H
#define TYPESET_PHRASE_H

#include "typeset_text.h"
#include <string>
#include <vector>

namespace Hope {

namespace Typeset {

class Construct;
class Line;
class Painter;
class Selection;
class Subphrase;
class Text;

class Phrase {
public:
    Phrase();
    ~Phrase();
    void appendConstruct(Construct* c);
    void appendConstruct(Construct* c, Text* t);
    size_t serialChars() const noexcept;
    void writeString(std::string& out, size_t& curr) const noexcept;
    std::string toString() const;
    virtual bool isLine() const noexcept = 0;
    Line* asLine() noexcept;
    Subphrase* asSubphrase() noexcept;
    Text* text(size_t index) const noexcept;
    Construct* construct(size_t index) const noexcept;
    Text* front() const noexcept;
    Text* back() const noexcept;
    Text* nextTextInPhrase(const Construct* c) const noexcept;
    Text* nextTextInPhrase(const Text* t) const noexcept;
    Text* prevTextInPhrase(const Construct* c) const noexcept;
    Text* prevTextInPhrase(const Text* t) const noexcept;
    Construct* nextConstructInPhrase(const Text* t) const noexcept;
    Construct* prevConstructInPhrase(const Text* t) const noexcept;
    Text* nextTextAsserted(const Text* t) const noexcept;
    Text* prevTextAsserted(const Text* t) const noexcept;
    Construct* nextConstructAsserted(const Text* t) const noexcept;
    Construct* prevConstructAsserted(const Text* t) const noexcept;
    size_t numTexts() const noexcept;
    size_t numConstructs() const noexcept;
    void remove(size_t start) noexcept;
    void remove(size_t start, size_t stop) noexcept;
    void insert(const std::vector<Construct*>& c, const std::vector<Text*>& t);
    void insert(size_t index, const std::vector<Construct*>& c, const std::vector<Text*>& t);
    void giveUpOwnership() noexcept;
    bool sameContent(const Phrase* other) const noexcept;
    void search(const std::string& target, std::vector<Selection>& hits, bool use_case, bool word) const;
    bool hasConstructs() const noexcept;
    size_t nestingDepth() const noexcept;
    bool empty() const noexcept;
    ParseNode parseNodeAt(double x, double y) const noexcept;

    size_t id;

    #ifdef HOPE_SEMANTIC_DEBUGGING
    std::string toStringWithSemanticTags() const;
    #endif

    #ifndef HOPE_TYPESET_HEADLESS
    static constexpr double EMPTY_PHRASE_WIDTH_RATIO = 0.6;
    Text* textLeftOf(double x) const noexcept;
    Construct* constructAt(double x, double y) const noexcept;
    double height() const noexcept {return above_center + under_center;}
    double yBottom() const noexcept {return y + height();}
    void updateSize() noexcept;
    void updateLayout() noexcept;
    virtual void paint(Painter& painter, bool forward = true) const;
    virtual void paintUntil(Painter& painter, Text* t_end, size_t index, bool forward = true) const;
    virtual void paintAfter(Painter& painter, Text* t_start, size_t index, bool forward = true) const;
    virtual void paintMid(Painter& painter, Text* tL, size_t iL, Text* tR, size_t iR, bool forward = true) const;
    bool contains(double x_test, double y_test) const noexcept;
    bool containsY(double y_test) const noexcept;
    Text* textNearest(double x, double y) const;
    double width  DEBUG_INIT_STALE;
    double above_center  DEBUG_INIT_STALE;
    double under_center  DEBUG_INIT_STALE;
    double x  DEBUG_INIT_STALE;
    double y  DEBUG_INIT_STALE;
    uint8_t script_level;
    #ifndef NDEBUG
    virtual void invalidateWidth() noexcept = 0;
    virtual void invalidateDims() noexcept = 0;
    void populateDocMapParseNodes(std::unordered_set<ParseNode>& nodes) const noexcept;
    #endif
    #endif

private:
    std::vector<Text*> texts;
    std::vector<Construct*> constructs;
};

}

}

#endif // TYPESET_PHRASE_H
