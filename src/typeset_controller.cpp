#include "typeset_controller.h"

#include "hope_unicode.h"
#include "typeset_closesymbol.h"
#include "typeset_construct.h"
#include "typeset_keywords.h"
#include "typeset_line.h"
#include "typeset_model.h"
#include "typeset_shorthand.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"
#include "typeset_themes.h"
#include <typeset_command_indent.h>
#include <typeset_command_line.h>
#include <typeset_command_pair.h>
#include <typeset_command_phrase.h>
#include <typeset_command_text.h>
#include <typeset_insert_chars.h>
#include <typeset_remove_chars.h>
#include <typeset_markerlink.h>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Typeset {

Controller::Controller(){}

Controller::Controller(Model* model)
    : active(model), anchor(active) {}

Controller::Controller(const Selection& sel) noexcept
    : active(sel.right), anchor(sel.left) {}

Controller& Controller::operator=(const Marker& marker) noexcept{
    active = anchor = marker;
    return *this;
}

Controller::Controller(const Marker& anchor, const Marker& active)
    : active(active), anchor(anchor) {}

const Marker& Controller::getAnchor() const noexcept{
    return anchor;
}

const Marker& Controller::getActive() const noexcept{
    return active;
}

bool Controller::hasSelection() const noexcept{
    return active != anchor;
}

void Controller::deselect() noexcept{
    consolidateToActive();
}

void Controller::moveToNextChar() noexcept {
    if(hasSelection()){
        consolidateRight();
    }else if(notAtTextEnd()){
        incrementActiveGrapheme();
        consolidateToActive();
    }else if(Construct* c = active.text->nextConstructInPhrase()){
        setBothToFrontOf(c->textEnteringFromLeft());
    }else if(isNested()){
        setBothToFrontOf(subphrase()->textRightOfSubphrase());
    }else if(Line* l = nextLine()){
        setBothToFrontOf(l->front());
    }
}

void Controller::moveToPrevChar() noexcept{
    if(hasSelection()){
        consolidateLeft();
    }else if(notAtTextStart()){
        decrementActiveGrapheme();
        consolidateToActive();
    }else if(Construct* c = active.text->prevConstructInPhrase()){
        setBothToBackOf(c->textEnteringFromRight());
    }else if(isNested()){
        setBothToBackOf(subphrase()->textLeftOfSubphrase());
    }else if(Line* l = prevLine()){
        setBothToBackOf(l->back());
    }
}

void Controller::moveToNextWord() noexcept{
    if(hasSelection()){
        consolidateRight();
    }else if(notAtTextEnd()){
        incrementToNextWord();
        consolidateToActive();
    }else{
        selectNextChar();
        consolidateToActive();
    }
}

void Controller::moveToPrevWord() noexcept{
    if(hasSelection()){
        consolidateLeft();
    }else if(notAtTextStart()){
        decrementToPrevWord();
        consolidateToActive();
    }else{
        selectPrevChar();
        consolidateToActive();
    }
}

void Controller::moveToStartOfLine() noexcept{
    setBothToFrontOf(phrase()->front());
}

void Controller::moveToEndOfLine() noexcept{
    setBothToBackOf(phrase()->back());
}

void Controller::moveToStartOfDocument() noexcept{
    setBothToFrontOf(getModel()->firstText());
}

void Controller::moveToEndOfDocument() noexcept{
    setBothToBackOf(getModel()->lastText());
}

void Controller::selectNextChar() noexcept {
    if(notAtTextEnd()){
        incrementActiveGrapheme();
    }else if(Construct* c = active.text->nextConstructInPhrase()){
        active.setToFrontOf(c->next());
    }else if(isTopLevel()){
        if(Line* l = nextLine()) active.setToFrontOf(l->front());
    }
}

void Controller::selectPrevChar() noexcept{
    if(notAtTextStart()){
        decrementActiveGrapheme();
    }else if(Construct* c = active.text->prevConstructInPhrase()){
        active.setToBackOf(c->prev());
    }else if(isTopLevel()){
        if(Line* l = prevLine()) active.setToBackOf(l->back());
    }
}

