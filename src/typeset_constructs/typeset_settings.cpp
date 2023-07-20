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
static bool isInExpandBox(double local_x, double local_y, uint8_t script_depth) noexcept {
    return local_y <= CHARACTER_HEIGHTS[script_depth] && local_x <= 1.5*CHARACTER_WIDTHS[script_depth];
}

static bool isInFormBox(double local_x, double local_y, uint8_t script_depth) noexcept {
    return local_y <= CHARACTER_HEIGHTS[script_depth] &&
           local_x > 1.5*CHARACTER_WIDTHS[script_depth] &&
           local_x <= 3.5*CHARACTER_WIDTHS[script_depth];
}

void Settings::onClick(Controller& controller, double local_x, double local_y) noexcept {
    local_x -= HSPACE;

    if(isInExpandBox(local_x, local_y, scriptDepth())) expandCollapse(this, controller);
    else if(isInFormBox(local_x, local_y, scriptDepth())) changeSettings(this, controller);
    else Construct::onClick(controller, local_x, local_y);
}

void Settings::onDoubleClick(Controller& controller, double local_x, double local_y) noexcept {
    local_x -= HSPACE;

    if(isInExpandBox(local_x, local_y, scriptDepth())) expandCollapse(this, controller);
    else changeSettings(this, controller);
}

const std::vector<Construct::ContextAction> Settings::actions {
    ContextAction("View/change settings", changeSettings),
    ContextAction("Expand/collapse", expandCollapse),
};

static constexpr size_t OFFSET = 2;

void Settings::updateSizeFromChildSizes() noexcept {
    width = label_glyph_length * CHARACTER_WIDTHS[scriptDepth()];
    if(expanded) width = std::max(width, (expanded_chars+OFFSET) * CHARACTER_WIDTHS[scriptDepth()]);
    width += 2*HSPACE;

    const double row_heights = expanded*(updates.size() * (VSPACE + CHARACTER_HEIGHTS[scriptDepth()]));
    above_center = ABOVE_CENTER[scriptDepth()] + row_heights/2;
    under_center = UNDER_CENTER[scriptDepth()] + row_heights/2;
}

void Settings::paintSpecific(Painter& painter) const {
    painter.setTypeIfAppropriate(SEM_KEYWORD);

    painter.drawText(x+HSPACE, y-1, "⃞ ⃞");

    if(isExpanded()){
        painter.drawText(x+HSPACE, y, "-  Settings");
        double rolling_y = y;
        for(const auto& update : updates){
            std::string_view setting_id_str = setting_id_strs[update.setting_id];
            std::string_view warning_str = warning_strs[update.prev_value];

            rolling_y += VSPACE + CHARACTER_HEIGHTS[scriptDepth()];
            painter.drawText(
                x+HSPACE + OFFSET*CHARACTER_WIDTHS[scriptDepth()],
                rolling_y,
                setting_id_str.data() + std::string(": ") + warning_str.data());
        }
    }else{
        painter.drawText(x+HSPACE, y, "+  Settings");
    }
}

void Settings::changeSettings(Construct* con, Controller& c, Subphrase*){
    changeSettings(con, c);
}

void Settings::changeSettings(Construct* con, Controller& c) {
    c.deselect();
    Settings* settings = debug_cast<Settings*>(con);

    if( !SettingsDialog::execSettingsForm(settings->updates) ) return;

    CommandChangeSettings* cmd = new CommandChangeSettings(settings, settings->updates);
    SettingsDialog::populateSettingsFromForm(cmd->stale_updates);

    c.getModel()->mutate(cmd, c);
}

void Settings::expandCollapse(Construct* con, Controller& c, Subphrase*){
    expandCollapse(con, c);
}

void Settings::expandCollapse(Construct* con, Controller& c) noexcept {
    c.deselect();
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
