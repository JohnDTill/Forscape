#ifndef TYPESET_TEXT_H
#define TYPESET_TEXT_H

#include <semantic_tags.h>
#include <string>
#include <vector>

#include <hope_common.h>

namespace Hope {

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
        static HOPE_UNORDERED_SET<Text*> all;
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
        void erase(size_t start, size_t size) noexcept;
        std::string_view from(size_t index) const noexcept;
        std::string_view view(size_t start, size_t sze) const noexcept;
        const std::string& getString() const noexcept;
        char charAt(size_t char_index) const noexcept;
        std::string_view codepointAt(size_t index) const noexcept;
        std::string_view graphemeAt(size_t index) const noexcept;
        size_t leadingSpaces() const noexcept;
        std::string_view checkKeyword(size_t iR) const noexcept;
        void findCaseInsensitive(const std::string& target, std::vector<Selection>& hits);
        bool precedes(Text* other) const noexcept;
        const char* data() const noexcept;

        size_t id;

        SemanticType getTypeLeftOf(size_t index) const noexcept;
        SemanticType getTypePrev() const noexcept;
        void tag(SemanticType type, size_t start, size_t stop) alloc_except;
        void tagBack(SemanticType type) alloc_except;
        std::vector<SemanticTag> tags;

        #ifdef HOPE_SEMANTIC_DEBUGGING
        std::string toSerialWithSemanticTags() const;
        #endif

        #ifndef HOPE_TYPESET_HEADLESS
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
        void paint(Painter& painter, bool forward = true) const;
        void paintUntil(Painter& painter, size_t stop, bool forward = true) const;
        void paintAfter(Painter& painter, size_t start, bool forward = true) const;
        void paintMid(Painter& painter, size_t start, size_t stop, bool forward = true) const;
        void paintGrouping(Painter& painter, size_t start) const;
        bool containsX(double x_test) const noexcept;
        bool containsY(double y_test) const noexcept;
        bool containsXInBounds(double x_test, size_t start, size_t stop) const noexcept;
        void resize() noexcept;
        double x  DEBUG_INIT_STALE;
        double y  DEBUG_INIT_STALE;
        #endif

    private:
        Phrase* parent;
        double width = 0;
        std::string str;

        #ifndef NDEBUG
        void invalidateWidth() noexcept;
        #endif
};

}

}

#endif // TYPESET_TEXT_H
