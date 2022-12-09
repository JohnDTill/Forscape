#include "typeset_subphrase.h"

#include "typeset_construct.h"

namespace Forscape {

namespace Typeset {

Text* Subphrase::textRightOfSubphrase() const noexcept {
    return parent->textRightOfSubphrase(this);
}

Text* Subphrase::textLeftOfSubphrase() const noexcept{
    return parent->textLeftOfSubphrase(this);
}

Text* Subphrase::textDown(double setpoint) const noexcept{
    return parent->textDown(this, setpoint);
}

Text* Subphrase::textUp(double setpoint) const noexcept{
    return parent->textUp(this, setpoint);
}

bool Subphrase::isLine() const noexcept {
    return false;
}

void Subphrase::setParent(Construct* c) noexcept{
    parent = c;
}

#ifndef FORSCAPE_TYPESET_HEADLESS
void Subphrase::paint(Painter& painter) const{
    if(numTexts() > 1 || !text(0)->empty()) Phrase::paint(painter);
    else painter.drawEmptySubphrase(x, y, width, height());
}

#ifndef NDEBUG
void Subphrase::invalidateWidth() noexcept{
    width = STALE;
    parent->invalidateWidth();
}

void Subphrase::invalidateDims() noexcept{
    width = above_center = under_center = STALE;
    parent->invalidateDims();
}
#endif
#endif

}

}
