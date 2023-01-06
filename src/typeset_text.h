#ifndef TYPESET_TEXT_H
#define TYPESET_TEXT_H

#include <semantic_tags.h>
#include <string>
#include <vector>

#include <forscape_common.h>

namespace Forscape {

namespace Typeset {

class Construct;
class Line;
class Model;
class Selection;
class Painter;
class Phrase;

class Text {
    public:
        #ifdef TYPESET_MEMORY_DEBUG
        static FORSCAPE_UNORDERED_SET<Text*> all;
        Text();
        ~Text();
        #endif

        void setParent(Phrase* p) noexcept;
        Phrase* getParent() const noexcept;
        Line* getLine() const noexcept;
        Model* getModel() const noexcept;
        Text* nextTextInPhrase() const noexcept;
        Text* prevTextInPhrase() const noexcept;
        Construct* nextConstructInPhrase() const noexcept;
        Construct* prevConstructInPhrase() const noexcept;
        Text* nextTextAsserted() const noexcept;
        Text* prevTextAsserted() const noexcept;
        Construct* nextConstructAsserted() const noexcept;
        Construct* prevConstructAsserted() const noexcept;
        void writeString(std::string& out, size_t& curr) const noexcept;
        void writeString(std::string& out, size_t& curr, size_t pos) const noexcept;
        void writeString(std::string& out, size_t& curr, size_t pos, size_t len) const noexcept;
        bool isTopLevel() const noexcept;
        bool isNested() const noexcept;
        size_t numChars() const noexcept;
        bool empty() const noexcept;
        void setString(std::string_view str) alloc_except;
        void setString(const char* ch, size_t sze) alloc_except;
        void append(std::string_view appended) alloc_except;
        void prependSpaces(size_t num_spaces) alloc_except;
        void removeLeadingSpaces(size_t num_spaces) noexcept;
        void overwrite(size_t start, const std::string& in) alloc_except;
        void overwrite(size_t start, std::string_view in) alloc_except;
        void insert(size_t start, const std::string& in) alloc_except;
        void erase(size_t start, const std::string& out) noexcept;
        std::string_view from(size_t index) const noexcept;
        std::string_view view(size_t start, size_t sze) const noexcept;
        const std::string& getString() const noexcept;
        char charAt(size_t char_index) const noexcept;
        std::string_view codepointAt(size_t index) const noexcept;
        std::string_view graphemeAt(size_t index) const noexcept;
        size_t leadingSpaces() const noexcept;
        std::string_view checkKeyword(size_t iR) const noexcept;
        void search(const std::string& target, std::vector<Selection>& hits, bool use_case, bool word);
        void search(const std::string& target, std::vector<Selection>& hits, size_t start, size_t end, bool use_case, bool word);
        bool precedes(Text* other) const noexcept;
        const char* data() const noexcept;

        size_t id;

        SemanticType getTypeLeftOf(size_t index) const noexcept;
        SemanticType getTypePrev() const noexcept;
        void tag(SemanticType type, size_t start, size_t stop) alloc_except;
        void tagBack(SemanticType type) alloc_except;
        std::vector<SemanticTag> tags;

        struct ParseNodeTag {
            ParseNode pn  DEBUG_INIT_NONE;
            size_t token_start  DEBUG_INIT_NONE;
            size_t token_end  DEBUG_INIT_NONE;

            ParseNodeTag() noexcept {}
            ParseNodeTag(ParseNode pn, size_t token_start, size_t token_end) noexcept
                : pn(pn), token_start(token_start), token_end(token_end) {}
        };

        #ifdef FORSCAPE_SEMANTIC_DEBUGGING
        std::string toSerialWithSemanticTags() const;
        #endif

        #ifndef FORSCAPE_TYPESET_HEADLESS
        double aboveCenter() const noexcept;
        double underCenter() const noexcept;
        double height() const noexcept;
        double xLocal(size_t index) const noexcept;
        double xPhrase(size_t index) const;
        double xGlobal(size_t index) const;
        double xRight() const noexcept;
        double yBot() const noexcept;
        double getWidth() const noexcept;
        void updateWidth() noexcept;
        uint8_t scriptDepth() const noexcept;
        size_t charIndexNearest(double x_in) const noexcept;
        size_t charIndexLeft(double x_in) const noexcept;
        void paint(Painter& painter) const;
        void paintUntil(Painter& painter, size_t stop) const;
        void paintAfter(Painter& painter, size_t start) const;
        void paintMid(Painter& painter, size_t start, size_t stop) const;
        void paintGrouping(Painter& painter, size_t start) const;
        bool containsX(double x_test) const noexcept;
        bool containsY(double y_test) const noexcept;
        bool containsXInBounds(double x_test, size_t start, size_t stop) const noexcept;
        double x  DEBUG_INIT_STALE;
        double y  DEBUG_INIT_STALE;

        size_t tagParseNode(ParseNode pn, size_t token_start, size_t token_end) alloc_except;
        void retagParseNodeLast(ParseNode pn) noexcept;
        void retagParseNode(ParseNode pn, size_t index) noexcept;
        void patchParseNode(ParseNode pn, size_t index) noexcept;
        void patchParseNode(size_t index, ParseNode pn, size_t start, size_t end) noexcept;
        void popParseNode() noexcept;
        void insertParseNodes(size_t index, size_t n) alloc_except;
        size_t parseNodeTagIndex(size_t char_index) const noexcept;
        ParseNode parseNodeAtIndex(size_t index) const noexcept;
        ParseNode parseNodeAtX(double x) const noexcept;
        #ifndef NDEBUG
        void populateDocMapParseNodes(std::unordered_set<ParseNode>& nodes) const noexcept;
        #endif

        std::vector<ParseNodeTag> parse_nodes;
        #endif

    private:
        Phrase* parent;
        double width = 0;
        std::string str;
};

}

}

#endif // TYPESET_TEXT_H
