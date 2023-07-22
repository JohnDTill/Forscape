#ifndef TYPESET_COMMAND_CHANGE_SETTINGS_H
#define TYPESET_COMMAND_CHANGE_SETTINGS_H

#include <typeset_command.h>
#include <forscape_dynamic_settings.h>

namespace Forscape {

namespace Typeset {

class Construct;

class CommandChangeSettings : public Command {
public:
    CommandChangeSettings(Construct* settings, std::vector<Code::Settings::Update>& live_updates) alloc_except;
    std::vector<Code::Settings::Update> stale_updates;

protected:
    virtual void undo(Controller& c) override final;
    virtual void redo(Controller& c) override final;

private:
    void swap(Controller& c) noexcept;

    Construct* settings;
    std::vector<Code::Settings::Update>& live_updates;
};

}

}

#endif // TYPESET_COMMAND_CHANGE_SETTINGS_H
