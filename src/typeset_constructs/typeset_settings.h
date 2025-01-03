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

namespace Forscape {

namespace Code {
class Settings;
}

namespace Typeset {

class Settings final : public Construct {
private:
    static constexpr size_t label_glyph_length = 12;
    bool expanded = false;

public:
    std::vector<Code::Settings::Update> updates;
    std::array<SettingValue, NUM_CODE_SETTINGS> inherited;
    virtual char constructCode() const noexcept override;
    virtual void writePrefix(std::string& out) const noexcept override;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual void onClick(Controller& controller, double local_x, double local_y) noexcept override;
    virtual void onDoubleClick(Controller& controller, double local_x, double local_y) noexcept override;
    virtual void updateSizeFromChildSizes() noexcept override;
    virtual void paintSpecific(Painter& painter) const override;
    static void changeSettings(Construct* con, Controller& c, Subphrase*);
    static void changeSettings(Construct* con, Controller& c);
    static void expandCollapse(Construct* con, Controller& c, Subphrase*);
    static void expandCollapse(Construct* con, Controller& c) noexcept;
    static const std::vector<Construct::ContextAction> actions;
    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept override;
    std::string getTooltip(double x_local, double y_local) const alloc_except;
    bool isExpanded() const noexcept;
    void updateString() alloc_except;

private:
    std::string str;
    size_t chars_left = 0;
    size_t chars_right = 0;
    #endif
};

}

}

#endif // TYPESET_SETTINGS_H
