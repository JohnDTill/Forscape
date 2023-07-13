#include "typeset_command_change_settings.h"

#include <typeset_construct.h>
#include <typeset_controller.h>

namespace Forscape {

namespace Typeset {

CommandChangeSettings::CommandChangeSettings(
        Construct* settings, std::vector<Code::Settings::Update>& live_updates) alloc_except
    : settings(settings), live_updates(live_updates) {}

void CommandChangeSettings::undo(Controller& c) {
    swap(c);
}

void CommandChangeSettings::redo(Controller& c) {
    swap(c);
}

void CommandChangeSettings::swap(Controller& c) noexcept {
    std::swap(live_updates, stale_updates);
    c.setBothToFrontOf(settings->next());
}

}

}
