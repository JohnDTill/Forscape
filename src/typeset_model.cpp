#include "typeset_model.h"

#include "hope_serial.h"
#include "typeset_all_constructs.h"
#include <typeset_command_list.h>
#include "typeset_line.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"

#ifndef NDEBUG
#include <iostream>
#endif

#include <hope_error.h>
#include "hope_message.h"
#include <hope_scanner.h>
#include <hope_parser.h>
#include <hope_symbol_build_pass.h>
#include <hope_symbol_link_pass.h>
#include <hope_interpreter.h>
#include <unordered_set>

namespace Hope {

namespace Typeset {

Model::Model(){
    lines.push_back( new Line(this) );
    lines[0]->id = 0;
}

Model::~Model(){
    clearRedo();
    for(Command* cmd : undo_stack) delete cmd;
    for(Line* l : lines) delete l;
}

#define TypesetSetupNullary(name) \
    text->getParent()->appendConstruct(new name); \
    text = text->nextTextInPhrase(); \
    break;

#define TypesetSetupConstruct(name, arg) { \
    name* construct = new name(arg); \
    text->getParent()->appendConstruct(construct); \
    text = construct->frontText(); \
    break; }

#define TypesetSetupMatrix(name) { \
    uint16_t rows = static_cast<uint16_t>(src[index++]); \
    uint16_t cols = static_cast<uint16_t>(src[index++]); \
    name* construct = new name(rows, cols); \
    text->getParent()->appendConstruct(construct); \
    text = construct->frontText(); \
    break; }

Model* Model::fromSerial(const std::string& src, bool is_output){
    return new Model(src, is_output);
}

#undef TypesetSetupNullary
#undef TypesetSetupConstruct
#undef TypesetSetupMatrix

std::string Model::toSerial() const {
    std::string out;
    out.resize(serialChars());
    writeString(out);

    return out;
}

std::string Model::run(){
    assert(errors.empty());

    interpreter.run(parser.parse_tree, symbol_builder.symbol_table, static_pass.instantiation_lookup);

    std::string str;

    InterpreterOutput* msg;
    while(interpreter.message_queue.try_dequeue(msg))
        switch(msg->getType()){
            case Hope::InterpreterOutput::Print:
                str += static_cast<PrintMessage*>(msg)->msg;
                delete msg;
                break;
            case Hope::InterpreterOutput::CreatePlot:
                //EVENTUALLY: maybe do something here?
                delete msg;
                break;
            case Hope::InterpreterOutput::AddDiscreteSeries:{
                delete msg;
                break;
            }
            default: assert(false);
        }

    if(interpreter.error_code != Code::ErrorCode::NO_ERROR_FOUND){
        Code::Error error(parser.parse_tree.getSelection(interpreter.error_node), interpreter.error_code);
        str += "\nLine " + error.line() + " - " + error.message();
        errors.push_back(error);
    }

    return str;
}

void Model::runThread(){
    assert(errors.empty());
    interpreter.runThread(parser.parse_tree, symbol_builder.symbol_table, static_pass.instantiation_lookup);
}

void Model::stop(){
    interpreter.stop();
}

Model::Model(const std::string& src, bool is_output)
    : is_output(is_output) {
    lines = linesFromSerial(src);
    for(size_t i = 0; i < lines.size(); i++){
        lines[i]->id = i;
        lines[i]->parent = this;
    }

    performSemanticFormatting();

    #ifndef HOPE_TYPESET_HEADLESS
    calculateSizes();
    updateLayout();
    #endif
}

#define TypesetSetupNullary(name) \
    text->getParent()->appendConstruct(new name); \
    text = text->nextTextInPhrase(); \
    break;

#define TypesetSetupConstruct(name, arg) { \
    name* construct = new name(arg); \
    text->getParent()->appendConstruct(construct); \
    text = construct->frontText(); \
    break; }

#define TypesetSetupMatrix(name) { \
    uint16_t rows = static_cast<uint16_t>(src[index++]); \
    uint16_t cols = static_cast<uint16_t>(src[index++]); \
    name* construct = new name(rows, cols); \
    text->getParent()->appendConstruct(construct); \
    text = construct->frontText(); \
    break; }

