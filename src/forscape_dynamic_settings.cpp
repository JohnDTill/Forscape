#include "forscape_dynamic_settings.h"

#include <typeset_settings.h>

namespace Forscape {

namespace Code {

void Settings::reset() noexcept {
    flags = DEFAULT_FLAGS;
}

void Settings::set(SettingId setting, SettingValue value) alloc_except {
    assert(setting < NUM_SETTINGS);
    assert(value < NUM_WARNING_LEVELS);
    updates.push_back( Update(setting, flags[setting]) );
    flags[setting] = value;
}

void Settings::enact(size_t flag) noexcept {
    auto c = reinterpret_cast<Typeset::Settings*>(flag);
    for(const auto& update : c->updates)
        set(update.setting_id, update.prev_value);
}

void Settings::enterScope() alloc_except {
    scope_start.push_back(updates.size());
}

void Settings::leaveScope() noexcept {
    if(scope_start.empty()) return; //DO THIS: handle this better

    const size_t start = scope_start.back();
    scope_start.pop_back();
    for(size_t i = updates.size(); i-->start;){
        const Update& update = updates[i];
        flags[update.setting_id] = update.prev_value;
    }
    updates.resize(start);
}

}

}
