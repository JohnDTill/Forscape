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
    size_t start;
    size_t len;
    const std::string* const error_out = nullptr;

    Error() noexcept = default;
    Error(const Typeset::Selection& selection, ErrorCode code, size_t start, size_t len, const std::string* const error_out) noexcept;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    void writeTo(Typeset::Text* t, Typeset::View* caller) const;
    static void writeErrors(const std::vector<Error>& errors, Typeset::Model* m, Typeset::View* caller);
    static Typeset::Model* writeErrors(const std::vector<Error>& errors, Typeset::View* caller);
    #endif
    std::string_view message() const noexcept;
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
    void fail(const Typeset::Selection& selection, ErrorCode code) alloc_except;
    void fail(const Typeset::Selection& selection, const std::string& str, ErrorCode code = ErrorCode::VALUE_NOT_DETERMINED) alloc_except;
    void warn(WarningLevel warning_level, const Typeset::Selection& selection, ErrorCode code) alloc_except;
    void warn(WarningLevel warning_level, const Typeset::Selection& selection, const std::string& str, ErrorCode code = ErrorCode::VALUE_NOT_DETERMINED) alloc_except;

private:
    void writeLocation(WarningLevel level, const Typeset::Selection& selection, const Typeset::Model* const model) alloc_except;
};

}

}

#endif // FORSCAPE_ERROR_H
