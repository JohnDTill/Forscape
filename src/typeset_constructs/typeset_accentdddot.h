#ifndef TYPESET_ACCENTDDDOT_H
#define TYPESET_ACCENTDDDOT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

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
    virtual void writePrefix(std::string& out) const noexcept override { out += ACCENTDDDOT_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        if(child()->hasConstructs()) return false;
        const std::string& str = child()->front()->getString();
        if(str.empty()){
            return false;
        }
        if(str.size() > codepointSize(str[0])) return false;
        if(!convertToUnicode(out, str, script)) return false;
        out += 0xE2;
        out += 0x83;
        out += 0x9B;

        return true;
    }

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
        painter.drawDot(x + width/2 - hoffset, y+should_drop*drop);
        painter.drawDot(x + width/2, y+should_drop*drop);
        painter.drawDot(x + width/2 + hoffset, y+should_drop*drop);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTDDDOT_H
