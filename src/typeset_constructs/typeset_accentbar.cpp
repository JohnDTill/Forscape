#include "typeset_accentbar.h"

#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

AccentBar::AccentBar(){
    setupUnaryArg();
}

char AccentBar::constructCode() const noexcept { return ACCENTBAR; }

#ifndef HOPE_TYPESET_HEADLESS
void AccentBar::updateSizeFromChildSizes() noexcept {
    width = child()->width;
    under_center = child()->under_center;
    should_drop = hasSmallChild();
    above_center = child()->above_center + !should_drop * voffset;
}

void AccentBar::updateChildPositions() noexcept {
    child()->x = x;
    child()->y = y + !should_drop*voffset;
}

void AccentBar::paintSpecific(Painter& painter) const {
    painter.drawLine(x+hoffset, y+should_drop*drop, width, 0);
}
#endif

}

}
