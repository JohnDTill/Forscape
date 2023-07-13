#include "typeset_settings.h"

namespace Forscape {

namespace Typeset {

char Settings::constructCode() const noexcept { return SETTINGS; }

/* DO THIS: support writing
void Settings::writeArgs(std::string& out, size_t& curr) const noexcept {
    for(const Code::Settings::Update update : updates){
        static_assert(Code::SettingId::NUM_SETTINGS <= std::numeric_limits<uint8_t>::max());
        out[curr++] = static_cast<uint8_t>(update.setting_id);
        static_assert(sizeof(Code::Settings::SettingValue) <= sizeof(uint8_t));
        out[curr++] = static_cast<uint8_t>(update.prev_value);
    }
    out[curr++] = std::numeric_limits<uint8_t>::max();
}

size_t Settings::dims() const noexcept {
    return 2 * updates.size() + 1;
}
*/

#ifndef FORSCAPE_TYPESET_HEADLESS
const std::vector<Construct::ContextAction> Settings::actions {
    ContextAction("View/change settings", changeSettings),
            ContextAction("Expand/collapse", expandCollapse),
};

void Settings::updateSizeFromChildSizes() noexcept {
    width = label_glyph_length * CHARACTER_WIDTHS[scriptDepth()];
    above_center = ABOVE_CENTER[scriptDepth()];
    under_center = UNDER_CENTER[scriptDepth()];
}

void Settings::paintSpecific(Painter& painter) const {
    painter.drawText(x, y, label);
    painter.drawSettings(x, y, width, height());
}

void Settings::changeSettings(Construct* con, Controller& c, Subphrase*){
    Settings* settings = debug_cast<Settings*>(con);

    if( !SettingsDialog::execSettingsForm(settings->updates) ) return;

    CommandChangeSettings* cmd = new CommandChangeSettings(settings, settings->updates);
    SettingsDialog::populateSettingsFromForm(cmd->stale_updates);
    c.getModel()->mutate(cmd, c);
}

void Settings::expandCollapse(Construct* con, Controller&, Subphrase*){
    Settings* settings = debug_cast<Settings*>(con);
    //DO THIS
}

const std::vector<Construct::ContextAction>& Settings::getContextActions(Subphrase*) const noexcept {
    return actions;
}
#endif

}

}
