#ifndef FORSCAPE_ERROR_H
#define FORSCAPE_ERROR_H

#include <typeset_selection.h>
#include <code_error_types.h>
#include <code_settings_constants.h>
#include <vector>

namespace Forscape {

namespace Typeset{
    class View;
}

namespace Code {

struct Error {
    Typeset::Selection selection;
    ErrorCode code;
    size_t flag;
    std::string str; //DO THIS: eliminate nested allocation

    Error() noexcept = default;
    Error(Typeset::Selection selection, ErrorCode code) noexcept;
    Error(Typeset::Selection selection, ErrorCode code, const std::string& str);

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void writeTo(Typeset::Text* t, Typeset::View* caller) const;
    static void writeErrors(const std::vector<Error>& errors, Typeset::Model* m, Typeset::View* caller);
    static Typeset::Model* writeErrors(const std::vector<Error>& errors, Typeset::View* caller);
    #endif
    std::string message() const;
    std::string line() const;
};

class ErrorStream {
private:
    std::string error_out;
    std::vector<Error> errors;
    std::vector<Error> warnings;

public:
    void reset() noexcept;
    bool noErrors() const noexcept;
    void fail(const Typeset::Selection& selection, const std::string& str, ErrorCode code = ErrorCode::VALUE_NOT_DETERMINED) alloc_except;
    void warn(WarningLevel warning_level, const Typeset::Selection& selection, const std::string& str, ErrorCode code = ErrorCode::VALUE_NOT_DETERMINED) alloc_except;
};

}

}

#endif // FORSCAPE_ERROR_H
