#ifndef TYPESET_SQRT_H
#define TYPESET_SQRT_H

#include "typeset_construct.h"

#include "typeset_command_replace_construct.h"
#include "typeset_subphrase.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Typeset {

class Nrt;

class Sqrt final : public Construct {
private:
    double line_height;
    double uptick_height = 6;
    double downtick_height = 2.5;
    double extra_width;

public:
    Sqrt(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return SQRT; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    static constexpr double vgap = 1;
    static constexpr double SLOPE = 5;
    static constexpr double vfudge = 2;
    static constexpr double hfudge = 0.4;
    static constexpr double MARGIN_LEFT = 1.2;
    static constexpr double MARGIN_RIGHT = 2;

    virtual void updateSizeFromChildSizes() noexcept override {
        above_center = child()->above_center + vgap;
        under_center = child()->under_center;
        line_height = height() - vfudge;
        extra_width = MARGIN_LEFT + MARGIN_RIGHT + (line_height + uptick_height + downtick_height) / SLOPE;
        width = child()->width + extra_width;
    }

    virtual void updateChildPositions() noexcept override {
        child()->x = x + extra_width + hfudge - MARGIN_RIGHT;
        child()->y = y + vgap;
    }

    virtual void paintSpecific(Painter& painter) const override {
        std::vector<std::pair<double, double> > pts;
        double a = x+width-MARGIN_RIGHT;
        double b = y;
        pts.push_back( std::make_pair(a, b) );
        a = x+extra_width-MARGIN_RIGHT;
        pts.push_back( std::make_pair(a, b) );
        a -= line_height/SLOPE;
        b = y+line_height;
        pts.push_back( std::make_pair(a, b) );
        a -= uptick_height/SLOPE;
        b = y+line_height-uptick_height;
        pts.push_back( std::make_pair(a,b) );
        a = x + MARGIN_LEFT;
        b = y+line_height-uptick_height+downtick_height;
        pts.push_back( std::make_pair(a,b) );
        painter.drawPath(pts);
    }

    static void modifySecondScript(Construct* con, Controller& c, Subphrase*);

    static const std::vector<Construct::ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override {
        return actions;
    }
    #endif
};

const std::vector<Construct::ContextAction> Sqrt::actions {
    ContextAction("Add nth root script", modifySecondScript),
};

void Sqrt::modifySecondScript(Construct* con, Controller& c, Subphrase*){
    Sqrt* m = debug_cast<Sqrt*>(con);
    Command* cmd = new ReplaceConstruct1vs2<Sqrt, Nrt, true>(m);
    c.getModel()->mutate(cmd, c);
}

}

}

#endif // TYPESET_SQRT_H
