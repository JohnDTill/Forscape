#ifndef TYPESET_BIG_INTEGRAL_H
#define TYPESET_BIG_INTEGRAL_H

#include "typeset_construct.h"

#include "typeset_command_replace_construct.h"
#include "typeset_integral_preference.h"
#include "typeset_subphrase.h"

namespace Forscape {

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
inline void writePrefixBigInt(std::string& out) noexcept {
    switch(type){
        case INTEGRAL0: out += INTEGRAL0_STR; break;
        case INTEGRAL1: out += INTEGRAL1_STR; break;
        case INTEGRAL2: out += INTEGRAL2_STR; break;
        case DOUBLEINTEGRAL0: out += DOUBLEINTEGRAL0_STR; break;
        case DOUBLEINTEGRAL1: out += DOUBLEINTEGRAL1_STR; break;
        case TRIPLEINTEGRAL0: out += TRIPLEINTEGRAL0_STR; break;
        case TRIPLEINTEGRAL1: out += TRIPLEINTEGRAL1_STR; break;
        case INTEGRALCONV0: out += INTEGRALCONV0_STR; break;
        case INTEGRALCONV1: out += INTEGRALCONV1_STR; break;
        case INTEGRALCONV2: out += INTEGRALCONV2_STR; break;
        case DOUBLEINTEGRALCONV0: out += DOUBLEINTEGRALCONV0_STR; break;
        case DOUBLEINTEGRALCONV1: out += DOUBLEINTEGRALCONV1_STR; break;
        case TRIPLEINTEGRALCONV0: out += TRIPLEINTEGRALCONV0_STR; break;
        case TRIPLEINTEGRALCONV1: out += TRIPLEINTEGRALCONV1_STR; break;
        default: assert(false);
    }
}

template<size_t type>
class BigIntegralSuper0 final : public Construct {
public:
    virtual char constructCode() const noexcept override { return type; }
    virtual void writePrefix(std::string& out) const noexcept override {
        writePrefixBigInt<type>(out);
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override {
        width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
        above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()];
        under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()];
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol(x, y, getBigIntegralString(type));
    }

    static void modifyFirstScript(Construct* con, Controller& c, Subphrase*);

    static const std::vector<Construct::ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override {
        return actions;
    }
    #endif
};

template<size_t type>
class BigIntegralSuper1 final : public Construct {
public:
    BigIntegralSuper1(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return type; }
    virtual void writePrefix(std::string& out) const noexcept override {
        writePrefixBigInt<type>(out);
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual void updateSizeFromChildSizes() noexcept override {
        symbol_width = BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()];
        above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()];

        if(integral_bounds_vertical){
            width = std::max(symbol_width, child()->width);
            under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()] + child()->height();
        }else{
            width = symbol_width + child()->width - INTEGRAL_SHIFT_LEFT*CHARACTER_WIDTHS[scriptDepth()];
            under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()] + child()->height()*INTEGRAL_RATIO - INTEGRAL_SHIFT_UP * BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()];
        }
    }

    virtual void updateChildPositions() noexcept override {
        if(integral_bounds_vertical){
            child()->x = x + (width - child()->width)/2;
            child()->y = y + height() - child()->height();
        }else{
            child()->x = x + BIG_SYM_SCALE*CHARACTER_WIDTHS[scriptDepth()] - INTEGRAL_SHIFT_LEFT*CHARACTER_WIDTHS[scriptDepth()];
            child()->y = y + height() - child()->height() - INTEGRAL_SHIFT_UP * BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()];
        }
    }

    virtual void paintSpecific(Painter& painter) const override {
        if(integral_bounds_vertical){
            double symbol_x = x + (width - symbol_width) / 2;
            painter.drawSymbol(symbol_x, y, getBigIntegralString(type));
        }else{
            painter.drawSymbol(x, y, getBigIntegralString(type));
        }
    }

    static void modifyFirstScript(Construct* con, Controller& c, Subphrase*){
        BigIntegralSuper1<type>* m = debug_cast<BigIntegralSuper1<type>*>(con);
        Command* cmd = new ReplaceConstruct<BigIntegralSuper1<type>, BigIntegralSuper0<type+(INTEGRAL0-INTEGRAL1)>>(m);
        c.getModel()->mutate(cmd, c);
    }

    static void modifySecondScript(Construct* con, Controller& c, Subphrase*);

    static const std::vector<Construct::ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override {
        return actions;
    }
    #endif
};

