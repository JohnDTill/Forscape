#ifndef FORSCAPE_DYNAMIC_SETTINGS_H
#define FORSCAPE_DYNAMIC_SETTINGS_H

#include <array>
#include <cinttypes>
#include <vector>

namespace Forscape {

namespace Code {

//DO THIS: a lot of this should be codegen

enum SettingId {
    UNUSED_VARIABLE,
    WARN_TRANSPOSE_T, //Transpose with letter 'T' instead of symbol '⊤'  //DO THIS: implement conditional warning

    NUM_SETTINGS,
};

enum WarningLevel {
    NO_WARNING,
    WARN,
    ERROR,

    NUM_WARNING_LEVELS,
};

class Settings {
public:
    typedef uint8_t SettingValue;

    struct Update {
        SettingId setting_id;
        SettingValue prev_value;

        Update() noexcept = default;
        Update(SettingId setting_id, SettingValue prev_value) noexcept
            : setting_id(setting_id), prev_value(prev_value) {}
    };

private:
    static constexpr std::array<SettingValue, NUM_SETTINGS> DEFAULT_FLAGS = {
        WARN,
        WARN
    };

    std::array<SettingValue, NUM_SETTINGS> flags = DEFAULT_FLAGS;
    std::vector<Update> updates;
    std::vector<size_t> scope_start;

public:
    template<SettingId setting> WarningLevel warningLevel() const noexcept {
        static_assert(setting < NUM_SETTINGS);
        return static_cast<WarningLevel>(flags[setting]);
    }

    template<SettingId setting> void setWarningLevel(WarningLevel warning_level) alloc_except {
        static_assert(setting < NUM_SETTINGS);
        assert(warning_level < NUM_WARNING_LEVELS);
        updates.push_back( Update(setting, flags[setting]) );
        flags[setting] = warning_level;
    }

    void reset() noexcept;
    void set(SettingId setting, SettingValue value) alloc_except;
    void enact(size_t flag) alloc_except;
    void enterScope() alloc_except;
    void leaveScope() noexcept;
};

}

}

#endif // FORSCAPE_DYNAMIC_SETTINGS_H