void Controller::selectNextWord() noexcept{
    if(notAtTextEnd()){
        incrementToNextWord();
    }else if(Construct* c = active.text->nextConstructInPhrase()){
        active.setToFrontOf(c->next());
    }else if(isTopLevel()){
        if(Line* l = nextLine()) active.setToFrontOf(l->front());
    }
}

void Controller::selectPrevWord() noexcept{
    if(notAtTextStart()){
        decrementToPrevWord();
    }else if(Construct* c = active.text->prevConstructInPhrase()){
        active.setToBackOf(c->prev());
    }else if(active.text->isTopLevel()){
        if(Line* l = prevLine()) active.setToBackOf(l->back());
    }
}

void Controller::selectStartOfLine() noexcept{
    active.setToFrontOf(phrase()->front());
}

void Controller::selectEndOfLine() noexcept{
    active.setToBackOf(phrase()->back());
}

void Controller::selectStartOfDocument() noexcept{
    if(isTopLevel()) active.setToFrontOf(getModel()->firstText());
    else active.setToFrontOf(phrase()->front());
}

void Controller::selectEndOfDocument() noexcept{
    if(isTopLevel()) active.setToBackOf(getModel()->lastText());
    else active.setToBackOf(phrase()->back());
}

void Controller::selectAll() noexcept{
    Model* m = getModel();
    anchor.setToFrontOf(m->firstText());
    active.setToBackOf(m->lastText());
}

std::string Controller::selectedText() const{
    return selection().str();
}

std::vector<Selection> Controller::findCaseInsensitive(const std::string& str) const{
    return selection().findCaseInsensitive(str);
}

#ifndef HOPE_TYPESET_HEADLESS
void Controller::moveToNextLine(double setpoint) noexcept{
    if(isNested()) active.setToLeftOf(subphrase()->textDown(setpoint), setpoint);
    else selectNextLine(setpoint);
    consolidateToActive();
}

void Controller::moveToPrevLine(double setpoint) noexcept{
    if(isNested()) active.setToLeftOf(subphrase()->textUp(setpoint), setpoint);
    else selectPrevLine(setpoint);
    consolidateToActive();
}

void Controller::moveToNextPage(double setpoint, double page_height) noexcept{
    selectNextPage(setpoint, page_height);
    consolidateToActive();
}

void Controller::moveToPrevPage(double setpoint, double page_height) noexcept{
    selectPrevPage(setpoint, page_height);
    consolidateToActive();
}

void Controller::selectNextLine(double setpoint) noexcept{
    if(isTopLevel())
        if(Line* l = nextLine())
            active.setToLeftOf(l->textLeftOf(setpoint), setpoint);
}

void Controller::selectPrevLine(double setpoint) noexcept{
    if(isTopLevel())
        if(Line* l = prevLine())
            active.setToLeftOf(l->textLeftOf(setpoint), setpoint);
}

void Controller::selectNextPage(double setpoint, double page_height) noexcept{
    if(isNested()) return;

    Line* l = activeLine();
    double target = l->y + l->above_center + page_height;
    l = l->parent->nearestLine(target);
    active.setToLeftOf(l->textLeftOf(setpoint), setpoint);
}

void Controller::selectPrevPage(double setpoint, double page_height) noexcept{
    if(isNested()) return;

    Line* l = activeLine();
    double target = l->y + l->above_center - page_height;
    l = l->parent->nearestLine(target);
    active.setToLeftOf(l->textLeftOf(setpoint), setpoint);
}

bool Controller::contains(double x, double y) const{
    return selection().contains(x, y);
}

void Controller::paintSelection(Painter& painter) const{
    selection().paintSelection(painter, isForward());
}

void Controller::paintCursor(Painter& painter) const {
    constexpr double EXTRA_UP = 1;
    constexpr double EXTRA_DOWN = 0;

    double x = active.x();
    double y = active.text->y - EXTRA_UP;
    double h = active.text->height() + EXTRA_UP + EXTRA_DOWN;
    painter.drawNarrowCursor(x, y, h);
}

