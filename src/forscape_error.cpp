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
        t->getModel()->appendSerialToOutput(message());
    }else{
        t->setString("Line " + line() + " - ");
        t->getModel()->appendSerialToOutput(message());
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

Error::Error(Typeset::Selection selection, ErrorCode code) noexcept
    : selection(selection), code(code), str(getMessage(code)) {
    if(shouldQuote(code)) str += selection.str();
}

Error::Error(Typeset::Selection selection, ErrorCode code, const std::string& str)
    : selection(selection), code(code), str(str) {}

std::string Error::message() const {
    return str;
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

void ErrorStream::fail(const Typeset::Selection& selection, const std::string& str, ErrorCode code) noexcept {
    error_out += str;
    errors.push_back(Error(selection, code, str));

    Typeset::Model* model = selection.getModel();
    model->errors.push_back(errors.back());
}

void ErrorStream::warn(WarningLevel warning_level, const Typeset::Selection& selection, const std::string& str, ErrorCode code) noexcept {
    error_out += str;
    Typeset::Model* model = selection.getModel();
    const Error error(selection, code, str);
    switch (warning_level) {
        case ERROR: errors.push_back(error); model->errors.push_back(error); break;
        case WARN: warnings.push_back(error); model->warnings.push_back(error); break;
        default: break;
    }
}

}

}
