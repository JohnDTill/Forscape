#ifndef TYPESET_ACCENTBAR_H
#define TYPESET_ACCENTBAR_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class AccentBar final : public Construct {
private:
    static constexpr double hoffset = 1;
    static constexpr double voffset = 1;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentBar(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTBAR; }

    virtual void updateSizeSpecific() noexcept override {
        width = child()->width;
        under_center = child()->under_center;
        should_drop = hasSmallChild();
        above_center = child()->above_center + !should_drop * voffset;
    }

    virtual void updateChildPositions() override {
        child()->x = x;
        child()->y = y + !should_drop*voffset;
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawLine(x+hoffset, y+should_drop*drop, width, 0);
    }
};

}

}

#endif // TYPESET_ACCENTBAR_H
