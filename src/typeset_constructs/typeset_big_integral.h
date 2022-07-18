#ifndef TYPESET_BIG_INTEGRAL_H
#define TYPESET_BIG_INTEGRAL_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"
#include "typeset_integral_preference.h"

namespace Hope {

namespace Typeset {

inline constexpr std::string_view getBigIntegralString(size_t type) noexcept {
    switch(type){
        case INTEGRAL0: case INTEGRAL1: case INTEGRAL2: return "∫";
        case INTEGRALCONV0: case INTEGRALCONV1: case INTEGRALCONV2: return "∮";
        case DOUBLEINTEGRAL0: case DOUBLEINTEGRAL1: return "∬";
        case DOUBLEINTEGRALCONV0: case DOUBLEINTEGRALCONV1: return "∯";
        case TRIPLEINTEGRAL0: case TRIPLEINTEGRAL1: return "∭";
        case TRIPLEINTEGRALCONV0: case TRIPLEINTEGRALCONV1: return "∰";
        default: assert(false); return "";
    }
}

template<size_t type>
class BigIntegralSuper2 final : public Construct {
public:
    BigIntegralSuper2(){
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
        if(integral_bounds_vertical){
            symbol_width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
            width = std::max(symbol_width, std::max(first()->width, second()->width));
            above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()] + first()->height();
            under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()] + second()->height();
        }else{
            symbol_width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
            width = symbol_width + std::max(first()->width, second()->width - INTEGRAL_SHIFT_LEFT*CHARACTER_WIDTHS[scriptDepth()]);
            above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()] + first()->height()*INTEGRAL_RATIO;
            under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()] + second()->height()*INTEGRAL_RATIO - INTEGRAL_SHIFT_UP * BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()];
        }
    }

    virtual void updateChildPositions() noexcept override {
        if(integral_bounds_vertical){
            first()->x = x + (width - first()->width)/2;
            first()->y = y - 0.5*ABOVE_CENTER[scriptDepth()];
            second()->x = x + (width - second()->width)/2;
            second()->y = y + height() - second()->height();
        }else{
            first()->x = x + BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
            first()->y = y;
            second()->x = x + BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()] - INTEGRAL_SHIFT_LEFT*CHARACTER_WIDTHS[scriptDepth()];
            second()->y = y + height() - second()->height() - INTEGRAL_SHIFT_UP * BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()];
        }
    }

    virtual void paintSpecific(Painter& painter) const override {
        if(integral_bounds_vertical){
            double symbol_x = x + (width - symbol_width) / 2;
            painter.drawSymbol(symbol_x, y + second()->height(), getBigIntegralString(type));
        }else{
            double symbol_y = y + first()->height()*INTEGRAL_RATIO;
            painter.drawSymbol(x, symbol_y, getBigIntegralString(type));
        }
    }
    #endif
};

//DO THIS: finish consolidating big integrals

typedef BigIntegralSuper2<INTEGRALCONV2> IntegralConv2;
typedef BigIntegralSuper2<INTEGRAL2> Integral2;

}

}

#endif // TYPESET_BIG_INTEGRAL_H