void Controller::paintInsertCursor(Painter& painter) const{
    Controller controller(*this);
    controller.consolidateToActive();
    controller.selectNextChar();

    QColor selection_bak = getColour(COLOUR_SELECTION);
    QColor text_cursor_color = getColour(COLOUR_CURSOR);
    setColour(COLOUR_SELECTION, text_cursor_color);

    if(controller.hasSelection()){
        controller.paintSelection(painter);
    }else{
        double x = active.text->xRight();
        double y = active.text->y;
        double h = active.text->height();
        double w = 5;
        painter.drawSelection(x, y, w, h);
    }

    setColour(COLOUR_SELECTION, selection_bak);
}
#endif

Model* Controller::getModel() const noexcept{
    return active.text->getModel();
}

bool Controller::atStart() const noexcept{
    return atTextStart() && active.text == getModel()->firstText();
}

bool Controller::atEnd() const noexcept{
    return atTextEnd() && active.text == getModel()->lastText();
}

bool Controller::atLineStart() const noexcept{
    return atTextStart() && active.text->id == 0 && isTopLevel();
}

void Controller::del() noexcept{
    if(hasSelection()){
        Command* cmd = deleteSelection();
        getModel()->mutate(cmd, *this);
    }else if(notAtTextEnd()){
        deleteChar();
    }else if(Construct* c = active.text->nextConstructInPhrase()){
        anchor.setToFrontOf(c->next());
    }else if(isNested()){
        selectConstruct(subphrase()->parent);
    }else if(Line* l = nextLine()){
        Command* cmd = deleteSelectionLine(anchor.text, anchor.index, l->front(), 0);
        getModel()->mutate(cmd, *this);
    }
}

void Controller::backspace() noexcept{
    if(hasSelection()){
        Command* cmd = deleteSelection();
        getModel()->mutate(cmd, *this);
    }else if(notAtTextStart()){
        decrementActiveGrapheme();
        anchor = active;
        deleteChar();
    }else if(Construct* c = active.text->prevConstructInPhrase()){
        anchor.setToBackOf(c->prev());
    }else if(isNested()){
        selectConstruct(subphrase()->parent);
    }else if(Line* l = prevLine()){
        Command* cmd = deleteSelectionLine(l->back(), l->back()->size(), active.text, active.index);
        getModel()->mutate(cmd, *this);
    }
}

void Controller::delWord() noexcept{
    if(hasSelection()){
        Command* cmd = deleteSelection();
        getModel()->mutate(cmd, *this);
    }else if(!atTextEnd()){
        selectNextWord();
        Command* cmd = deleteSelectionText();
        getModel()->mutate(cmd, *this);
    }else{
        del();
    }
}

void Controller::backspaceWord() noexcept{
    if(hasSelection()){
        Command* cmd = deleteSelection();
        getModel()->mutate(cmd, *this);
    }else if(!atTextStart()){
        selectPrevWord();
        Command* cmd = deleteSelectionText();
        getModel()->mutate(cmd, *this);
    }else{
        backspace();
    }
}

void Controller::newline() noexcept{
    if(isNested()) return;
    std::string str = "\n" + std::string(INDENT_SIZE*activeLine()->scope_depth, ' ');
    getModel()->mutate(getInsertSerial(str), *this);
}

void Controller::tab(){
    if(hasSelection()){
        if(isPhraseSelection()){
            del();
        }else{
            Marker active_backup = active;
            Marker anchor_backup = anchor;

            Command* cmd = isForward() ?
                           CommandIndent::indent(anchorLine(), activeLine()) :
                           CommandIndent::indent(activeLine(), anchorLine());
            getModel()->mutate(cmd, *this);

            active = active_backup;
            anchor = anchor_backup;

            if(active.atFirstTextInPhrase()) active.index += INDENT_SIZE;
            if(anchor.atFirstTextInPhrase()) anchor.index += INDENT_SIZE;
        }
    }else{
        size_t spaces = active.countSpacesLeft();
        size_t num_to_insert = INDENT_SIZE - spaces % INDENT_SIZE;
        std::string str(num_to_insert, ' ');
        insertText(str);
    }
}

