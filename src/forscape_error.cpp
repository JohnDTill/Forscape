#include "forscape_error.h"

#include <typeset_line.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>
#include <typeset_text.h>

namespace Forscape {

namespace Code {

#ifndef FORSCAPE_TYPESET_HEADLESS
void Error::writeTo(Typeset::Text* t, Typeset::View* caller) const {
    Typeset::Line* l = selection.getStartLine();
    if(caller){
        t->getParent()->appendConstruct(new Typeset::MarkerLink(l, caller, selection.getModel()));
        t = t->nextTextAsserted();
        t->setString(" - ");
        t->getModel()->appendSerialToOutput(std::string(message()));
    }else{
        t->setString("Line " + line() + " - ");
        t->getModel()->appendSerialToOutput(std::string(message()));
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

    m->appendSerialToOutput(*errors.front().error_out);
}

Typeset::Model* Error::writeErrors(const std::vector<Error>& errors, Typeset::View* caller){
    Typeset::Model* m = new Typeset::Model;
    m->is_output = true;
    writeErrors(errors, m, caller);

    return m;
}
#endif

Error::Error(const Typeset::Selection& selection, ErrorCode code, size_t start, size_t len, const std::string* const error_out) noexcept
    : selection(selection), code(code), start(start), len(len), error_out(error_out) {}

std::string_view Error::message() const noexcept {
    assert(error_out != nullptr);
    return std::string_view(error_out->data()+start, len);
}

std::string Error::line() const {
    return std::to_string(selection.getStartLine()->id+1);
}

void ErrorStream::reset() noexcept {
    error_out.clear();
    errors.clear();
    warnings.clear();
}

bool ErrorStream::noErrors() const noexcept {
    return errors.empty();
}

void ErrorStream::fail(const Typeset::Selection& selection, ErrorCode code) alloc_except {
    Typeset::Model* model = selection.getModel();
    writeLocation(ERROR, selection, model);

    const size_t start = error_out.size();
    error_out += getMessage(code);
    errors.push_back(Error(selection, code, start, error_out.size()-start, &error_out));
    model->errors.push_back(errors.back());

    //DO THIS: quotes must show up
    if(shouldQuote(code)){
        error_out += ": ";
        error_out += selection.str();
    }

    error_out += '\n';
}

void ErrorStream::fail(const Typeset::Selection& selection, const std::string& str, ErrorCode code) alloc_except {
    Typeset::Model* model = selection.getModel();
    writeLocation(ERROR, selection, model);

    const size_t start = error_out.size();
    error_out += str;
    error_out += '\n';
    errors.push_back(Error(selection, code, start, str.length(), &error_out));
    model->errors.push_back(errors.back());
}

void ErrorStream::warn(WarningLevel warning_level, const Typeset::Selection& selection, ErrorCode code) alloc_except {
    if(warning_level == NO_WARNING) return;

    Typeset::Model* model = selection.getModel();
    writeLocation(warning_level, selection, model);

    const size_t start = error_out.size();
    error_out += getMessage(code);

    const Error error(selection, code, start, error_out.size()-start, &error_out);

    switch (warning_level) {
        case ERROR: errors.push_back(error); model->errors.push_back(error); break;
        case WARN: warnings.push_back(error); model->warnings.push_back(error); break;
        default: break;
    }

    if(shouldQuote(code)){
        error_out += ": ";
        error_out += selection.str();
    }

    error_out += '\n';
}

void ErrorStream::warn(WarningLevel warning_level, const Typeset::Selection& selection, const std::string& str, ErrorCode code) alloc_except {
    if(warning_level == NO_WARNING) return;

    Typeset::Model* model = selection.getModel();
    writeLocation(warning_level, selection, model);

    const size_t start = error_out.size();
    error_out += str;
    error_out += '\n';

    const Error error(selection, code, start, str.size(), &error_out);
    switch (warning_level) {
        case ERROR: errors.push_back(error); model->errors.push_back(error); break;
        case WARN: warnings.push_back(error); model->warnings.push_back(error); break;
        default: break;
    }
}

void ErrorStream::writeLocation(WarningLevel level, const Typeset::Selection& selection, const Typeset::Model* const model) alloc_except {
    assert(model == selection.getModel());
    error_out += warning_labels[level];
    error_out += '\n';
    error_out += model->path.u8string();
    error_out += ": Line ";
    error_out += selection.getStartLineAsString();
    error_out += '\n';
    //DO THIS: no link here, markerlink design is bad
}

}

}
