#ifndef TYPESET_ACCENTDDOT_H
#define TYPESET_ACCENTDDOT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class AccentDdot final : public Construct { 
private:
    static constexpr double voffset = 1;
    static constexpr double hoffset = 2;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentDdot(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTDDOT; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeSpecific() noexcept override {
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
        painter.drawDot(x + width/2 - hoffset/2, y+should_drop*drop);
        painter.drawDot(x + width/2 + hoffset/2, y+should_drop*drop);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTDDOT_H
