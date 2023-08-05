#ifndef TYPESET_SELECTION_H
#define TYPESET_SELECTION_H

#include "typeset_marker.h"
#include <semantic_tags.h>
#include <string_view>
#include <vector>

#ifdef QT_CORE_LIB
#include <QImage>
#include <QtSvg/QSvgGenerator>
#endif

namespace Forscape {

namespace Typeset {

class Painter;

class Selection{
public:
    Selection() noexcept;
    Selection(const Marker& left, const Marker& right) noexcept;
    Selection(const std::pair<const Marker&, const Marker&>& pair) noexcept;
    Selection(Text* t, size_t l, size_t r) noexcept;
    Selection(const Selection& other) noexcept = default;
    Selection& operator=(const Selection& other) noexcept = default;
    std::string str() const;
    std::string_view strView() const noexcept;
    bool operator==(const Selection& other) const noexcept;
    bool operator!=(const Selection& other) const noexcept;
    bool startsWith(const Selection& other) const noexcept;
    size_t hashDesignedForIdentifiers() const noexcept;
    bool isTopLevel() const noexcept;
    bool isNested() const noexcept;
    bool isTextSelection() const noexcept;
    bool isPhraseSelection() const noexcept;
    bool isEmpty() const noexcept;
    Line* getStartLine() const noexcept;
    std::string getStartLineAsString() const alloc_except;
    Model* getModel() const noexcept;
    void search(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const;

    Marker right;
    Marker left;

    size_t getConstructArgSize() const noexcept;
    size_t getMatrixRows() const noexcept;
    void formatBasicIdentifier() const noexcept;
    void formatComment() const noexcept;
    void formatKeyword() const noexcept;
    void formatNumber() const noexcept;
    void formatMutableId() const noexcept;
    void formatString() const noexcept;
    void format(SemanticType type) const noexcept;
    void formatSimple(SemanticType type) const noexcept;
    SemanticType getFormat() const noexcept;
    bool containsInclusive(const Typeset::Marker& other) const noexcept;
    size_t topLevelChars() const noexcept;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    bool contains(double x, double y) const noexcept;
    bool containsWithEmptyMargin(double x, double y) const noexcept;
    bool containsText(double x, double y) const noexcept;
    bool containsPhrase(double x, double y) const noexcept;
    bool containsLine(double x, double y) const noexcept;
    bool containsLine(double x, double y, Text* tL, size_t iL, Text* tR, size_t iR) const noexcept;
    void paint(Painter& painter) const;
    void paintSelectionText(Painter& painter, double xLeft, double xRight) const;
    void paintSelectionPhrase(Painter& painter, double xLeft, double xRight) const;
    void paintSelectionLines(Painter& painter, double xLeft, double yT, double xRight, double yB) const;
    void paintSelection(Painter& painter, double xLeft, double yT, double xRight, double yB) const;
    void paintError(Painter& painter) const;
    void paintErrorSelectionless(Painter& painter) const;
    void paintErrorText(Painter& painter) const;
    void paintErrorPhrase(Painter& painter) const;
    void paintErrorLines(Painter& painter) const;
    void paintHighlight(Painter& painter) const;
    void paintHighlightText(Painter& painter) const;
    void paintHighlightPhrase(Painter& painter) const;
    double yTop() const noexcept;
    double yBot() const noexcept;
    double yTopPhrase() const noexcept;
    double yBotPhrase() const noexcept;
    bool overlapsY(double yT, double yB) const noexcept;
    void mapConstructToParseNode(ParseNode pn) const noexcept;

    static constexpr double EMPTY_SUBPHRASE_MARGIN = 6;
    #endif

    #ifndef NDEBUG
    bool inValidState(bool require_markers_on_same_level = true) const noexcept;
    #endif

    #ifdef QT_CORE_LIB
    QImage toPng() const;
    void toSvg(QSvgGenerator& generator) const;
    #endif

private:
    Phrase* phrase() const noexcept;
    size_t characterSpan() const noexcept;
    size_t textSpan() const noexcept;
    std::string selectedTextSelection() const;
    std::string selectedPhrase() const;
    std::string selectedLines() const;
    void searchText(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const;
    void searchPhrase(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const;
    void searchLines(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const;
    #ifndef FORSCAPE_TYPESET_HEADLESS
    std::array<double, 4> getDimensions() const noexcept;
    std::array<double, 4> getDimensionsText() const noexcept;
    std::array<double, 4> getDimensionsPhrase() const noexcept;
    std::array<double, 4> getDimensionsLines() const noexcept;
    #endif

    static constexpr double NEWLINE_EXTRA = 5;
};

}

}


template <>
struct std::hash<Forscape::Typeset::Selection>{
    std::size_t operator()(const Forscape::Typeset::Selection& s) const {
        return s.hashDesignedForIdentifiers();
    }
};

#endif // TYPESET_SELECTION_H
