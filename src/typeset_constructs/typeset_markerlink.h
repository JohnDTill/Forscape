#ifndef TYPESET_MARKERLINK_H
#define TYPESET_MARKERLINK_H

#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_model.h"

#ifndef FORSCAPE_TYPESET_HEADLESS
#include "typeset_view.h"
#endif

namespace Forscape {

namespace Typeset {

class MarkerLink final : public Construct {
private:
    size_t line_id;
    View* view;
    Model* model;

public:
    MarkerLink() : line_id(0), view(nullptr), model(nullptr) {}
    virtual char constructCode() const noexcept override { return MARKERLINK; }
    virtual void writePrefix(std::string& out) const noexcept override { /* DO NOTHING */ }

    size_t serialChars() const noexcept override {
        return std::string("Line :").size() + std::to_string(line_id).size();
    }

    virtual void writeString(std::string& out) const noexcept override {
        out += "Line";
        out += std::to_string(line_id+1);
        out += ':';
    }

    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        assert(script == 0);
        writeString(out);
        return true;
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    MarkerLink(Line* line, View* view, Model* model) : line_id(line->id), view(view), model(model) {}

    virtual void onClick(Controller& controller, double, double) noexcept override {
        controller.selectConstruct(this);
        controller.deselect();
        debug_cast<Editor*>(view)->setFocus(Qt::FocusReason::ShortcutFocusReason);
        debug_cast<Editor*>(view)->clickLink(model, line_id);
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        assert(scriptDepth() == 0);
        width = CHARACTER_WIDTHS[0] * (6 + std::to_string(line_id+1).size());
        above_center = prev()->aboveCenter();
        under_center = prev()->underCenter();
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.setType(SEM_LINK);
        painter.drawText(x, y, "Line " + std::to_string(line_id+1) + ':');
        painter.drawLine(x, y+height(), width, 0);
    }

    std::string getTooltip(double x_local, double y_local) const alloc_except {
        return model->path.u8string();
    }
    #endif
};

}

}

#endif // TYPESET_MARKERLINK_H