std::vector<Line*> Model::linesFromSerial(const std::string& src){
    assert(isValidSerial(src));

    size_t index = 0;
    size_t start = index;
    std::vector<Line*> lines;
    lines.push_back(new Line());
    Text* text = lines.back()->front();

    while(index < src.size()){
        auto ch = src[index++];

        if(ch == OPEN){
            text->setString(&src[start], index-start-1);

            switch (src[index++]) {
                HOPE_TYPESET_PARSER_CASES
                default: assert(false);
            }

            start = index;
        }else if(ch == CLOSE){
            text->setString(&src[start], index-start-1);
            start = index;
            Subphrase* closed_subphrase = static_cast<Subphrase*>(text->getParent());
            text = closed_subphrase->textRightOfSubphrase();
        }else if(ch == '\n' && text->isTopLevel()){
            text->setString(&src[start], index-start-1);

            start = index;
            lines.push_back(new Line());
            text = lines.back()->front();
        }
    }

    text->setString(&src[start], index-start);

    return lines;
}

#undef TypesetSetupNullary
#undef TypesetSetupConstruct
#undef TypesetSetupMatrix

#ifdef HOPE_SEMANTIC_DEBUGGING
std::string Model::toSerialWithSemanticTags() const{
    std::string out = lines[0]->toStringWithSemanticTags();
    for(size_t i = 1; i < lines.size(); i++){
        out += '\n';
        out += lines[i]->toStringWithSemanticTags();
    }

    return out;
}
#endif

Line* Model::appendLine(){
    Line* l = new Line(this);
    l->id = lines.size();
    lines.push_back(l);
    return l;
}

Line* Model::lastLine() const noexcept{
    return lines.back();
}

