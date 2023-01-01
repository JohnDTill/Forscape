#include "typeset_model.h"

#include "forscape_program.h"
#include "forscape_serial.h"
#include "typeset_all_constructs.h"
#include <typeset_command_list.h>
#include "typeset_line.h"
#include "typeset_subphrase.h"
#include "typeset_text.h"

#include <forscape_error.h>
#include "forscape_message.h"
#include <forscape_scanner.h>
#include <forscape_parser.h>
#include <forscape_symbol_lexical_pass.h>
#include <forscape_symbol_link_pass.h>
#include <forscape_interpreter.h>
#include <fstream>
#include <unordered_set>

namespace Forscape {

namespace Typeset {

#ifdef TYPESET_MEMORY_DEBUG
FORSCAPE_UNORDERED_SET<Model*> Model::all;
#endif

Model::Model(){
    #ifdef TYPESET_MEMORY_DEBUG
    all.insert(this);
    #endif
    lines.push_back( new Line(this) );
    lines[0]->id = 0;
}

Model::~Model(){
    clearRedo();
    for(Command* cmd : undo_stack) delete cmd;
    for(Line* l : lines) delete l;

    #ifdef TYPESET_MEMORY_DEBUG
    all.erase(this);
    #endif
}

void Model::clear() noexcept{
    clearRedo();
    for(Command* cmd : undo_stack) delete cmd;
    undo_stack.clear();
    for(Line* l : lines) delete l;
    lines.clear();

    lines.push_back( new Line(this) );
    lines[0]->id = 0;
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

    interpreter.run(parser.parse_tree, &symbol_builder.symbol_table, static_pass.instantiation_lookup);

    std::string str;

    InterpreterOutput* msg;
    while(interpreter.message_queue.try_dequeue(msg))
        switch(msg->getType()){
            case Forscape::InterpreterOutput::Print:
                str += static_cast<PrintMessage*>(msg)->msg;
                delete msg;
                break;
            case Forscape::InterpreterOutput::CreatePlot:
                //EVENTUALLY: maybe do something here?
                delete msg;
                break;
            case Forscape::InterpreterOutput::AddDiscreteSeries:{
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

    #ifndef FORSCAPE_TYPESET_HEADLESS
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
                FORSCAPE_TYPESET_PARSER_CASES
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

#ifdef FORSCAPE_SEMANTIC_DEBUGGING
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

void Model::registerCommaSeparatedNumber(const Selection& sel) noexcept {
    comma_separated_numbers.push_back(sel);
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

#ifndef FORSCAPE_TYPESET_HEADLESS
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

    #ifndef FORSCAPE_TYPESET_HEADLESS
    calculateSizes();
    updateLayout();
    #endif
}

bool Model::isSavedDeepComparison() const {
    if(path.empty()) return empty();

    //Avoid a deep comparison if size from file meta data doesn't match
    //if(std::filesystem::file_size(path) != serialChars()) return false; //Disabled for line endings

    std::ifstream in(path);
    assert(in.is_open());

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string saved_src = buffer.str();
    saved_src.erase( std::remove(saved_src.begin(), saved_src.end(), '\r'), saved_src.end() );

    //MAYDO: no need to convert model to serial, but cost is probably negligible compared to I/O
    std::string curr_src = toSerial();

    return saved_src == curr_src;
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

        #ifndef FORSCAPE_TYPESET_HEADLESS
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

        #ifndef FORSCAPE_TYPESET_HEADLESS
        calculateSizes();
        updateLayout();
        #endif
    }
}

void Model::resetUndoRedo() noexcept {
    clearRedo();
    for(Command* cmd : undo_stack) delete cmd;
    undo_stack.clear();
    redo_stack.clear();
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

#ifndef FORSCAPE_TYPESET_HEADLESS
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

void Model::paint(Painter& painter, double xL, double yT, double xR, double yB) const{
    Line* start = nearestAbove(yT);
    Line* end = nearestLine(yB);

    for(size_t i = start->id; i <= end->id; i++){
        Line* l = lines[i];
        l->paint(painter, xL, xR);

        #ifdef FORSCAPE_TYPESET_LAYOUT_DEBUG
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

void Model::paintNumberCommas(Painter& painter, double xL, double yT, double xR, double yB, const Selection& sel) const {
    for(const Selection& comma_sel : comma_separated_numbers){
        Marker m = comma_sel.right;
        const size_t num_chars = m.index - comma_sel.left.index;
        const double y = m.y();
        if(y >= yB || m.yBot() <= yT) continue;
        const uint8_t script_depth = m.text->scriptDepth();
        const double dx = 3*CHARACTER_WIDTHS[script_depth];
        double x = m.x();
        painter.setScriptLevel(script_depth);
        const double left_x = std::max(x - dx*((num_chars-1)/3), xL);

        while(x > left_x){
            x -= dx;
            if(x < xR) painter.drawComma(x, y, sel.contains(x, y));
        }
    }
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
        #ifndef FORSCAPE_TYPESET_HEADLESS
        t->parse_nodes.clear();
        #endif

        if(Construct* c = t->nextConstructInPhrase()){
            #ifndef FORSCAPE_TYPESET_HEADLESS
            c->pn = NONE;
            #endif
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
    comma_separated_numbers.clear();
}

void Model::performSemanticFormatting(){
    if(is_output) return;

    clearFormatting();
    scanner.scanAll();
    parser.parseAll();
    symbol_builder.resolveSymbols();
    static_pass.resolve(); //DO THIS: the program entry point should run here
}

void Model::premutate() noexcept{
    clearRedo();
    clearFormatting();
}

void Model::postmutate(){
    performSemanticFormatting();

    #ifndef FORSCAPE_TYPESET_HEADLESS
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

Code::ParseTree& Model::parseTree() noexcept{
    return parser.parse_tree;
}

}

}
