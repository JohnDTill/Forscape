#ifndef TYPESET_BIG_SYMBOL_H
#define TYPESET_BIG_SYMBOL_H

#include "typeset_construct.h"
#include "typeset_integral_preference.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

inline constexpr std::string_view getBigSymbolString(size_t type) noexcept {
    switch(type){
        case BIGPROD0: case BIGPROD1: case BIGPROD2: return "∏";
        case BIGSUM0: case BIGSUM1: case BIGSUM2: return "∑";
        case BIGCOPROD0: case BIGCOPROD1: case BIGCOPROD2: return "∐";
        case BIGINTERSECTION0: case BIGINTERSECTION1: return "⋂";
        case BIGUNION0: case BIGUNION1: case BIGUNION2: return "⋃";
        default: assert(false); return "";
    }
}

template<size_t type>
class BigSymbol0 final : public Construct {
public:
    virtual char constructCode() const noexcept override { return type; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override {
        width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
        above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()];
        under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()]*0.9;
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol(x, y, getBigSymbolString(type));
    }
    #endif
};

template<size_t type>
class BigSymbol1 final : public Construct {
public:
    BigSymbol1(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return type; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual void updateSizeFromChildSizes() noexcept override {
        symbol_width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
        width = std::max(symbol_width, child()->width);
        above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()];
        under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()]*0.9 + child()->height();
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x + (width - child()->width)/2;
        child()->y = y + height() - child()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        double symbol_x = x + (width - symbol_width) / 2;
        painter.drawSymbol(symbol_x, y, getBigSymbolString(type));
    }
    #endif
};

template<size_t type>
class BigSymbol2 final : public Construct {
public:
    BigSymbol2(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return type; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id==1 ? first()->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id==0 ? second()->textLeftOf(x) : next();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        symbol_width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
        width = std::max(symbol_width, std::max(first()->width, second()->width));
        above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()] + first()->height();
        under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()]*0.9 + second()->height();
    }

    virtual void updateChildPositions() noexcept override {
        first()->x = x + (width - first()->width)/2;
        first()->y = y;
        second()->x = x + (width - second()->width)/2;
        second()->y = y + height() - second()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        double symbol_x = x + (width - symbol_width) / 2;
        painter.drawSymbol(symbol_x, y + second()->height(), getBigSymbolString(type));
    }
    #endif
};

typedef BigSymbol0<BIGCOPROD0> BigCoProd0;
typedef BigSymbol0<BIGINTERSECTION0> BigIntersection0;
typedef BigSymbol0<BIGPROD0> BigProd0;
typedef BigSymbol0<BIGSUM0> BigSum0;
typedef BigSymbol0<BIGUNION0> BigUnion0;

typedef BigSymbol1<BIGCOPROD1> BigCoProd1;
typedef BigSymbol1<BIGINTERSECTION1> BigIntersection1;
typedef BigSymbol1<BIGPROD1> BigProd1;
typedef BigSymbol1<BIGSUM1> BigSum1;
typedef BigSymbol1<BIGUNION1> BigUnion1;

typedef BigSymbol2<BIGCOPROD2> BigCoProd2;
typedef BigSymbol2<BIGINTERSECTION2> BigIntersection2;
typedef BigSymbol2<BIGPROD2> BigProd2;
typedef BigSymbol2<BIGSUM2> BigSum2;
typedef BigSymbol2<BIGUNION2> BigUnion2;

}

}

#endif // TYPESET_BIG_SYMBOL_H
