#ifndef TYPESET_SUBPHRASE_H
#define TYPESET_SUBPHRASE_H

#include "typeset_phrase.h"

namespace Hope {

namespace Typeset {

class Subphrase final : public Phrase {
public:
    Text* textRightOfSubphrase() const noexcept;
    Text* textLeftOfSubphrase() const noexcept;
    Text* textDown(double setpoint) const noexcept;
    Text* textUp(double setpoint) const noexcept;
    virtual bool isLine() const noexcept override;
    void setParent(Construct* c) noexcept;
    Construct* parent;

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void paint(Painter& painter) const override;
    #ifndef NDEBUG
    virtual void invalidateWidth() noexcept override;
    virtual void invalidateDims() noexcept override;
    #endif
    #endif
};

}

}

#endif
