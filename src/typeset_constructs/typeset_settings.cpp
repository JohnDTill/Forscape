#include "typeset_settings.h"

#include "forscape_serial.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

static constexpr double VSPACE = 3.5;
static constexpr double HSPACE = 2.5;
static constexpr double INNER_COL_HSPACE = 5;

char Settings::constructCode() const noexcept { return SETTINGS; }

void Settings::writePrefix(std::string& out) const noexcept {
    out += SETTINGS_STR OPEN_STR;
    bool subsequent = false;
    for(const Code::Settings::Update update : updates){
        if(subsequent) out += ',';
        out += setting_names[update.setting_id];
        out += '=';
        out += warning_names[update.prev_value];
        subsequent = true;
    }
    out += CLOSE_STR;
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

void Settings::updateSizeFromChildSizes() noexcept {
    width = label_glyph_length * CHARACTER_WIDTHS[scriptDepth()] + 2*HSPACE;
    if(expanded) width = std::max(width, (chars_left+chars_right) * CHARACTER_WIDTHS[scriptDepth()] + 2*HSPACE + 2*INNER_COL_HSPACE);

    const double row_heights = expanded*(updates.size() * (VSPACE + CHARACTER_HEIGHTS[scriptDepth()]));
    above_center = ABOVE_CENTER[scriptDepth()] + row_heights/2;
    under_center = UNDER_CENTER[scriptDepth()] + row_heights/2;
}

void Settings::paintSpecific(Painter& painter) const {
    painter.setTypeIfAppropriate(SEM_KEYWORD);

    painter.drawText(x+HSPACE, y-1, "⃞ ⃞");

    if(isExpanded()){
        painter.drawText(x+HSPACE, y, "-  Settings");
        if(updates.empty()) return;

        double rolling_y = y;
        const double centre = x + (HSPACE+INNER_COL_HSPACE) + chars_left * CHARACTER_WIDTHS[scriptDepth()];
        const double row_height = VSPACE + CHARACTER_HEIGHTS[scriptDepth()];
        for(const auto& update : updates){
            std::string_view setting_id_str = setting_id_labels[update.setting_id];
            std::string_view warning_str = warning_labels[update.prev_value];

            rolling_y += row_height;
            painter.drawText(
                x + HSPACE,
                rolling_y,
                setting_id_str);
            painter.drawText(
                centre + INNER_COL_HSPACE,
                rolling_y,
                warning_str);

            painter.drawDashedLine(x, rolling_y - VSPACE/2, width, 0);
        }
        painter.drawDashedLine(centre, y + row_height - VSPACE/2, 0, height() - row_height + VSPACE/2);
        painter.drawDashedLine(x, y + row_height - VSPACE/2, 0, height() - row_height + VSPACE/2);
        painter.drawDashedLine(x + width, y + row_height - VSPACE/2, 0, height() - row_height + VSPACE/2);
        painter.drawDashedLine(x, y + height(), width, 0);
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

    SettingsDialog::updateInherited(settings->inherited);
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

std::string Settings::getTooltip(double x_local, double y_local) const alloc_except {
    if(!expanded) return str;

    if(y_local <= CHARACTER_HEIGHTS[scriptDepth()] + VSPACE/2 || y_local >= height()) return "";
    size_t row = (y_local - VSPACE/2) / (VSPACE + CHARACTER_HEIGHTS[scriptDepth()]) - 1;

    const auto& update = updates[row];
    std::string_view setting = setting_id_labels[update.setting_id];
    std::string_view warning = warning_labels[update.prev_value];

    const double midline = width - HSPACE - chars_right * CHARACTER_WIDTHS[scriptDepth()];

    if(x_local > HSPACE && x_local <= HSPACE + setting.size() * CHARACTER_WIDTHS[scriptDepth()]){
        return setting_descriptions[update.setting_id].data();
    }else if(x_local <= midline + warning.size() * CHARACTER_WIDTHS[scriptDepth()] && x_local > midline){
        return warning_descriptions[update.prev_value].data();
    }else{
        return "";
    }
}

bool Settings::isExpanded() const noexcept {
    return expanded;
}

void Settings::updateString() alloc_except {
    chars_left = 0;
    chars_right = 0;

    if(updates.empty()){
        str = "No changes";
        return;
    }

    str.clear();
    for(const auto& update : updates){
        std::string_view setting_id_str = setting_id_labels[update.setting_id];
        std::string_view warning_str = warning_labels[update.prev_value];

        str += setting_id_str;
        str += ':';
        str += ' ';
        str += warning_str;
        str += '\n';
        chars_left = std::max(chars_left, setting_id_str.size());
        chars_right = std::max(chars_right, warning_str.size());
    }
    str.pop_back();
}
#endif

}

}
