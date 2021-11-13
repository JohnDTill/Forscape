#include "typeset_line.h"

#include <typeset_model.h>

namespace Hope {

namespace Typeset {

Line::Line()
    : Phrase() {
    script_level = 0;
}

Line::Line(Model* model){
    parent = model;
    script_level = 0;
}

bool Line::isLine() const noexcept {
    return true;
}

Line* Line::next() const noexcept{
    return parent->nextLine(this);
}

Line* Line::prev() const noexcept{
    return parent->prevLine(this);
}

Line* Line::nextAsserted() const noexcept{
    return parent->nextLineAsserted(this);
}

size_t Line::leadingSpaces() const noexcept{
    return front()->leadingSpaces();
}

const std::vector<Line*>& Line::lines() const noexcept{
    return parent->lines;
}

Line* Line::nearestLine(double y) const noexcept{
    return parent->nearestLine(y);
}

#ifndef HOPE_TYPESET_HEADLESS
void Line::resize() noexcept{
    updateSize();
    parent->updateLayout();
}
#endif

}

}
