#ifndef TYPESET_NRT_H
#define TYPESET_NRT_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "typeset_command.h"
#include "typeset_controller.h"
#include "typeset_model.h"

namespace Forscape {

namespace Typeset {

class Sqrt;

class Nrt final : public Construct {
private:
    double line_height;
    double uptick_height = 6;
    double downtick_height = 2.5;
    double extra_width;
    double root_voffset;

public:
    Nrt(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return NRT; }
    virtual void writePrefix(std::string& out) const noexcept override { out += NRT_STR; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t id) const noexcept override{
        return id == 0;
    }

    static constexpr double vgap = 1;
    static constexpr double SLOPE = 5;
    static constexpr double vfudge = 2;
    static constexpr double hfudge = 0.4;
    static constexpr double MARGIN_LEFT = 1.2;
    static constexpr double MARGIN_RIGHT = 2;
    static constexpr double SCRIPT_OVERLAP = 4;
    static constexpr double RATIO = 4.0/6;

    virtual void updateSizeFromChildSizes() noexcept override {
        above_center = second()->above_center + vgap;

        if(first()->height()/2 < above_center*RATIO){
            root_voffset = first()->height()/2 - above_center*(1-RATIO);
        }else{
            root_voffset = first()->height()/2 - above_center*(1-RATIO);
        }
        above_center += root_voffset;

        under_center = second()->under_center;
        line_height = height() - vfudge;
        extra_width = MARGIN_LEFT + MARGIN_RIGHT + (line_height + uptick_height + downtick_height) / SLOPE;
        width = first()->width - SCRIPT_OVERLAP + second()->width + extra_width;
    }

    virtual void updateChildPositions() noexcept override {
        first()->x = x;
        first()->y = y;
        second()->x = x + first()->width - SCRIPT_OVERLAP + extra_width + hfudge - MARGIN_RIGHT;
        second()->y = y + vgap + root_voffset;
    }

    virtual void paintSpecific(Painter& painter) const override {
        std::vector<std::pair<double, double> > pts;
        double a = x+width-MARGIN_RIGHT;
        double b = y + root_voffset;
        pts.push_back( std::make_pair(a, b) );
        a = x+extra_width-MARGIN_RIGHT + first()->width - SCRIPT_OVERLAP;
        pts.push_back( std::make_pair(a, b) );
        a -= line_height/SLOPE;
        b = y+line_height;
        pts.push_back( std::make_pair(a, b) );
        a -= uptick_height/SLOPE;
        b = y+line_height-uptick_height;
        pts.push_back( std::make_pair(a,b) );
        a = x + MARGIN_LEFT + first()->width - SCRIPT_OVERLAP;
        b = y+line_height-uptick_height+downtick_height;
        pts.push_back( std::make_pair(a,b) );
        painter.drawPath(pts);
    }

    static void modifySecondScript(Construct* con, Controller& c, Subphrase*){
        Nrt* m = debug_cast<Nrt*>(con);
        Command* cmd = new ReplaceConstruct1vs2<Sqrt, Nrt, false>(m);
        c.getModel()->mutate(cmd, c);
    }

    static const std::vector<Construct::ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override {
        return actions;
    }
    #endif
};

#ifndef FORSCAPE_TYPESET_HEADLESS
const std::vector<Construct::ContextAction> Nrt::actions {
    ContextAction("Remove nth root script", modifySecondScript),
};
#endif

}

}

#endif // TYPESET_NRT_H
