#ifndef FORSCAPE_ERROR_H
#define FORSCAPE_ERROR_H

#include <typeset_selection.h>
#include <code_error_types.h>
#include <vector>

namespace Forscape {

namespace Typeset{
    class View;
}

namespace Code {

struct Error {
    Typeset::Selection selection;
    ErrorCode code;

    Error() noexcept = default;
    Error(Typeset::Selection controller, ErrorCode code) noexcept;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void writeTo(Typeset::Text* t, Typeset::View* caller) const;
    static void writeErrors(const std::vector<Error>& errors, Typeset::Model* m, Typeset::View* caller);
    static Typeset::Model* writeErrors(const std::vector<Error>& errors, Typeset::View* caller);
    #endif
    std::string message() const;
    std::string line() const;
};

}

}

#endif // FORSCAPE_ERROR_H