void Controller::detab() noexcept{
    if(hasSelection()){
        if(isPhraseSelection()){
            del();
        }else{
            Marker active_backup = active;
            Marker anchor_backup = anchor;

            CommandIndent* cmd = isForward() ?
                           CommandIndent::deindent(anchorLine(), activeLine()) :
                           CommandIndent::deindent(activeLine(), anchorLine());
            if(cmd){
                getModel()->mutate(cmd, *this);

                active = active_backup;
                anchor = anchor_backup;

                if(active.atFirstTextInPhrase()){
                    size_t removed = isForward() ? cmd->spaces.back() : cmd->spaces.front();
                    if(active.index >= removed) active.index -= removed;
                    else active.index = 0;
                }

                if(anchor.atFirstTextInPhrase()){
                    size_t removed = isForward() ? cmd->spaces.front() : cmd->spaces.back();
                    if(anchor.index >= removed) anchor.index -= removed;
                    else anchor.index = 0;
                }
            }
        }
    }else{
        size_t spaces = active.countSpacesLeft();
        size_t misaligned = spaces % INDENT_SIZE;
        if(misaligned){
            anchor.index -= misaligned;
            del();
        }else if(spaces){
            anchor.index -= INDENT_SIZE;
            del();
        }
    }
}

void Controller::keystroke(const std::string& str){
    if(hasSelection() | (active.index==0)){
        insertText(str);
        return;
    }else if(str == " "){
        std::string_view word = active.checkKeyword();
        const std::string& sub = Keywords::lookup(word);
        insertText(str);

        if(!sub.empty()){
            anchor.index -= (word.size()+2);
            Command* rm = CommandText::remove(active.text, anchor.index, active.index-anchor.index);
            rm->redo(*this);
            active.index = anchor.index;
            Command* ins = insertSerialNoSelection(sub);
            rm->undo(*this);
            Command* replace = new CommandPair(rm, ins);
            getModel()->mutate(replace, *this);
        }
    }else{
        assert(str.size() == 1);
        uint32_t second = static_cast<uint8_t>(str[0]);
        uint32_t first = active.codepointLeft();
        const std::string& sub = Shorthand::lookup(first, second);

        if(!sub.empty()){
            insertText(str);
            anchor.index--;
            anchor.decrementCodepoint();
            Command* rm = CommandText::remove(active.text, anchor.index, active.index-anchor.index);
            rm->redo(*this);
            active.index = anchor.index;
            Command* ins = insertSerialNoSelection(sub);
            rm->undo(*this);
            Command* replace = new CommandPair(rm, ins);
            getModel()->mutate(replace, *this);
            return;
        }else if(CloseSymbol::isClosing(str[0])){
            if(active.onlySpacesLeft()){
                Line* active_line = activeLine();
                if(Line* l = active_line->prev()){
                    if(l->scope_depth){
                        anchor.setToFrontOf(active_line);
                        std::string indented(INDENT_SIZE*(l->scope_depth-1)+1, ' ');
                        indented.back() = str[0];
                        insertText(indented);
                        return;
                    }
                }
            }
        }

        insertText(str);
    }
}

void Controller::insertText(const std::string& str){
    if(hasSelection()){
        Command* a = deleteSelection();
        consolidateLeft();
        Command* b = insertFirstChar(str);
        Command* cmd = new CommandPair(a,b);
        getModel()->mutate(cmd, *this);
    }else{
        const auto& undo_stack = getModel()->undo_stack;
        if(!undo_stack.empty() && undo_stack.back()->isCharacterInsertion())
            insertAdditionalChar(undo_stack.back(), str);
        else if(!undo_stack.empty() && undo_stack.back()->isPairInsertion())
            insertAdditionalChar(static_cast<CommandPair*>(undo_stack.back())->b, str);
        else
            getModel()->mutate(insertFirstChar(str), *this);
    }
}

void Controller::insertSerial(const std::string& src){
    getModel()->mutate(getInsertSerial(src), *this);
}

void Controller::setBothToFrontOf(Text* t) noexcept{
    active.setToFrontOf(t);
    anchor = active;
}

void Controller::setBothToBackOf(Text* t) noexcept{
    active.setToBackOf(t);
    anchor = active;
}

