#ifndef TYPESET_LINE_H
#define TYPESET_LINE_H

#include "typeset_phrase.h"

namespace Hope {

namespace Typeset {

class Model;

class Line final : public Phrase {
public:
    Line();
    Line(Model* model);
    virtual bool isLine() const noexcept override;
    Model* parent;
    Line* next() const noexcept;
    Line* prev() const noexcept;
    Line* nextAsserted() const noexcept;
    size_t leadingSpaces() const noexcept;
    const std::vector<Line*>& lines() const noexcept;
    Line* nearestLine(double y) const noexcept;

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void resize() noexcept override;
    #endif

    size_t scope_depth = 0;
};

}

}

#endif // TYPESET_LINE_H
