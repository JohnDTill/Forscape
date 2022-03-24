#ifndef TYPESET_THEMES_H
#define TYPESET_THEMES_H
#ifndef HOPE_TYPESET_HEADLESS

#include <inttypes.h>
#include <string_view>
class QColor;

namespace Hope {

namespace Typeset {

enum ColourRole{
    Background,
    DisabledBackground,
    SelectionBox,
    SelectionText,
    TextCursor,
    ErrorBackground,
    ErrorBorder,
    LineBoxFill,
    LineBoxBorder,
    LineNumActive,
    LineNumPassive,
    GroupingHighlight,
    GroupingBackground,
};

constexpr std::string_view colour_names [GroupingBackground+1] {
    "Background",
    "Background (disabled)",
    "Selection",
    "Selected Text",
    "Cursor",
    "Error Background",
    "Error Border",
    "Linebox Fill",
    "Linebox Border",
    "Line Num (active)",
    "Line Num (passive)",
    "Grouping Highlight",
    "Grouping Background",
};

enum Preset{
    Default,
    Dark,
    Matrix,
    Chalkboard,
};

constexpr std::string_view preset_names [Chalkboard+1] {
    "Default",
    "Dark Mode",
    "Matrix",
    "Chalkboard",
};

void setColour(ColourRole role, uint8_t red, uint8_t green, uint8_t blue) noexcept;
void setColour(ColourRole role, const QColor& color) noexcept;
template<ColourRole role> const QColor& getColour() noexcept;
const QColor& getColour(ColourRole role) noexcept;
std::string_view getString(ColourRole role) noexcept;
void setPreset(Preset preset) noexcept;
std::string_view getPresetName(Preset preset) noexcept;
void setDefaultTheme() noexcept;
void setDarkTheme() noexcept;
void setMatrixTheme() noexcept;
void setChalkboardTheme() noexcept;
size_t numColourRoles() noexcept;
size_t numColourPresets() noexcept;

}

}

#endif // HOPE_TYPESET_HEADLESS
#endif // TYPESET_THEMES_H
