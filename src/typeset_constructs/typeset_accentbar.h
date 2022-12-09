#ifndef TYPESET_ACCENTBAR_H
#define TYPESET_ACCENTBAR_H

#include "typeset_construct.h"

namespace Forscape {

namespace Typeset {

class AccentBar final : public Construct {
private:
    static constexpr double hoffset = 1;
    static constexpr double voffset = 1;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentBar();
    virtual char constructCode() const noexcept override;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override;
    virtual void updateChildPositions() noexcept override;
    virtual void paintSpecific(Painter& painter) const override;
    #endif
};

}

}

#endif // TYPESET_ACCENTBAR_H
