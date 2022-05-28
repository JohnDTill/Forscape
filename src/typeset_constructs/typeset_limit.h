#ifndef TYPESET_LIMIT_H
#define TYPESET_LIMIT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class Limit final : public Construct {
private:
    double lim_offset;
    double first_offset;
    double second_offset;
    double child_above;

public:
    Limit(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return LIMIT; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }

    virtual void updateSizeFromChildSizes() noexcept override {
        double word_width = CHARACTER_WIDTHS[scriptDepth()]*3;
        double arrow_width = CHARACTER_WIDTHS[first()->script_level];
        double lower_width = first()->width + arrow_width + second()->width;
        if(word_width > lower_width){
            width = word_width;
            lim_offset = 0;
            first_offset = (word_width - lower_width)/2;
        }else{
            width = lower_width;
            first_offset = 0;
            lim_offset = (lower_width - word_width)/2;
        }
        second_offset = first_offset + first()->width + arrow_width;

        child_above = std::max(first()->above_center, second()->above_center);

        above_center = prev()->aboveCenter();
        under_center = prev()->underCenter() + child_above + std::max(first()->under_center, second()->under_center);
    }

    virtual void updateChildPositions() noexcept override {
        first()->x = x + first_offset;
        first()->y = y + prev()->height() + (child_above - first()->above_center);
        second()->x = x + second_offset;
        second()->y = y + prev()->height() + (child_above - second()->above_center);
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawText(x + lim_offset, y, "lim");
        painter.setScriptLevel(first()->script_level);
        painter.drawText(first()->x + first()->width, first()->y, "→");
    }
    #endif
};

}

}

#endif // TYPESET_LIMIT_H
