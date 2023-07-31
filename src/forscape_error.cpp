#include "forscape_error.h"

#include "forscape_program.h"
#include "typeset_line.h"
#include <typeset_markerlink.h>
#include "typeset_model.h"
#include "typeset_text.h"

namespace Forscape {

namespace Code {

#ifndef FORSCAPE_TYPESET_HEADLESS
void Error::writeTo(Typeset::Text* t, Typeset::View* caller) const {
    Typeset::Line* l = selection.getStartLine();
    Typeset::Model* model = t->getModel();
    t->tags.push_back( SemanticTag(0, SEM_ERROR) );
    t->setString("ERROR");
    model->appendLine();
    t = model->lastText();
    t->tags.push_back( SemanticTag(0, SEM_ERROR) );
    model->appendSerialToOutput(selection.getModel()->path.u8string());
    model->appendLine();
    t = model->lastText();

    if(caller){
        t->getParent()->appendConstruct(new Typeset::MarkerLink(l, caller, selection.getModel()));
        t = t->nextTextAsserted();
        t->setString(" - ");
        model->appendSerialToOutput(std::string(consoleMessage()));
    }else{
        t->setString("Line " + line() + " - ");
        model->appendSerialToOutput(std::string(consoleMessage()));
    }
    t->tags.push_back( SemanticTag(0, SEM_ERROR) );
}

void Error::writeErrors(const std::vector<Error>& errors, Typeset::Model* m, Typeset::View* caller){
    if(!m->empty()){
        m->appendLine();
        Typeset::Text* t = m->lastText();
        t->setString("_______________________________");
        t->tags.push_back( SemanticTag(0, SEM_ERROR) );
        m->appendLine();
    }

    for(const Error& err : errors){
        err.writeTo(m->lastText(), caller);
        m->appendLine();
    }
}

Typeset::Model* Error::writeErrors(const std::vector<Error>& errors, Typeset::View* caller){
    Typeset::Model* m = new Typeset::Model;
    m->is_output = true;
    writeErrors(errors, m, caller);

    return m;
}
#endif

Error::Error(const Typeset::Selection& selection, ErrorCode code, size_t start, size_t len, const std::string* const error_out) noexcept
    : selection(selection), code(code), tooltip_start(start), tooltip_len(len), buffer(error_out) {}

std::string_view Error::tooltipMessage() const noexcept {
    assert(buffer != nullptr);
    return std::string_view(buffer->data()+tooltip_start, tooltip_len);
}

std::string_view Error::consoleMessage() const noexcept {
    assert(buffer != nullptr);
    return std::string_view(buffer->data()+console_start, console_len);
}

std::string Error::line() const {
    return std::to_string(selection.getStartLine()->id+1);
}

void ErrorStream::reset() noexcept {
    error_warning_buffer.clear();
    errors.clear();
    warnings.clear();
}

bool ErrorStream::noErrors() const noexcept {
    return errors.empty();
}

void ErrorStream::fail(const Typeset::Selection& selection, ErrorCode code) alloc_except {
    Typeset::Model* model = selection.getModel();

    const size_t start = active_buffer->size();
    *active_buffer += getMessage(code);

    Error error(selection, code, start, active_buffer->size()-start, active_buffer);

    if(shouldQuote(code)){
        *active_buffer += ": ";
        *active_buffer += selection.str();
    }

    error.console_start = start;
    error.console_len = active_buffer->size()-start;

    errors.push_back(error);
    model->errors.push_back(error);
}

void ErrorStream::fail(const Typeset::Selection& selection, const std::string& str, ErrorCode code) alloc_except {
    Typeset::Model* model = selection.getModel();

    const size_t start = active_buffer->size();
    *active_buffer += str;
    *active_buffer += '\n';
    Error error(selection, code, start, str.length(), active_buffer);
    error.console_start = start;
    error.console_len = str.length();
    errors.push_back(error);
    model->errors.push_back(error);
}

void Forscape::Code::ErrorStream::warn(SettingId setting, const Typeset::Selection& selection, ErrorCode code) noexcept {
    warn(Program::instance()->settings.warningLevel(setting), selection, code);
}

void ErrorStream::warn(WarningLevel warning_level, const Typeset::Selection& selection, ErrorCode code) alloc_except {
    assert(warning_level < NUM_WARNING_LEVELS);
    if(warning_level == NO_WARNING) return;

    Typeset::Model* model = selection.getModel();

    const size_t start = active_buffer->size();
    *active_buffer += getMessage(code);

    Error error(selection, code, start, active_buffer->size()-start, active_buffer);
    error.console_start = start;
    error.console_len = error.tooltip_len;

    switch (warning_level) {
        case ERROR: errors.push_back(error); model->errors.push_back(error); break;
        case WARN: warnings.push_back(error); model->warnings.push_back(error); break;
        default: break;
    }

    if(shouldQuote(code)){
        *active_buffer += ": ";
        *active_buffer += selection.str();
    }
}

void ErrorStream::warn(SettingId setting, const Typeset::Selection& selection, const std::string& str, ErrorCode code) noexcept {
    warn(Program::instance()->settings.warningLevel(setting), selection, str, code);
}

void ErrorStream::warn(WarningLevel warning_level, const Typeset::Selection& selection, const std::string& str, ErrorCode code) alloc_except {
    if(warning_level == NO_WARNING) return;

    Typeset::Model* model = selection.getModel();

    const size_t start = active_buffer->size();
    *active_buffer += str;

    Error error(selection, code, start, str.size(), active_buffer);
    error.console_start = start;
    error.console_len = str.size();

    switch (warning_level) {
        case ERROR: errors.push_back(error); model->errors.push_back(error); break;
        case WARN: warnings.push_back(error); model->warnings.push_back(error); break;
        default: break;
    }
}

void ErrorStream::setBuffer(std::string* buffer) noexcept {
    active_buffer = buffer;
}

void Forscape::Code::ErrorStream::setProgramBuffer() noexcept {
    active_buffer = &error_warning_buffer;
}

const std::vector<Error>& ErrorStream::getErrors() const noexcept {
    return errors;
}

}

}
