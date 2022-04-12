#ifndef TYPESET_MARKER_H
#define TYPESET_MARKER_H

#include <inttypes.h>
#include <unordered_map>

namespace Hope {

namespace Typeset {

class Line;
class Model;
class Phrase;
class Subphrase;
class Text;

struct Marker{
    Marker();
    Marker(const Model* model) noexcept;
    Marker(Text* t, size_t i) noexcept;
    Marker(const Marker&) noexcept = default;
    Marker(Marker&&) noexcept = default;
    Marker& operator=(const Marker&) noexcept = default;
    bool precedesInclusive(const Marker& other) const noexcept;
    void setToFrontOf(Text* t) noexcept;
    void setToBackOf(Text* t) noexcept;
    void setToFrontOf(const Phrase* p) noexcept;
    void setToBackOf(const Phrase* p) noexcept;
    bool isTopLevel() const noexcept;
    bool isNested() const noexcept;
    char charRight() const noexcept;
    char charLeft() const noexcept;
    Phrase* phrase() const noexcept;
    Subphrase* subphrase() const noexcept;
    Line* getLine() const noexcept;
    Line* parentAsLine() const noexcept;
    Line* nextLine() const noexcept;
    Line* prevLine() const noexcept;
    bool atTextEnd() const noexcept;
    bool atTextStart() const noexcept;
    bool notAtTextEnd() const noexcept;
    bool notAtTextStart() const noexcept;
    bool atFirstTextInPhrase() const noexcept;
    void incrementCodepoint() noexcept;
    void decrementCodepoint() noexcept;
    void incrementGrapheme() noexcept;
    void decrementGrapheme() noexcept;
    void incrementToNextWord() noexcept;
    void decrementToPrevWord() noexcept;
    bool operator==(const Marker& other) const noexcept;
    bool operator!=(const Marker& other) const noexcept;
    size_t countSpacesLeft() const noexcept;
    std::string_view checkKeyword() const noexcept;
    uint32_t codepointLeft() const noexcept;
    bool onlySpacesLeft() const noexcept;
    std::string_view strRight() const noexcept;
    std::string_view strLeft() const noexcept;
    bool compareRight(const Marker& other) const noexcept;
    bool compareLeft(const Marker& other) const noexcept;
    Model* getModel() const noexcept;

    #ifndef HOPE_TYPESET_HEADLESS
    double x() const;
    double y() const noexcept;
    void setToPointOf(Text* t, double setpoint);
    void setToLeftOf(Text* t, double setpoint);
    #endif

    //Scanner / Parser
    uint32_t advance() noexcept;
    bool peek(char ch) const noexcept;
    void skipWhitespace() noexcept;
    uint32_t scan() noexcept;
    uint32_t scanGlyph() noexcept;
    void selectToIdentifierEnd() noexcept;
    void selectToNumberEnd() noexcept;
    bool selectToStringEnd() noexcept;

    Text* text;
    size_t index;
};

}

}

template<> struct std::hash<Hope::Typeset::Marker> {
    std::size_t operator()(const Hope::Typeset::Marker& marker) const noexcept {
        std::size_t h1 = std::hash<void*>{}(marker.text);
        std::size_t h2 = std::hash<size_t>{}(marker.index);
        return h1 ^ (h2 << 1); // or use boost::hash_combine (see Discussion) https://en.cppreference.com/w/Talk:cpp/utility/hash
    }
};

#endif // TYPESET_MARKER_H
