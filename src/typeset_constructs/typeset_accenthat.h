#ifndef TYPESET_ACCENTHAT_H
#define TYPESET_ACCENTHAT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

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
    virtual void writePrefix(std::string& out) const noexcept override { out += ACCENTHAT_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        if(child()->hasConstructs()) return false;
        const std::string& str = child()->front()->getString();
        if(str.empty()){
            out += '^';
            return true;
        }
        if(str.size() > codepointSize(str[0])) return false;
        if(!convertToUnicode(out, str, script)) return false;
        out += 0xCC;
        out += 0x82;

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
        painter.drawSymbol("∧", x+hoffset, y+should_drop*drop, width, voffset + extra);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTHAT_H
