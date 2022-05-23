#ifndef TYPESET_TRIPLEINTEGRALCONV0_H
#define TYPESET_TRIPLEINTEGRALCONV0_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class TripleIntegralConv0 final : public Construct { 
public:
    virtual char constructCode() const noexcept override { return TRIPLEINTEGRALCONV0; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeSpecific() noexcept override {
        width = CHARACTER_WIDTHS[scriptDepth()];
        above_center = ABOVE_CENTER[scriptDepth()];
        under_center = UNDER_CENTER[scriptDepth()];
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol(x, y, "∰");
    }
    #endif
};

}

}

#endif // TYPESET_TRIPLEINTEGRALCONV0_H
