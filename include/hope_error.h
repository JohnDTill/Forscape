#ifndef HOPE_ERROR_H
#define HOPE_ERROR_H

#include <typeset_selection.h>
#include <code_error_types.h>
#include <vector>

namespace Hope {

namespace Typeset{
    class View;
}

namespace Code {

struct Error {
    const Typeset::Selection selection;
    const ErrorCode code;

    Error(Typeset::Selection controller, ErrorCode code)
        : selection(controller), code(code) {}

    #ifndef HOPE_TYPESET_HEADLESS
    void writeTo(Typeset::Text* t, Typeset::View* caller) const;
    static void writeErrors(const std::vector<Error>& errors, Typeset::Model* m, Typeset::View* caller);
    static Typeset::Model* writeErrors(const std::vector<Error>& errors, Typeset::View* caller);
    #endif
    std::string message() const;
    std::string line() const;
};

}

}

#endif // HOPE_ERROR_H
