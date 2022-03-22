#ifndef TYPESET_THEMES_H
#define TYPESET_THEMES_H
#ifndef HOPE_TYPESET_HEADLESS

#include <inttypes.h>
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

void setColour(ColourRole role, uint8_t red, uint8_t green, uint8_t blue) noexcept;
template<ColourRole role> const QColor& getColour() noexcept;
const QColor& getColour(ColourRole role) noexcept;
void setDefaultTheme() noexcept;

}

}

#endif // HOPE_TYPESET_HEADLESS
#endif // TYPESET_THEMES_H
