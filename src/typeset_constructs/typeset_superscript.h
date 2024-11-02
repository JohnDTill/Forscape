#ifndef TYPESET_SUPERSCRIPT_H
#define TYPESET_SUPERSCRIPT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

class Superscript final : public Construct { 
public:
    Superscript(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return SUPERSCRIPT; }
    virtual void writePrefix(std::string& out) const noexcept override { out += SUPERSCRIPT_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        if(script != 0) return false;
        return child()->writeUnicode(out, 1);
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }

    void updateSizeFromTextLeft(const Text* t) noexcept {
        if(child()->height()/2 < t->aboveCenter()*3/4)
            above_center = t->aboveCenter()*3/4 + child()->height()/2;
        else
            above_center = child()->height();
    }

    void updateSizeFromConstructLeft(const Construct* c) noexcept {
        if(child()->height()/2 < c->above_center*5/6)
            above_center = c->above_center*5/6 + child()->height()/2;
        else
            above_center = child()->height();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        width = child()->width;
        under_center = 0;

        Text* t = prev();

        if(!t->empty())
            updateSizeFromTextLeft(t);
        else if(Construct* c = t->prevConstructInPhrase())
            updateSizeFromConstructLeft(c);
        else
            above_center = child()->height();
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x;
        child()->y = y;
    }
    #endif
};

}

}

#endif // TYPESET_SUPERSCRIPT_H
