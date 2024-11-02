#ifndef TYPESET_CASES_H
#define TYPESET_CASES_H

#include "typeset_construct.h"

#include "typeset_command.h"
#include "typeset_controller.h"
#include "typeset_model.h"
#include "typeset_subphrase.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Typeset {

class Cases final : public Construct {
private:
    double v_width;
    double c_width;
    double ellipse_width;
    static constexpr double ELEMENT_HPADDING = 16;
    static constexpr double COMMA_OFFSET = 2;
    static constexpr double ELEMENT_VPADDING = 2;
    size_t nRows() const noexcept { return numArgs()/2; }

public:
    Cases(uint8_t n){
        setupNAargs(2*n);
    }

    virtual char constructCode() const noexcept override { return CASES; }
    virtual void writePrefix(std::string& out) const noexcept override {
        out += "{";
        out += std::to_string(nRows());
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id >= 2 ? arg(caller->id - 2)->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id + 2 < numArgs() ? arg(caller->id + 2)->textLeftOf(x) : next();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        ellipse_width = prev()->height()*3/4;

        v_width = 0;
        c_width = 0;
        double h = ELEMENT_VPADDING * (nRows()-1);
        for(size_t i = 0; i < numArgs(); i+=2){
            Subphrase* v = arg(i);
            Subphrase* c = arg(i+1);

            v_width = std::max(v_width, v->width);
            c_width = std::max(c_width, c->width);
            h += std::max(v->above_center, c->above_center);
            h += std::max(v->under_center, c->under_center);
        }

        width = ellipse_width + v_width + c_width + ELEMENT_HPADDING;
        above_center = under_center = h/2;
    }

    virtual void updateChildPositions() noexcept override {
        double yc = y;
        for(size_t i = 0; i < numArgs(); i+=2){
            Subphrase* v = arg(i);
            Subphrase* c = arg(i+1);

            double above_center = std::max(v->above_center, c->above_center);
            v->y = yc + above_center - v->above_center;
            c->y = yc + above_center - c->above_center;
            yc += ELEMENT_VPADDING + above_center + std::max(v->under_center, c->under_center);

            v->x = x + ellipse_width + (v_width - v->width)/2;
            c->x = x + (width-c_width) + (c_width - c->width)/2;
        }
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.setType(SEM_DEFAULT);
        painter.drawElipse(x, y, ellipse_width, height());

        painter.setScriptLevel(parent->script_level);
        double offset = DESCENT[scriptDepth()];
        double xp = x+width-c_width-(ELEMENT_HPADDING-COMMA_OFFSET);
        for(size_t i = 0; i < numArgs(); i+=2){
            Subphrase* v = arg(i);
            double y = v->y + v->above_center - offset;
            painter.drawText(xp, y, ",");
        }
    }

    static std::vector<ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase* child) const noexcept override {
        if(child == nullptr) return no_actions;
        actions[2].enabled = (numArgs() > 4);
        return actions;
    }

    void insertRow(size_t row, Subphrase* first, Subphrase* second){
        args.resize(args.size() + 2);

        for(size_t i = args.size()-1; i > 2*row+1; i--){
            args[i] = args[i-2];
            args[i]->id = i;
        }

        args[2*row] = first;
        args[2*row + 1] = second;
        first->id = 2*row;
        second->id = 2*row + 1;
    }

    void removeRow(size_t row) noexcept {
        for(size_t i = 2*row+2; i < args.size(); i++){
            args[i-2] = args[i];
            args[i]->id = i-2;
        }

        args.resize(args.size() - 2);
    }

    template<bool is_insert>
    class CmdRow : public Command{
    private:
        bool owning;
        Cases& cases;
        const size_t row;
        Subphrase* first;
        Subphrase* second;

        public:
            CmdRow(Cases& cases, size_t row)
                : cases(cases), row(row){

                if(is_insert){
                    first = new Subphrase;
                    first->setParent(&cases);
                    second = new Subphrase;
                    second->setParent(&cases);
                }else{
                    first = cases.arg(2*row);
                    second = cases.arg(2*row+1);
                }
            }

            ~CmdRow(){
                if(owning){
                    delete first;
                    delete second;
                }
            }

        private:
            void insert(Controller& c){
                owning = false;
                cases.insertRow(row, first, second);
                c.setBothToBackOf(second->back());
            }

            void remove(Controller& c) noexcept {
                owning = true;
                cases.removeRow(row);
                c.setBothToFrontOf(cases.arg(2*(row-1))->front());
            }

            virtual void redo(Controller& c) override final{
                if(is_insert) insert(c);
                else remove(c);
            }

            virtual void undo(Controller& c) override final{
                if(is_insert) remove(c);
                else insert(c);
            }
    };

    template<bool insert, bool offset>
    static void rowCmd(Construct* con, Controller& c, Subphrase* child){
        assert(dynamic_cast<Cases*>(con));
        assert(child != nullptr);
        Cases* cases = static_cast<Cases*>(con);
        size_t clicked_row = child->id / 2;
        Command* cmd = new CmdRow<insert>(*cases, clicked_row+offset);
        c.getModel()->mutate(cmd, c);
    }
    #endif
};

#ifndef FORSCAPE_TYPESET_HEADLESS
inline std::vector<Cases::ContextAction> Cases::actions {
    ContextAction("Create row below", rowCmd<true, true>),
    ContextAction("Create row above", rowCmd<true, false>),
    ContextAction("Delete row", rowCmd<false, false>),
};
#endif

}

}

#endif // TYPESET_CASES_H
