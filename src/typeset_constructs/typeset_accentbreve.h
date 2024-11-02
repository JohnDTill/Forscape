#ifndef TYPESET_ACCENTBREVE_H
#define TYPESET_ACCENTBREVE_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

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
    virtual void writePrefix(std::string& out) const noexcept override { out += ACCENTBREVE_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        if(child()->hasConstructs()) return false;
        const std::string& str = child()->front()->getString();
        if(str.empty()){
            out += "˘";
            return true;
        }
        const size_t first_codepoint_size = codepointSize(str[0]);
        if(first_codepoint_size == str.size()){
            if(!convertToUnicode(out, str, script)) return false;
            out += 0xCC;
            out += 0x86;
            return true;
        }
        const size_t second_codepoint_size = codepointSize(str[first_codepoint_size]);
        if(first_codepoint_size + second_codepoint_size == str.size()){
            // Combining Double Breve
            if(!convertToUnicode(out, str.substr(0, first_codepoint_size), script)) return false;
            out += 0xCD;
            out += 0x9D;
            if(!convertToUnicode(out, str.substr(first_codepoint_size, second_codepoint_size), script)) return false;
            return true;
        }

        return false;
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
        painter.drawSymbol("˘", x+hoffset, y+should_drop*drop, width, voffset + extra);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTBREVE_H
