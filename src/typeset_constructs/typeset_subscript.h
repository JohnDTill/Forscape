#ifndef TYPESET_SUBSCRIPT_H
#define TYPESET_SUBSCRIPT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

class Subscript final : public Construct { 
public:
    Subscript(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return SUBSCRIPT; }
    virtual void writePrefix(std::string& out) const noexcept override { out += SUBSCRIPT_STR; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }

    void updateSizeFromTextLeft(const Text* t) noexcept {
        if(child()->height()/2 < t->underCenter()*3/4)
            under_center = t->underCenter()*3/4 + child()->height()/2;
        else
            under_center = child()->height();
    }

    void updateSizeFromConstructLeft(const Construct* c) noexcept {
        if(child()->height()/2 < c->under_center*5/6)
            under_center = c->under_center*5/6 + child()->height()/2;
        else
            under_center = child()->height();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        width = child()->width;
        above_center = 0;

        Text* t = prev();

        if(!t->empty())
            updateSizeFromTextLeft(t);
        else if(Construct* c = t->prevConstructInPhrase())
            updateSizeFromConstructLeft(c);
        else
            under_center = child()->height();
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x;
        child()->y = y + height()-child()->height();
    }
    #endif
};

}

}

#endif // TYPESET_SUBSCRIPT_H
