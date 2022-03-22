#ifndef HOPE_TYPESET_HEADLESS

#include "typeset_themes.h"

#include <QColor>
#include <QPalette>
#include <array>

namespace Hope {

namespace Typeset {

static constexpr size_t NUM_COLOUR_ROLES = GroupingBackground+1;
static std::array<QColor, NUM_COLOUR_ROLES> colours;

void setColour(ColourRole role, uint8_t red, uint8_t green, uint8_t blue) noexcept{
    colours[static_cast<size_t>(role)] = QColor::fromRgb(red, green, blue);
}

void setDefaultTheme() noexcept{
    colours[Background] = Qt::white;
    colours[DisabledBackground] = QPalette().color(QPalette::Disabled, QPalette::Window);
    colours[SelectionBox] = QColor::fromRgb(0, 120, 215);
    colours[SelectionText] = Qt::white;
    colours[TextCursor] = Qt::black;
    colours[ErrorBackground] = QColor::fromRgb(255, 180, 180);
    colours[ErrorBorder] = QColor::fromRgb(255, 50, 50);
    colours[LineBoxFill] = QColor::fromRgb(240, 240, 240);
    colours[LineBoxBorder] = Qt::lightGray;
    colours[LineNumActive] = Qt::black;
    colours[LineNumPassive] = Qt::darkGray;
    colours[GroupingHighlight] = QColor::fromRgb(255, 0, 0);
    colours[GroupingBackground] = QColor::fromRgb(180, 238, 180);
}

template<ColourRole role>
const QColor& getColour() noexcept{
    return colours[role];
}

template const QColor& getColour<Background>() noexcept;
template const QColor& getColour<DisabledBackground>() noexcept;
template const QColor& getColour<SelectionBox>() noexcept;
template const QColor& getColour<SelectionText>() noexcept;
template const QColor& getColour<TextCursor>() noexcept;
template const QColor& getColour<ErrorBackground>() noexcept;
template const QColor& getColour<ErrorBorder>() noexcept;
template const QColor& getColour<LineBoxFill>() noexcept;
template const QColor& getColour<LineBoxBorder>() noexcept;
template const QColor& getColour<LineNumActive>() noexcept;
template const QColor& getColour<LineNumPassive>() noexcept;
template const QColor& getColour<GroupingHighlight>() noexcept;
template const QColor& getColour<GroupingBackground>() noexcept;


const QColor& getColour(ColourRole role) noexcept{
    return colours[role];
}

}

}

#endif
