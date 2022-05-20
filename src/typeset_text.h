#ifndef TYPESET_TEXT_H
#define TYPESET_TEXT_H

#include <semantic_tags.h>
#include <string>
#include <vector>

#ifdef TYPESET_MEMORY_DEBUG
#include <hope_common.h>
#endif

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
        void writeString(std::string& out, size_t& curr, size_t pos, size_t len = std::string::npos) const noexcept;
        bool isTopLevel() const noexcept;
        bool isNested() const noexcept;
        size_t size() const noexcept;
        bool empty() const noexcept;
        char at(size_t index) const noexcept;
        std::string substr(size_t pos, size_t len = std::string::npos) const;
        std::string_view codepointAt(size_t index) const noexcept;
        std::string_view graphemeAt(size_t index) const noexcept;
        size_t leadingSpaces() const noexcept;
        std::string_view checkKeyword(size_t iR) const noexcept;
        void findCaseInsensitive(const std::string& target, std::vector<Selection>& hits);
        bool precedes(Text* other) const noexcept;

        std::string str;
        size_t id;

        SemanticType getTypeLeftOf(size_t index) const noexcept;
        SemanticType getTypePrev() const noexcept;
        void tag(SemanticType type, size_t start, size_t stop);
        void tagBack(SemanticType type);
        std::vector<SemanticTag> tags;

        #ifdef HOPE_SEMANTIC_DEBUGGING
        std::string toSerialWithSemanticTags() const;
        #endif

        #ifndef HOPE_TYPESET_HEADLESS
        double aboveCenter() const;
        double underCenter() const;
        double height() const;
        void updateWidth();
        void invalidateX() noexcept;
        double xLocal(size_t index) const;
        double xPhrase(size_t index) const;
        double xGlobal(size_t index) const;
        double xRight() const noexcept;
        double getWidth() const noexcept;
        uint8_t scriptDepth() const noexcept;
        size_t indexNearest(double x_in) const noexcept;
        size_t indexLeft(double x_in) const noexcept;
        void paint(Painter& painter, bool forward = true) const;
        void paintUntil(Painter& painter, size_t stop, bool forward = true) const;
        void paintAfter(Painter& painter, size_t start, bool forward = true) const;
        void paintMid(Painter& painter, size_t start, size_t stop, bool forward = true) const;
        void paintGrouping(Painter& painter, size_t start) const;
        bool containsX(double x_test) const noexcept;
        bool containsY(double y_test) const noexcept;
        bool containsXInBounds(double x_test, size_t start, size_t stop) const noexcept;
        void resize();
        double x;
        double y;
        #endif

    private:
        Phrase* parent;
        double width;
};

}

}

#endif // TYPESET_TEXT_H