Selection Controller::selection() const noexcept{
    return isForward() ? Selection(anchor, active) : Selection(active, anchor);
}

void Controller::consolidateToActive() noexcept{
    anchor = active;
}

void Controller::consolidateToAnchor() noexcept{
    active = anchor;
}

void Controller::consolidateRight() noexcept{
    if(isForward()) consolidateToActive();
    else consolidateToAnchor();
}

void Controller::consolidateLeft() noexcept{
    if(isForward()) consolidateToAnchor();
    else consolidateToActive();
}

bool Controller::isForward() const noexcept{
    if(isTextSelection()) return active.index > anchor.index;
    else if(isPhraseSelection()) return active.text->id > anchor.text->id;
    else return activeLine()->id > anchorLine()->id;
}

bool Controller::isTopLevel() const noexcept{
    return active.text->isTopLevel();
}

bool Controller::isNested() const noexcept{
    return active.text->isNested();
}

bool Controller::isTextSelection() const noexcept{
    return active.text == anchor.text;
}

bool Controller::isPhraseSelection() const noexcept{
    return active.phrase() == anchor.phrase();
}

char Controller::charRight() const noexcept{
    return active.charRight();
}

char Controller::charLeft() const noexcept{
    return active.charLeft();
}

Phrase* Controller::phrase() const noexcept{
    return active.phrase();
}

Subphrase* Controller::subphrase() const noexcept{
    assert(isPhraseSelection());
    return active.subphrase();
}

Line* Controller::activeLine() const noexcept{
    return active.parentAsLine();
}

Line* Controller::anchorLine() const noexcept{
    return anchor.parentAsLine();
}

Line* Controller::nextLine() const noexcept{
    return activeLine()->next();
}

Line* Controller::prevLine() const noexcept{
    return activeLine()->prev();
}

bool Controller::atTextEnd() const noexcept{
    return active.atTextEnd();
}

bool Controller::atTextStart() const noexcept{
    return active.atTextStart();
}

bool Controller::notAtTextEnd() const noexcept{
    return active.notAtTextEnd();
}

bool Controller::notAtTextStart() const noexcept{
    return active.notAtTextStart();
}

void Controller::incrementActiveCodepoint() noexcept{
    active.incrementCodepoint();
}

void Controller::decrementActiveCodepoint() noexcept{
    active.decrementCodepoint();
}

void Controller::incrementActiveGrapheme() noexcept{
    active.incrementGrapheme();
}

void Controller::decrementActiveGrapheme() noexcept{
    active.decrementGrapheme();
}

void Controller::incrementToNextWord() noexcept{
    active.incrementToNextWord();
}

void Controller::decrementToPrevWord() noexcept{
    active.decrementToPrevWord();
}

void Controller::selectLine(const Line* l) noexcept{
    anchor.setToFrontOf(l);
    if(Line* n = l->next()) active.setToFrontOf(n);
    else active.setToBackOf(l);
}

void Controller::selectConstruct(const Construct* c) noexcept{
    #ifndef HOPE_TYPESET_HEADLESS
    if(c->constructCode() != MARKERLINK){
        active.setToFrontOf(c->next());
        anchor.setToBackOf(c->prev());
    }else{
        static_cast<const MarkerLink*>(c)->clickThrough();
    }
    #else
    active.setToFrontOf(c->next());
    anchor.setToBackOf(c->prev());
    #endif
}

void Controller::deleteChar(){
    const auto& undo_stack = getModel()->undo_stack;
    if(!undo_stack.empty() && undo_stack.back()->isCharacterDeletion())
        deleteAdditionalChar(undo_stack.back());
    else
        deleteFirstChar();
}

void Controller::deleteAdditionalChar(Command* cmd){
    RemoveChars* rm = static_cast<RemoveChars*>(cmd);
    if(rm->t == active.text){
        if(rm->index == active.index){
            Model* m = getModel();
            m->premutate();
            rm->removeAdditionalChar();
            m->postmutate();
        }else if(rm->index == active.index+graphemeSize(active.text->str, active.index)){
            Model* m = getModel();
            m->premutate();
            rm->removeCharLeft();
            m->postmutate();
        }else{
            deleteFirstChar();
        }
    }else{
        deleteFirstChar();
    }
}

