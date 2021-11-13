#ifndef TYPESET_ACCENTHAT_H
#define TYPESET_ACCENTHAT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class AccentHat final : public Construct {
private:
    static constexpr double hoffset = 1;
    static constexpr double voffset = 1;
    static constexpr double extra = 3;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentHat(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTHAT; }

    virtual void updateSizeSpecific() noexcept override {
        width = child()->width;
        should_drop = hasSmallChild();
        above_center = child()->above_center + !should_drop*voffset;
        under_center = child()->under_center;
    }

    virtual void updateChildPositions() override {
        child()->x = x;
        child()->y = y + !should_drop*voffset;
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol("∧", x+hoffset, y+should_drop*drop, width, voffset + extra);
    }
};

}

}

#endif // TYPESET_ACCENTHAT_H
