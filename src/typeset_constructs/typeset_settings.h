#ifndef TYPESET_SETTINGS_H
#define TYPESET_SETTINGS_H

#include "typeset_construct.h"
#include "forscape_dynamic_settings.h"

#ifndef FORSCAPE_TYPESET_HEADLESS
#include <typeset_command_change_settings.h>
#include <typeset_controller.h>
#include <typeset_model.h>
#include <typeset_settings_dialog.h>
#endif

//DO THIS: should open on click or double-click, maybe have a button for expand/condense

namespace Forscape {

namespace Code {
class Settings;
}

namespace Typeset {

class Settings final : public Construct {
private:
    static constexpr std::string_view label = "Settings";
    static constexpr size_t label_glyph_length = label.size();
    bool expanded = false;

public:
    std::vector<Code::Settings::Update> updates;
    virtual char constructCode() const noexcept override;
    virtual void writeArgs(std::string& out, size_t& curr) const noexcept override;
    virtual size_t dims() const noexcept override;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual void updateSizeFromChildSizes() noexcept override;
    virtual void paintSpecific(Painter& painter) const override;
    static void changeSettings(Construct* con, Controller& c, Subphrase*);
    static void expandCollapse(Construct* con, Controller&, Subphrase*);
    static const std::vector<Construct::ContextAction> actions;
    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override;
    std::string getString() const alloc_except;
    bool isExpanded() const noexcept;
    void updateString() alloc_except;

private:
    std::string str;
    size_t expanded_chars = 0;
    #endif
};

}

}

#endif // TYPESET_SETTINGS_H
