#ifndef TYPESET_SUP_H
#define TYPESET_SUP_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

class Sup final : public Construct { 
public:
private:
    static constexpr std::string_view word = "sup";
    double word_offset;
    double child_offset;

public:
    Sup(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return SUP; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }

    virtual void updateSizeFromChildSizes() noexcept override {
        double word_width = CHARACTER_WIDTHS[scriptDepth()]*word.size();
        if(word_width > child()->width){
            width = word_width;
            word_offset = 0;
            child_offset = (word_width - child()->width)/2;
        }else{
            width = child()->width;
            child_offset = 0;
            word_offset = (child()->width - word_width)/2;
        }
        above_center = prev()->aboveCenter();
        under_center = prev()->underCenter() + child()->height();
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x + child_offset;
        child()->y = y + prev()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawText(x + word_offset, y, std::string(word.data(), word.size()));
    }
    #endif
};

}

}

#endif // TYPESET_SUP_H
