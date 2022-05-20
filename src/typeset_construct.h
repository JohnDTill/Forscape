#ifndef TYPESET_CONSTRUCT_H
#define TYPESET_CONSTRUCT_H

#include <construct_codes.h>
#include <cassert>
#include <string>
#include <vector>

#ifndef HOPE_TYPESET_HEADLESS
#include "typeset_painter.h"
#endif

#ifdef TYPESET_MEMORY_DEBUG
#include <hope_common.h>
#endif

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Typeset {

class Controller;
class Phrase;
class Selection;
class Subphrase;
class Text;

class Construct {
public:
    #ifdef TYPESET_MEMORY_DEBUG
    static HOPE_UNORDERED_SET<Construct*> all;
    Construct();
    #endif

    virtual ~Construct();
    void setParent(Phrase* p) noexcept;
    Subphrase* front() const noexcept;
    Subphrase* back() const noexcept;
    Text* frontText() const noexcept;
    Text* frontTextAsserted() const noexcept;
    Text* next() const noexcept;
    Text* prev() const noexcept;
    Text* textEnteringFromLeft() const noexcept;
    Text* textEnteringFromRight() const noexcept;
    Text* textRightOfSubphrase(const Subphrase* caller) const noexcept;
    Text* textLeftOfSubphrase(const Subphrase* caller) const noexcept;
    virtual Text* textUp(const Subphrase* caller, double x) const noexcept;
    virtual Text* textDown(const Subphrase* caller, double x) const noexcept;
    virtual size_t serialChars() const noexcept;
    virtual size_t dims() const noexcept;
    virtual void writeString(std::string& out, size_t& curr) const noexcept;
    virtual void writeArgs(std::string& out, size_t& curr) const noexcept;
    virtual char constructCode() const noexcept = 0;
    std::string toString() const;
    size_t numArgs() const noexcept;
    bool sameContent(const Construct* other) const noexcept;
    void findCaseInsensitive(const std::string& target, std::vector<Selection>& hits) const;

    Phrase* parent;
    size_t id;

    #ifdef HOPE_SEMANTIC_DEBUGGING
    std::string toStringWithSemanticTags() const;
    #endif

    #ifndef HOPE_TYPESET_HEADLESS
    bool contains(double x, double y) const noexcept;
    Construct* constructAt(double x, double y) noexcept;
    Subphrase* argAt(double x, double y) const noexcept;
    uint8_t scriptDepth() const noexcept;
    virtual bool increasesScriptDepth(uint8_t id) const noexcept;
    void updateSize() noexcept;
    void updateLayout() noexcept;
    void resize() noexcept;
    void paint(Painter& painter) const;
    double height() const noexcept;
    double width;
    double above_center;
    double under_center;
    double x;
    double y;

    struct ContextAction {
        void(*takeAction)(Construct* con, Controller& c, Subphrase* child);
        bool enabled = true;
        const std::string name;

        ContextAction(const std::string& name,
                      void(*takeAction)(Construct* con, Controller& c, Subphrase* child));
    };

    static const std::vector<ContextAction> no_actions;
    virtual const std::vector<ContextAction>& getContextActions(Subphrase*) const noexcept;
    #endif

    Subphrase* arg(size_t index) const noexcept;

protected:
    void setupUnaryArg();
    void setupBinaryArgs();
    void setupNAargs(uint16_t n);
    Subphrase* child() const noexcept;
    Subphrase* first() const noexcept;
    Subphrase* second() const noexcept;

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeSpecific() noexcept = 0;
    virtual void updateChildPositions();
    virtual void paintSpecific(Painter&) const;
    virtual void invalidateX() noexcept;
    virtual void invalidateY() noexcept;
    virtual void invalidatePos() noexcept;
    #endif

    std::vector<Subphrase*> args;

    bool hasSmallChild() const noexcept;

private:
    size_t getSubphraseIndex(const Subphrase* s) const noexcept;
};

}

}

#endif // TYPESET_CONSTRUCT_H
