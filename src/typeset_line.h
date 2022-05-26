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
    Model* parent  DEBUG_INIT_NULLPTR;
    Line* next() const noexcept;
    Line* prev() const noexcept;
    Line* nextAsserted() const noexcept;
    size_t leadingSpaces() const noexcept;
    const std::vector<Line*>& lines() const noexcept;

    #ifndef HOPE_TYPESET_HEADLESS
    Line* nearestLine(double y) const noexcept;
    Line* nearestAbove(double y) const noexcept;
    virtual void resize() noexcept override;
    #ifndef NDEBUG
    virtual void invalidateWidth() noexcept override;
    virtual void invalidateDims() noexcept override;
    #endif
    #endif

    size_t scope_depth = 0;
};

}

}

#endif // TYPESET_LINE_H
