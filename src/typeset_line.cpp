#include "typeset_line.h"

#include <typeset_model.h>

namespace Hope {

namespace Typeset {

Line::Line()
    : Phrase() {
    #ifndef HOPE_TYPESET_HEADLESS
    script_level = 0;
    #endif

    x = 0; //DO THIS - delete
}

Line::Line(Model* model){
    parent = model;
    #ifndef HOPE_TYPESET_HEADLESS
    script_level = 0;
    #endif

    x = 0; //DO THIS - delete
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

#ifndef HOPE_TYPESET_HEADLESS
Line* Line::nearestLine(double y) const noexcept{
    return parent->nearestLine(y);
}

Line* Line::nearestAbove(double y) const noexcept{
    return parent->nearestAbove(y);
}

void Line::resize() noexcept{
    updateSize();
    parent->updateLayout(); //DO THIS
}

#ifndef NDEBUG
void Line::invalidateWidth() noexcept{
    width = STALE;
    if(parent) parent->width = STALE;
}

void Line::invalidateDims() noexcept{
    width = above_center = under_center = parent->height = STALE;
}
#endif
#endif

}

}
