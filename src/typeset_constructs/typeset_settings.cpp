#include "typeset_settings.h"

#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

static constexpr double VSPACE = 2.5;
static constexpr double HSPACE = 2.5;

//DO THIS: codegen
static constexpr std::array<std::string_view, Code::NUM_SETTINGS> setting_id_strs = { "Unused var", "Transpose T" };
static constexpr std::array<std::string_view, Code::NUM_WARNING_LEVELS> warning_strs = {"No Warning", "Warning", "Error"};

char Settings::constructCode() const noexcept { return SETTINGS; }

void Settings::writeArgs(std::string& out, size_t& curr) const noexcept {
    for(const Code::Settings::Update update : updates){
        static_assert(Code::SettingId::NUM_SETTINGS <= 0b01111111);
        out[curr++] = static_cast<uint8_t>(update.setting_id) + 1;
        static_assert(sizeof(Code::Settings::SettingValue) <= sizeof(uint8_t));
        out[curr++] = static_cast<uint8_t>(update.prev_value) + 1;
    }
    out[curr++] = 0b01111111;
}

size_t Settings::dims() const noexcept {
    return 2 * updates.size() + 1;
}

#ifndef FORSCAPE_TYPESET_HEADLESS
const std::vector<Construct::ContextAction> Settings::actions {
    ContextAction("View/change settings", changeSettings),
    ContextAction("Expand/collapse", expandCollapse),
};

void Settings::updateSizeFromChildSizes() noexcept {
    width = (label_glyph_length + 3*!expanded) * CHARACTER_WIDTHS[scriptDepth()];
    if(expanded) width = std::max(width, expanded_chars * CHARACTER_WIDTHS[scriptDepth()]);
    width += 2*HSPACE;

    const double row_heights = expanded*(updates.size() * (VSPACE + CHARACTER_HEIGHTS[scriptDepth()]));
    above_center = ABOVE_CENTER[scriptDepth()] + row_heights/2;
    under_center = UNDER_CENTER[scriptDepth()] + row_heights/2;
}

void Settings::paintSpecific(Painter& painter) const {
    painter.setTypeIfAppropriate(SEM_KEYWORD);

    painter.drawLine(x, y-2, width, 0);
    painter.drawLine(x, y + CHARACTER_HEIGHTS[scriptDepth()], width, 0);
    painter.drawLine(x, y+height(), width, 0);
    painter.drawLine(x, y-2, 0, height()+2);
    painter.drawLine(x+width, y-2, 0, height()+2);

    if(isExpanded()){
        painter.drawText(x+HSPACE, y, label);
        double rolling_y = y;
        for(const auto& update : updates){
            std::string_view setting_id_str = setting_id_strs[update.setting_id];
            std::string_view warning_str = warning_strs[update.prev_value];

            rolling_y += VSPACE + CHARACTER_HEIGHTS[scriptDepth()];
            painter.drawText(x+HSPACE, rolling_y, setting_id_str.data() + std::string(": ") + warning_str.data());
        }
    }else{
        const std::string str = label.data() + std::string("...");
        painter.drawText(x+HSPACE, y, str);
    }
}

void Settings::changeSettings(Construct* con, Controller& c, Subphrase*){
    Settings* settings = debug_cast<Settings*>(con);

    if( !SettingsDialog::execSettingsForm(settings->updates) ) return;

    CommandChangeSettings* cmd = new CommandChangeSettings(settings, settings->updates);
    SettingsDialog::populateSettingsFromForm(cmd->stale_updates);

    c.getModel()->mutate(cmd, c);
}

void Settings::expandCollapse(Construct* con, Controller& c, Subphrase*){
    Settings* settings = debug_cast<Settings*>(con);
    settings->expanded = !settings->expanded;
    c.getModel()->postmutate();
}

const std::vector<Construct::ContextAction>& Settings::getContextActions(Subphrase*) const noexcept {
    return actions;
}

std::string Settings::getString() const alloc_except {
    return str;
}

bool Settings::isExpanded() const noexcept {
    return expanded;
}

void Settings::updateString() alloc_except {
    expanded_chars = 0;

    if(updates.empty()){
        str = "No changes";
        return;
    }

    str.clear();
    for(const auto& update : updates){
        std::string_view setting_id_str = setting_id_strs[update.setting_id];
        std::string_view warning_str = warning_strs[update.prev_value];

        str += setting_id_str;
        str += ':';
        str += ' ';
        str += warning_str;
        str += '\n';
        expanded_chars = std::max(expanded_chars, setting_id_str.size() + warning_str.size());
    }
    str.pop_back();
    expanded_chars += 2;
}
#endif

}

}
