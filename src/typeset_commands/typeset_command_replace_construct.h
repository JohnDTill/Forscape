#ifndef TYPESET_COMMAND_REPLACE_CONSTRUCT_H
#define TYPESET_COMMAND_REPLACE_CONSTRUCT_H

#include "typeset_command.h"
#include "typeset_controller.h"
#include "typeset_model.h"
#include "typeset_subphrase.h"

namespace Forscape {

namespace Typeset {

template<typename Before, typename After>
class ReplaceConstruct : public Command {
private:
    bool enacted;
    Before* before;
    After* after;

public:
    ReplaceConstruct(Before* before) : before(before) {
        after = new After();
        after->parent = before->parent;
        after->id = before->id;
    }

    ~ReplaceConstruct() noexcept { delete (enacted ? static_cast<Construct*>(before) : after); }

protected:
    virtual void redo(Controller& c) override final{
        enacted = true;
        after->parent->constructs[after->id] = after;
        c.setBothToFrontOf(after->next());
    }

    virtual void undo(Controller& c) override final{
        enacted = false;
        before->parent->constructs[before->id] = before;
        c.setBothToFrontOf(before->next());
    }
};

template<typename Con1, typename Con2, bool to2>
class ReplaceConstruct1vs2 : public Command {
private:
    bool own2;
    Con1* con1;
    Con2* con2;

    Subphrase* moved;
    Subphrase* fake;

public:
    ReplaceConstruct1vs2(Con1* con1) : con1(con1) {
        assert(to2);

        con2 = new Con2();
        con2->parent = con1->parent;
        con2->id = con1->id;

        moved = con1->arg(0);
        fake = con2->arg(1);

        assert(con1->numArgs() == 1);
        assert(con2->numArgs() == 2);
    }

    ReplaceConstruct1vs2(Con2* con2) : con2(con2) {
        assert(!to2);

        con1 = new Con1();
        con1->parent = con2->parent;
        con1->id = con2->id;

        moved = con2->arg(1);
        fake = con1->arg(0);

        assert(con1->numArgs() == 1);
        assert(con2->numArgs() == 2);
    }

    ~ReplaceConstruct1vs2() noexcept { delete (own2 ? static_cast<Construct*>(con1) : con2); }

protected:
    void setFirst(Controller& c) noexcept {
        own2 = false;
        con1->parent->constructs[con1->id] = con1;
        con1->setArg(0, moved);
        con2->setArg(1, fake);
        moved->id = 0;
        fake->id = 1;
        moved->setParent(con1);
        fake->setParent(con2);
        c.setBothToFrontOf(con1->next());
    }

    void setSecond(Controller& c) noexcept {
        own2 = true;
        con2->parent->constructs[con2->id] = con2;
        con2->setArg(1, moved);
        con1->setArg(0, fake);
        moved->id = 1;
        fake->id = 0;
        moved->setParent(con2);
        fake->setParent(con1);
        c.setBothToFrontOf(con2->next());
    }

    virtual void redo(Controller& c) override final{
        if(to2) setSecond(c);
        else setFirst(c);
    }

    virtual void undo(Controller& c) override final{
        if(to2) setFirst(c);
        else setSecond(c);
    }
};

}

}

#endif // TYPESET_COMMAND_REPLACE_CONSTRUCT_H
