#ifndef TYPESET_BIGSUM0_H
#define TYPESET_BIGSUM0_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class BigSum0 final : public Construct { 
public:
    virtual char constructCode() const noexcept override { return BIGSUM0; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override {
        width = CHARACTER_WIDTHS[scriptDepth()];
        above_center = ABOVE_CENTER[scriptDepth()];
        under_center = UNDER_CENTER[scriptDepth()];
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol(x, y, "∑");
    }
    #endif
};

}

}

#endif // TYPESET_BIGSUM0_H
