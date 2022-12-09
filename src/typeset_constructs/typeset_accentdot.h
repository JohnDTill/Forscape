#ifndef TYPESET_ACCENTDOT_H
#define TYPESET_ACCENTDOT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

class AccentDot final : public Construct { 
private:
    static constexpr double voffset = 1;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentDot(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTDOT; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override {
        width = child()->width;
        should_drop = hasSmallChild();
        above_center = child()->above_center + !should_drop*voffset;
        under_center = child()->under_center;
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x;
        child()->y = y + !should_drop*voffset;
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawDot(x + width/2, y+should_drop*drop);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTDOT_H
