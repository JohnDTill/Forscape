#ifndef TYPESET_MARKERLINK_H
#define TYPESET_MARKERLINK_H

#include "typeset_construct.h"
#include "typeset_line.h"

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

    size_t serialChars() const noexcept override {
        return std::string("Line :").size() + std::to_string(line_id).size();
    }

    virtual void writeString(std::string& out, size_t& curr) const noexcept override {
        out[curr++] = 'L';
        out[curr++] = 'i';
        out[curr++] = 'n';
        out[curr++] = 'e';

        std::string num = std::to_string(line_id+1);
        for(const char ch : num) out[curr++] = ch;

        out[curr++] = ':';
    }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    MarkerLink(Line* line, View* view, Model* model) : line_id(line->id), view(view), model(model) {}

    void clickThrough() const {
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
    #endif
};

}

}

#endif // TYPESET_MARKERLINK_H