void Controller::deleteFirstChar(){
    RemoveChars* rm = new RemoveChars(active.text, active.index);
    getModel()->mutate(rm, *this);
}

Command* Controller::deleteSelection(){
    if(isTextSelection()) return deleteSelectionText();
    else if(isPhraseSelection()) return deleteSelectionPhrase();
    else return deleteSelectionLine();
}

Command* Controller::deleteSelectionText(){
    Command* cmd = active.index > anchor.index ?
                   CommandText::remove(active.text, anchor.index, active.index-anchor.index) :
                   CommandText::remove(active.text, active.index, anchor.index-active.index);
    return cmd;
}

Command* Controller::deleteSelectionPhrase(){
    if(isForward()) return deleteSelectionPhrase(anchor.text, anchor.index, active.text, active.index);
    else return deleteSelectionPhrase(active.text, active.index, anchor.text, anchor.index);
}

Command* Controller::deleteSelectionPhrase(Text* tL, size_t iL, Text* tR, size_t iR){
    Command* cmd = CommandPhrase::remove(tL, iL, tR, iR);
    return cmd;
}

Command* Controller::deleteSelectionLine(){
    if(isForward()) return deleteSelectionLine(anchor.text, anchor.index, active.text, active.index);
    else return deleteSelectionLine(active.text, active.index, anchor.text, anchor.index);
}

Command* Controller::deleteSelectionLine(Text* tL, size_t iL, Text* tR, size_t iR){
    return CommandLine::remove(tL, iL, tR, iR);
}

void Controller::insertAdditionalChar(Command* cmd, const std::string& str){
    InsertChars* in = static_cast<InsertChars*>(cmd);
    if((in->t == active.text) & (in->index+in->inserted.size() == active.index)){
        Model* m = getModel();
        m->premutate();
        in->insertAdditionalChar(str);
        m->postmutate();
        active.index += str.size();
        anchor.index = active.index;
    }else{
        Command* cmd = insertFirstChar(str);
        getModel()->mutate(cmd, *this);
    }
}

Command* Controller::insertFirstChar(const std::string& str){
    InsertChars* rm = new InsertChars(active.text, active.index, str);
    return rm;
}

Command* Controller::getInsertSerial(const std::string& str){
    if(hasSelection()){
        Command* a = deleteSelection();
        a->redo(*this);
        Command* b = insertSerialNoSelection(str);
        a->undo(*this);
        return new CommandPair(a, b);
    }else{
        return insertSerialNoSelection(str);
    }
}

Command* Controller::insertSerialNoSelection(const std::string& str){
    std::vector<Line*> lines = Model::linesFromSerial(str);
    if(lines.size() > 1){
        return CommandLine::insert(active.text, active.index, lines);
    }else if(lines[0]->numTexts() > 1){
        return CommandPhrase::insert(active.text, active.index, lines[0]);
    }else{
        std::string str = lines[0]->front()->str;
        delete lines[0];
        return CommandText::insert(active.text, active.index, str);
    }
}

#ifndef HOPE_TYPESET_HEADLESS
void Controller::clickTo(const Phrase* p, double x, double y) noexcept{
    if(x <= p->x){
        setBothToFrontOf(p->front());
    }else if(x >= p->x + p->width){
        setBothToBackOf(p->back());
    }else{
        Text* t = p->textLeftOf(x);

        if(t->containsX(x)){
            active.setToPointOf(t, x);
            consolidateToActive();
        }else if(Construct* c = t->nextConstructInPhrase()){
            clickTo(c, x, y);
        }else{
            setBothToBackOf(t);
        }
    }
}

void Controller::clickTo(const Construct* c, double x, double y) noexcept{
    if(Subphrase* s = c->argAt(x, y)) clickTo(s, x, y);
    else selectConstruct(c);
}

