#ifndef TYPESET_ACCENTDDDOT_H
#define TYPESET_ACCENTDDDOT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class AccentDddot final : public Construct { 
private:
    static constexpr double voffset = 1;
    static constexpr double hoffset = 2;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentDddot(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTDDDOT; }

    #ifndef HOPE_TYPESET_HEADLESS
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
        painter.drawDot(x + width/2 - hoffset, y+should_drop*drop);
        painter.drawDot(x + width/2, y+should_drop*drop);
        painter.drawDot(x + width/2 + hoffset, y+should_drop*drop);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTDDDOT_H
