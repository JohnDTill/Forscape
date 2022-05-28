#ifndef TYPESET_ACCENTBREVE_H
#define TYPESET_ACCENTBREVE_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class AccentBreve final : public Construct { 
private:
    static constexpr double hoffset = 1;
    static constexpr double voffset = 1;
    static constexpr double extra = 8;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentBreve(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTBREVE; }

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
        painter.drawSymbol("˘", x+hoffset, y+should_drop*drop, width, voffset + extra);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTBREVE_H