template<size_t type>
class BigIntegralSuper2 final : public Construct {
public:
    BigIntegralSuper2(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return type; }
    virtual void writePrefix(std::string& out) const noexcept override {
        writePrefixBigInt<type>(out);
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
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

        if(integral_bounds_vertical){
            width = std::max(symbol_width, std::max(first()->width, second()->width));
            above_center = BIG_SYM_SCALE*ABOVE_CENTER[scriptDepth()] + first()->height();
            under_center = BIG_SYM_SCALE*UNDER_CENTER[scriptDepth()] + second()->height();
        }else{
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

    static void modifySecondScript(Construct* con, Controller& c, Subphrase*){
        BigIntegralSuper2<type>* m = debug_cast<BigIntegralSuper2<type>*>(con);
        Command* cmd = new ReplaceConstruct1vs2<BigIntegralSuper1<type+(INTEGRAL1-INTEGRAL2)>, BigIntegralSuper2<type>, false>(m);
        c.getModel()->mutate(cmd, c);
    }

    static const std::vector<Construct::ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override {
        return actions;
    }
    #endif
};

typedef BigIntegralSuper0<DOUBLEINTEGRALCONV0> DoubleIntegralConv0;
typedef BigIntegralSuper0<DOUBLEINTEGRAL0> DoubleIntegral0;
typedef BigIntegralSuper0<INTEGRALCONV0> IntegralConv0;
typedef BigIntegralSuper0<INTEGRAL0> Integral0;
typedef BigIntegralSuper0<TRIPLEINTEGRALCONV0> TripleIntegralConv0;
typedef BigIntegralSuper0<TRIPLEINTEGRAL0> TripleIntegral0;

typedef BigIntegralSuper1<DOUBLEINTEGRALCONV1> DoubleIntegralConv1;
typedef BigIntegralSuper1<DOUBLEINTEGRAL1> DoubleIntegral1;
typedef BigIntegralSuper1<INTEGRALCONV1> IntegralConv1;
typedef BigIntegralSuper1<INTEGRAL1> Integral1;
typedef BigIntegralSuper1<TRIPLEINTEGRALCONV1> TripleIntegralConv1;
typedef BigIntegralSuper1<TRIPLEINTEGRAL1> TripleIntegral1;

typedef BigIntegralSuper2<INTEGRALCONV2> IntegralConv2;
typedef BigIntegralSuper2<INTEGRAL2> Integral2;

#ifndef FORSCAPE_TYPESET_HEADLESS
template<size_t type> const std::vector<Construct::ContextAction> BigIntegralSuper0<type>::actions {
    ContextAction("Add subscript", modifyFirstScript),
};

template<size_t type> const std::vector<Construct::ContextAction> BigIntegralSuper1<type>::actions  =
    (type == INTEGRAL1 || type == INTEGRALCONV1) ?
std::vector<Construct::ContextAction>({
    ContextAction("Add superscript", modifySecondScript),
    ContextAction("Remove subscript", modifyFirstScript),
}) : std::vector<Construct::ContextAction>({
    ContextAction("Remove subscript", modifyFirstScript),
});

template<size_t type> const std::vector<Construct::ContextAction> BigIntegralSuper2<type>::actions {
    ContextAction("Remove superscript", modifySecondScript),
};

template<size_t type> void BigIntegralSuper0<type>::modifyFirstScript(Construct* con, Controller& c, Subphrase*){
    BigIntegralSuper0<type>* m = debug_cast<BigIntegralSuper0<type>*>(con);
    Command* cmd = new ReplaceConstruct<BigIntegralSuper0<type>, BigIntegralSuper1<type-(INTEGRAL0-INTEGRAL1)>>(m);
    c.getModel()->mutate(cmd, c);
}

template<size_t type> void BigIntegralSuper1<type>::modifySecondScript(Construct* con, Controller& c, Subphrase*){
    BigIntegralSuper1<type>* m = debug_cast<BigIntegralSuper1<type>*>(con);
    Command* cmd = new ReplaceConstruct1vs2<BigIntegralSuper1<type>, BigIntegralSuper2<type-(INTEGRAL1-INTEGRAL2)>, true>(m);
    c.getModel()->mutate(cmd, c);
}
#endif

}

}

#endif // TYPESET_BIG_INTEGRAL_H