void Controller::shiftClick(const Phrase* p, double x) noexcept{
    if(x <= p->x) active.setToFrontOf(p);
    else if(x >= p->x + p->width) active.setToBackOf(p);
    else{
        Text* t = p->textLeftOf(x);
        Construct* c = t->nextConstructInPhrase();
        if(c && x > c->x + c->width/2) active.setToFrontOf(c->next());
        else active.setToPointOf(p->textLeftOf(x), x);
    }
}

double Controller::xActive() const{
    return active.x();
}

double Controller::xAnchor() const{
    return anchor.x();
}
#endif

uint32_t Controller::advance() noexcept{
    assert(notAtTextEnd());
    return static_cast<uint8_t>(active.text->at(active.index++));
}

bool Controller::peek(char ch) const noexcept{
    return notAtTextEnd() && charRight() == ch;
}

void Controller::skipWhitespace() noexcept{
    while(notAtTextEnd() && (charRight() == ' ')) active.index++;
    consolidateToActive();
}

uint32_t Controller::scan() noexcept{
    if(notAtTextEnd()){
        return scanGlyph();
    }else if(Construct* c = active.text->nextConstructInPhrase()){
        active.setToFrontOf(c->next());
        return constructScannerCode(c->constructCode());
    }else if(isNested()){
        active.setToFrontOf(subphrase()->textRightOfSubphrase());
        return CLOSE;
    }else if(Line* l = nextLine()){
        active.setToFrontOf(l);
        return '\n';
    }else{
        return '\0';
    }
}

uint32_t Controller::scanGlyph() noexcept{
    uint32_t ch = advance();
    if(isAscii(ch)){
        return ch;
    }else if(sixthBitUnset(ch)){
        return ch | (advance() << 8);
    }else if(fifthBitUnset(ch)){
        uint32_t bit2 = advance() << 8;
        uint32_t bit3 = advance() << 16;
        return ch | bit2 | bit3;
    }else{
        uint32_t bit2 = advance() << 8;
        uint32_t bit3 = advance() << 16;
        uint32_t bit4 = advance() << 24;
        return ch | bit2 | bit3 | bit4;
    }
}

void Controller::selectToIdentifierEnd() noexcept{
    while(notAtTextEnd() && isAlphaNumeric(charRight())) active.index++;
}

void Controller::selectToNumberEnd() noexcept{
    while(notAtTextEnd() && isNumeric(charRight())) active.index++;
}

bool Controller::selectToStringEnd() noexcept{
    for(;;){
        if(active.index >= active.text->size()){
            if(Text* t = active.text->nextTextInPhrase()){
                active.text = t;
                active.index = 0;
            }else{
                active.index = active.text->size();
                return false;
            }
        }else{
            char ch = active.text->at(active.index++);
            switch (ch) {
                case '"': return true;
                case '\\': active.index++;
            }
        }
    }
}

std::string_view Controller::selectedFlatText() const noexcept{
    return std::string_view(anchor.text->str.data() + anchor.index, active.index-anchor.index);
}

Controller::Controller(const Controller& lhs, const Controller& rhs)
    : active(rhs.active), anchor(lhs.anchor) {}

void Controller::formatBasicIdentifier() const noexcept{
    formatSimple(SEM_ID);
}

void Controller::formatComment() const noexcept{
    formatSimple(SEM_COMMENT);
}

void Controller::formatKeyword() const noexcept{
    formatSimple(SEM_KEYWORD);
}

void Controller::formatNumber() const noexcept{
    active.text->tag(SEM_NUMBER, anchor.index, active.index);
}

void Controller::formatMutableId() const noexcept{
    active.text->tag(SEM_ID_FUN_IMPURE, anchor.index, active.index);
}

void Controller::formatString() const noexcept{
    formatSimple(SEM_STRING);
}

void Controller::format(SemanticType type) const noexcept{
    active.text->tag(type, anchor.index, active.index);
}

void Controller::formatSimple(SemanticType type) const noexcept{
    if(!anchor.text->tags.empty() && anchor.text->tags.back().index == anchor.index)
        anchor.text->tags.back().type = type;
    else
        anchor.text->tags.push_back( SemanticTag(anchor.index, type) );

    active.text->tags.push_back( SemanticTag(active.index, SEM_DEFAULT) );
}

}

}
