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

//DO THIS: should reveal settings on hover
//DO THIS: should open on click or double-click, maybe have a button for expand/condense

namespace Forscape {

namespace Code {
class Settings;
}

namespace Typeset {

class Settings final : public Construct {
private:
    static constexpr std::string_view label = "⚒ Settings...";
    static constexpr size_t label_glyph_length = label.size() - std::string_view("⚒").size() + 1;

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
    #endif
};

}

}

#endif // TYPESET_SETTINGS_H
