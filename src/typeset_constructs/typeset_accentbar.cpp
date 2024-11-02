#include "typeset_accentbar.h"

#include "typeset_subphrase.h"
#include "forscape_serial_unicode.h"

namespace Forscape {

namespace Typeset {

AccentBar::AccentBar(){
    setupUnaryArg();
}

char AccentBar::constructCode() const noexcept { return ACCENTBAR; }

void AccentBar::writePrefix(std::string& out) const noexcept {
    out += ACCENTBAR_STR;
}

bool AccentBar::writeUnicode(std::string& out, int8_t script) const noexcept {
    std::string child_str;
    if(!child()->writeUnicode(child_str, script)) return false;
    if(child_str.empty()){
        out += "‾";
        return true;
    }else if(codepointSize(child_str[0]) == child_str.size()){
        out += child_str;
        out += 0xCC;
        out += 0x85;
    }else for(size_t i = 0; i < child_str.size();){
        // Combining Double Macron
        if(i!=0){
            out += 0xCD;
            out += 0x9E;
        }
        const size_t codepoint_size = codepointSize(child_str[i]);
        for(size_t j = 0; j < codepoint_size; j++)
            out += child_str[i++];
    }

    return true;
}

#ifndef FORSCAPE_TYPESET_HEADLESS
void AccentBar::updateSizeFromChildSizes() noexcept {
    width = child()->width;
    under_center = child()->under_center;
    should_drop = hasSmallChild();
    above_center = child()->above_center + !should_drop * voffset;
}

void AccentBar::updateChildPositions() noexcept {
    child()->x = x;
    child()->y = y + !should_drop*voffset;
}

void AccentBar::paintSpecific(Painter& painter) const {
    painter.drawLine(x+hoffset, y+should_drop*drop, width, 0);
}
#endif

}

}