void Model::search(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const{
    assert(!str.empty());
    for(Line* l : lines) l->search(str, hits, use_case, word);
}

bool Model::empty() const noexcept{
    return lines.size() == 1 && lines[0]->empty();
}

Text* Model::firstText() const noexcept{
    return lines[0]->front();
}

Text* Model::lastText() const noexcept{
    return lines.back()->back();
}

size_t Model::serialChars() const noexcept {
    size_t serial_chars = lines.size() - 1;
    for(Line* l : lines) serial_chars += l->serialChars();
    return serial_chars;
}

void Model::writeString(std::string& out) const noexcept {
    size_t curr = 0;
    lines.front()->writeString(out, curr);
    for(size_t i = 1; i < lines.size(); i++){
        out[curr++] = '\n';
        lines[i]->writeString(out, curr);
    }

    assert(isValidSerial(out));
}

Line* Model::nextLine(const Line* l) const noexcept{
    return l->id+1 < lines.size() ? lines[l->id+1] : nullptr;
}

Line* Model::prevLine(const Line* l) const noexcept{
    return l->id > 0 ? lines[l->id-1] : nullptr;
}

Line* Model::nextLineAsserted(const Line* l) const noexcept{
    assert(l->id+1 < lines.size());
    return lines[l->id+1];
}

Line* Model::prevLineAsserted(const Line* l) const noexcept{
    assert(l->id > 0);
    return lines[l->id-1];
}

#ifndef HOPE_TYPESET_HEADLESS
Line* Model::nearestLine(double y) const noexcept{
    auto search = std::lower_bound(
                    lines.rbegin(),
                    lines.rend(),
                    y,
                    [](Line* l, double y){return l->y - LINE_VERTICAL_PADDING/2 > y;}
                );

    return (search != lines.rend()) ? *search : lines[0];
}

Line* Model::nearestAbove(double y) const noexcept{
    auto search = std::lower_bound(
                    lines.rbegin(),
                    lines.rend(),
                    y,
                    [](Line* l, double y){return l->y + l->height() > y;}
                );

    return (search != lines.rend()) ? *search : lines[0];
}

Construct* Model::constructAt(double x, double y) const noexcept{
    return nearestLine(y)->constructAt(x, y);
}

void Model::updateWidth() noexcept{
    width = 0;
    for(Line* l : lines) width = std::max(width, l->width);
}

double Model::getWidth() noexcept{
    updateWidth();
    return width;
}

void Model::updateHeight() noexcept{
    height = lines.back()->yBottom();
}

double Model::getHeight() noexcept{
    updateHeight();
    return height;
}
#endif

size_t Model::numLines() const noexcept{
    return lines.size();
}

void Model::appendSerialToOutput(const std::string& src){
    assert(is_output);

    Controller controller(this);
    controller.insertSerial(src);

    //EVENTUALLY: need to guard against large horizontal prints
    static constexpr size_t MAX_LINES = 8192;
    if(lines.size() > MAX_LINES){
        size_t lines_removed = lines.size() - MAX_LINES;
        for(size_t i = 0; i < lines_removed; i++) delete lines[i];
        lines.erase(lines.begin(), lines.begin() + lines_removed);
        for(size_t i = 0; i < MAX_LINES; i++) lines[i]->id = i;
    }

    #ifndef HOPE_TYPESET_HEADLESS
    calculateSizes();
    updateLayout();
    #endif
}

void Model::mutate(Command* cmd, Controller& controller){
    premutate();
    cmd->redo(controller);
    undo_stack.push_back(cmd);
    postmutate();
}

void Model::clearRedo(){
    for(Command* cmd : redo_stack) delete cmd;
    redo_stack.clear();
}

void Model::undo(Controller& controller){
    if(!undo_stack.empty()){
        clearFormatting();
        Command* cmd = undo_stack.back();
        undo_stack.pop_back();
        cmd->undo(controller);
        redo_stack.push_back(cmd);
        performSemanticFormatting();

        #ifndef HOPE_TYPESET_HEADLESS
        calculateSizes();
        updateLayout();
        #endif
    }
}

void Model::redo(Controller& controller){
    if(!redo_stack.empty()){
        clearFormatting();
        Command* cmd = redo_stack.back();
        redo_stack.pop_back();
        cmd->redo(controller);
        undo_stack.push_back(cmd);
        performSemanticFormatting();

        #ifndef HOPE_TYPESET_HEADLESS
        calculateSizes();
        updateLayout();
        #endif
    }
}

bool Model::undoAvailable() const noexcept{
    return !undo_stack.empty();
}

bool Model::redoAvailable() const noexcept{
    return !redo_stack.empty();
}

void Model::remove(size_t start, size_t stop) noexcept{
    lines.erase(lines.begin()+start, lines.begin()+stop);
    for(size_t i = start; i < lines.size(); i++)
        lines[i]->id = i;
}

void Model::insert(const std::vector<Line*>& l){
    for(size_t i = l.front()->id; i < lines.size(); i++)
        lines[i]->id += l.size();
    lines.insert(lines.begin() + l.front()->id, l.begin(), l.end());
}

Marker Model::begin() const noexcept{
    return Marker(firstText(), 0);
}

#ifndef HOPE_TYPESET_HEADLESS
void Model::calculateSizes(){
    for(Line* l : lines) l->updateSize();
}

void Model::updateLayout(){
    double y = 0;

    for(Line* l : lines){
        l->y = y;
        l->updateLayout();
        y += l->height() + LINE_VERTICAL_PADDING;
    }
}

void Model::paint(Painter& painter) const{
    for(Line* l : lines){
        l->paint(painter);

        #ifdef HOPE_TYPESET_LAYOUT_DEBUG
        painter.drawHorizontalConstructionLine(l->y - LINE_VERTICAL_PADDING/2);
        #endif
    }
}

void Model::paint(Painter& painter, double, double yT, double, double yB) const{
    Line* start = nearestAbove(yT);
    Line* end = nearestLine(yB);

    for(size_t i = start->id; i <= end->id; i++){
        Line* l = lines[i];
        l->paint(painter);

        #ifdef HOPE_TYPESET_LAYOUT_DEBUG
        painter.drawHorizontalConstructionLine(l->y - LINE_VERTICAL_PADDING/2);
        #endif
    }
}

void Model::paintGroupings(Painter& painter, const Marker& loc) const{
    auto open_lookup = parser.open_symbols.find(loc);
    if(open_lookup != parser.open_symbols.end()){
        loc.text->paintGrouping(painter, loc.index);
        Typeset::Marker right = open_lookup->second;
        right.decrementCodepoint();
        right.text->paintGrouping(painter, right.index);
    }

    auto close_lookup = parser.close_symbols.find(loc);
    if(close_lookup != parser.close_symbols.end()){
        Typeset::Marker rstart = loc;
        rstart.decrementCodepoint();
        loc.text->paintGrouping(painter, rstart.index);
        Typeset::Marker left = close_lookup->second;
        left.text->paintGrouping(painter, left.index);
    }

    //EVENTUALLY: delete me when scope lookup has adequate testing
    #ifdef DRAW_ACTIVE_SCOPE
    size_t scope = symbol_builder.symbol_table.containingScope(loc);
    const Code::ScopeSegment& seg = symbol_builder.symbol_table.scopes[scope];
    painter.drawNarrowCursor(seg.start.x(), seg.start.y(), 12);
    if(scope+1 < symbol_builder.symbol_table.scopes.size()){
        const Code::ScopeSegment& seg = symbol_builder.symbol_table.scopes[scope+1];
        painter.drawNarrowCursor(seg.start.x(), seg.start.y(), 12);
    }else{
        painter.drawNarrowCursor(lastLine()->x + lastLine()->width, lastText()->y, 12);
    }
    #endif
}

Selection Model::idAt(double x, double y) noexcept{
    Line* l = nearestLine(y);
    if(!l->containsY(y)) return Selection();

    Text* t = l->textNearest(x, y);
    if(!t->containsX(x) || !t->containsY(y)) return Selection();

    size_t index = t->charIndexLeft(x);

    SemanticType type = t->getTypeLeftOf(index);
    if(!isId(type)) return Selection();

    size_t start = 0;
    size_t end = t->numChars();
    for(auto it = t->tags.rbegin(); it != t->tags.rend(); it++){
        if(it->index <= index){
            start = it->index;
            break;
        }
        end = it->index;
    }

    return Selection(t, start, end);
}

ParseNode Model::parseNodeAt(double x, double y) const noexcept {
    return nearestLine(y)->parseNodeAt(x, y);
}

#ifndef NDEBUG
void Model::populateDocMapParseNodes(std::unordered_set<ParseNode>& nodes) const noexcept{
    for(Line* l : lines) l->populateDocMapParseNodes(nodes);
}
#endif
#endif

#ifndef NDEBUG
std::string Model::parseTreeDot() const {
    return parser.parse_tree.toGraphviz();
}
#endif

Selection Model::idAt(const Marker& marker) noexcept{
    //EVENTUALLY this is broken for subscripts.
    //You need to fundamentally rethink how you find this, perhaps a binary search.

    for(size_t i = marker.text->tags.size(); i-->0;){
        const SemanticTag& tag = marker.text->tags[i];
        if(tag.index == marker.index){
            if(!isId(tag.type)) continue;
            size_t start = marker.index;
            size_t end = i+1==marker.text->tags.size() ? marker.text->numChars() : marker.text->tags[i+1].index;
            return Selection(marker.text, start, end);
        }else if(tag.index < marker.index){
            if(!isId(tag.type)) return Selection();
            size_t start = tag.index;
            size_t end = i+1==marker.text->tags.size() ? marker.text->numChars() : marker.text->tags[i+1].index;
            return Selection(marker.text, start, end);
        }
    }

    return Selection();
}

Selection Model::find(const std::string& str) noexcept{
    Text* t = firstText();
    for(;;){
        auto it = t->getString().find(str);
        if(it != std::string::npos){
            size_t start = it;
            size_t end = start + str.size();

            return Selection(t, start, end);
        }

        if(Construct* c = t->nextConstructInPhrase()){
            Text* candidate = c->frontText();
            t = candidate ? candidate : t->nextTextAsserted();
        }else if(t->isNested()){
            t = t->getParent()->asSubphrase()->textRightOfSubphrase();
        }else if(Line* l = t->getParent()->asLine()->next()){
            t = l->front();
        }else{
            break;
        }
    }

    return Selection();
}

void Model::clearFormatting() noexcept{
    Text* t = firstText();
    for(;;){
        t->tags.clear();
        #ifndef HOPE_TYPESET_HEADLESS
        t->parse_nodes.clear();
        #endif

        if(Construct* c = t->nextConstructInPhrase()){
            Text* candidate = c->frontText();
            t = candidate ? candidate : t->nextTextAsserted();
        }else if(t->isNested()){
            t = t->getParent()->asSubphrase()->textRightOfSubphrase();
        }else if(Line* l = t->getParent()->asLine()->next()){
            t = l->front();
        }else{
            break;
        }
    }

    errors.clear();
    warnings.clear();
}

void Model::performSemanticFormatting(){
    if(is_output) return;

    clearFormatting();
    scanner.scanAll();
    parser.parseAll();
    symbol_builder.resolveSymbols();
    static_pass.resolve();
}

void Model::premutate() noexcept{
    clearRedo();
    clearFormatting();
}

void Model::postmutate(){
    performSemanticFormatting();

    #ifndef HOPE_TYPESET_HEADLESS
    calculateSizes();
    updateLayout();
    #endif
}

void Model::rename(const std::vector<Selection>& targets, const std::string& name, Controller& c){
    CommandList* lst = new CommandList();
    for(auto it = targets.rbegin(); it != targets.rend(); it++){
        Controller c(*it);
        lst->cmds.push_back( c.deleteSelection() );
        c.consolidateLeft();
        lst->cmds.push_back( c.insertFirstChar(name) );
    }
    mutate(lst, c);
}

}

}
