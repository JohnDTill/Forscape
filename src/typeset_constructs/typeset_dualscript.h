#ifndef TYPESET_DUALSCRIPT_H
#define TYPESET_DUALSCRIPT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

class Dualscript final : public Construct { 
public:
    Dualscript(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return DUALSCRIPT; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }

    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id==1 ? first()->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id==0 ? second()->textLeftOf(x) : next();
    }

    void updateSizeFromTextLeft(const Text* t) noexcept {
        if(first()->height()/2 < t->aboveCenter()*3/4)
            above_center = t->aboveCenter()*3/4 + first()->height()/2;
        else
            above_center = first()->height();

        if(second()->height()/2 < t->underCenter()*3/4)
            under_center = t->underCenter()*3/4 + second()->height()/2;
        else
            under_center = second()->height();
    }

    void updateSizeFromConstructLeft(const Construct* c) noexcept {
        if(first()->height()/2 < c->above_center*5/6)
            above_center = c->above_center*5/6 + first()->height()/2;
        else
            above_center = first()->height();

        if(second()->height()/2 < c->under_center*5/6)
            under_center = c->under_center*5/6 + second()->height()/2;
        else
            under_center = second()->height();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        width = std::max(first()->width, second()->width);

        Text* t = prev();

        if(!t->empty())
            updateSizeFromTextLeft(t);
        else if(Construct* c = t->prevConstructInPhrase())
            updateSizeFromConstructLeft(c);
        else{
            above_center = first()->height();
            under_center = second()->height();
        }
    }

    virtual void updateChildPositions() noexcept override {
        first()->x = x;
        first()->y = y;

        second()->x = x;
        second()->y = y + height()-second()->height();
    }
    #endif
};

}

}

#endif // TYPESET_DUALSCRIPT_H
