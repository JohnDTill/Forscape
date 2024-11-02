#ifndef TYPESET_ACCENTTILDE_H
#define TYPESET_ACCENTTILDE_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

namespace Typeset {

class AccentTilde final : public Construct {
private:
    static constexpr double hoffset = 1;
    static constexpr double voffset = 1;
    static constexpr double extra = 7;
    static constexpr double symbol_offset = -2;
    static constexpr double drop = 2;
    bool should_drop;

public:
    AccentTilde(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return ACCENTTILDE; }
    virtual void writePrefix(std::string& out) const noexcept override { out += ACCENTTILDE_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        if(child()->hasConstructs()) return false;
        const std::string& str = child()->front()->getString();
        if(str.empty()){
            out += "˜";
            return true;
        }
        const size_t first_codepoint_size = codepointSize(str[0]);
        if(first_codepoint_size == str.size()){
            if(!convertToUnicode(out, str, script)) return false;
            out += 0xCC;
            out += 0x83;
            return true;
        }
        const size_t second_codepoint_size = codepointSize(str[first_codepoint_size]);
        if(first_codepoint_size + second_codepoint_size == str.size()){
            // Combining Double Tilde
            if(!convertToUnicode(out, str.substr(0, first_codepoint_size), script)) return false;
            out += 0xCD;
            out += 0xA0;
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
        painter.drawSymbol('~', x+hoffset, y + symbol_offset + should_drop*drop, width, voffset + extra);
    }
    #endif
};

}

}

#endif // TYPESET_ACCENTTILDE_H
