#ifndef FORSCAPE_DYNAMIC_SETTINGS_H
#define FORSCAPE_DYNAMIC_SETTINGS_H

#include <code_settings_constants.h>
#include <array>
#include <cassert>
#include <cinttypes>
#include <stddef.h>
#include <vector>

namespace Forscape {

namespace Code {

class Settings {
public:
    struct Update {
        SettingId setting_id;
        SettingValue prev_value;

        Update() noexcept = default;
        Update(SettingId setting_id, SettingValue prev_value) noexcept
            : setting_id(setting_id), prev_value(prev_value) {}

        bool operator==(const Update& other) const noexcept {
            return setting_id == other.setting_id && prev_value == other.prev_value;
        }
    };

private:
    std::array<SettingValue, NUM_CODE_SETTINGS> flags = DEFAULT_CODE_SETTINGS;
    std::vector<Update> updates;
    std::vector<size_t> scope_start;

public:
    template<SettingId setting> WarningLevel warningLevel() const noexcept {
        static_assert(setting < NUM_CODE_SETTINGS);
        return static_cast<WarningLevel>(flags[setting]);
    }

    template<SettingId setting> void setWarningLevel(WarningLevel warning_level) alloc_except {
        static_assert(setting < NUM_CODE_SETTINGS);
        assert(warning_level < NUM_WARNING_LEVELS);
        updates.push_back( Update(setting, flags[setting]) );
        flags[setting] = warning_level;
    }

    void reset() noexcept;
    void set(SettingId setting, SettingValue value) alloc_except;
    void updateInherited(size_t flag) const noexcept;
    void enact(size_t flag) alloc_except;
    void enterScope() alloc_except;
    void leaveScope() noexcept;
};

}

}

#endif // FORSCAPE_DYNAMIC_SETTINGS_H
