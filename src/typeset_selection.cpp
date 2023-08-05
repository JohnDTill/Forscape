#include "typeset_selection.h"

#include "typeset_construct.h"
#include "typeset_line.h"
#include <typeset_matrix.h>
#include "typeset_model.h"
#include "typeset_phrase.h"
#include "typeset_text.h"
#include <array>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Typeset {

#define tL Selection::left.text
#define iL Selection::left.index
#define tR Selection::right.text
#define iR Selection::right.index
#define xL Selection::left.x()
#define xR Selection::right.x()
#define p Selection::phrase()
#define pL Selection::left.phrase()
#define pR Selection::right.phrase()
#define lL pL->asLine()
#define lR pR->asLine()
#define getLines lL->lines

Selection::Selection() noexcept {}

Selection::Selection(const Marker& left, const Marker& right) noexcept
    : right(right), left(left) {
    //assert(inValidState()); //EVENTUALLY: re-enable this
    #ifndef NDEBUG
    //if(!inValidState()) std::cout << "INVALID SELECTION" << std::endl; //EVENTUALLY: eliminate invalid markers
    #endif
}

Selection::Selection(const std::pair<const Marker&, const Marker&>& pair) noexcept
    : right(pair.second), left(pair.first){
    assert(inValidState());
}

Selection::Selection(Text* t, size_t l, size_t r) noexcept
    : right(Marker(t,r)), left(Marker(t,l)) {
    assert(inValidState());
}

std::string Selection::str() const {
    if(isTextSelection()) return selectedTextSelection();
    else if(isPhraseSelection()) return selectedPhrase();
    else return selectedLines();
}

std::string_view Selection::strView() const noexcept {
    assert(isTextSelection());
    assert(iR >= iL);
    return std::string_view(tL->getString().data()+iL, characterSpan());
}

bool Selection::operator==(const Selection& other) const noexcept {
    if(isTextSelection()){
        return other.isTextSelection() && strView() == other.strView();
    }else if(isPhraseSelection()){
        if(!other.isPhraseSelection()) return false;

        size_t sze = tR->id - tL->id;

        if(sze != other.tR->id - other.tL->id)
            return false;

        if(!left.compareRight(other.left) || !right.compareLeft(other.right))
            return false;

        for(size_t i = 1; i < sze; i++){
            Text* A_text = phrase()->text( tL->id+i );
            Text* B_text = other.phrase()->text( other.tL->id+i );
            if(A_text->getString() != B_text->getString()) return false;
        }

        for(size_t i = 0; i < sze; i++){
            Construct* A_con = phrase()->construct( tL->id+i );
            Construct* B_con = other.phrase()->construct( other.tL->id+i );
            if(!A_con->sameContent(B_con)) return false;
        }

        return true;
    }else{
        if(other.isPhraseSelection()) return false;

        size_t sze = pR->id - pL->id;

        if(sze != other.lR->id - other.lL->id)
            return false;

        const std::vector<Line*>& lines = getLines();
        for(size_t i = 1; i < sze; i++){
            Line* A_line = lines[ lL->id+i ];
            Line* B_line = lines[ other.lL->id+i ];
            if(!A_line->sameContent(B_line))
                return false;
        }

        Typeset::Text* A_prefix_t = lL->back();
        Typeset::Selection A_prefix(left, Typeset::Marker(A_prefix_t, A_prefix_t->numChars()));
        Typeset::Text* B_prefix_t = other.lL->back();
        Typeset::Selection B_prefix(other.left, Typeset::Marker(B_prefix_t, B_prefix_t->numChars()));

        Typeset::Selection A_suffex(Typeset::Marker(lR->front(), 0), right);
        Typeset::Selection B_suffex(Typeset::Marker(other.lR->front(), 0), other.right);

        return A_prefix == B_prefix && A_suffex == B_suffex;
    }
}

bool Selection::operator!=(const Selection& other) const noexcept {
    return !(*this == other);
}

bool Selection::startsWith(const Selection& other) const noexcept {
    if(other.isTextSelection()){
        auto prefix = other.strView();
        if(isTextSelection()){
            return strView().substr(0, prefix.size()) == prefix;
        }else{
            return left.strRight().substr(0, prefix.size()) == prefix;
        }
    }else{
        assert(other.isPhraseSelection()); //This is not expected to be called for lines

        if(isTextSelection()) return false;

        if(left.strLeft() != other.left.strLeft()) return false;
        size_t index = left.text->id;
        size_t other_index = left.text->id;
        for(;;){
            Construct* c = phrase()->construct(index);
            Construct* other_c = other.phrase()->construct(other_index);
            if(!c->sameContent(other_c)) return false;
            index++;
            other_index++;
            if(other_index < other.right.text->id){
                if(phrase()->text(index)->getString() != other.phrase()->text(other_index)->getString()) return false;
            }else{
                std::string_view right_bit = other.right.strLeft();
                return right_bit == right.strLeft().substr(0, right_bit.size());
            }
        }
    }
}

size_t Selection::hashDesignedForIdentifiers() const noexcept {
    //The vast majority of identifiers will be text selections.
    if(isTextSelection()) return std::hash<std::string_view>()(strView());

    //Rare non-flat ids have additional text in a single script, e.g. n_a or x^*
    Construct* c = tL->nextConstructAsserted(); //This could fail on newlines
    Text* tc = c->frontTextAsserted(); //I think all id-constructs have args
    size_t h = std::hash<std::string_view>()( std::string_view(tL->getString().data()+iL) );
    h ^= c->constructCode();
    h ^= std::hash<std::string>()(tc->getString());

    return h;
}

bool Selection::isTopLevel() const noexcept {
    return tL->isTopLevel();
}

bool Selection::isNested() const noexcept {
    return tL->isNested();
}

bool Selection::isTextSelection() const noexcept {
    return tL == tR;
}

bool Selection::isPhraseSelection() const noexcept {
    return left.phrase() == right.phrase();
}

bool Selection::isEmpty() const noexcept {
    return left == right;
}

Line* Selection::getStartLine() const noexcept {
    return tL->getLine();
}

std::string Selection::getStartLineAsString() const alloc_except {
    return std::to_string(getStartLine()->id + 1);
}

Model* Selection::getModel() const noexcept {
    assert(left.getModel() == right.getModel());
    return left.getModel();
}

void Selection::search(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const {
    assert(!str.empty());

    if(isTextSelection()) return searchText(str, hits, use_case, word);
    else if(isPhraseSelection()) return searchPhrase(str, hits, use_case, word);
    else return searchLines(str, hits, use_case, word);
}

size_t Selection::getConstructArgSize() const noexcept {
    assert(isPhraseSelection());
    assert(tR->id == tL->id+1);

    return tL->nextConstructAsserted()->numArgs();
}

size_t Selection::getMatrixRows() const noexcept {
    assert(isPhraseSelection());
    assert(tR->id == tL->id+1);
    assert(tL->nextConstructAsserted()->constructCode() == MATRIX);

    return static_cast<Typeset::Matrix*>(tL->nextConstructAsserted())->rows;
}

void Selection::formatBasicIdentifier() const noexcept {
    formatSimple(SEM_ID);
}

void Selection::formatComment() const noexcept {
    formatSimple(SEM_COMMENT);
}

void Selection::formatKeyword() const noexcept {
    formatSimple(SEM_KEYWORD);
}

void Selection::formatNumber() const noexcept {
    tL->tag(SEM_NUMBER, iL, iR);
}

void Selection::formatMutableId() const noexcept {
    tL->tag(SEM_ID_FUN_IMPURE, iL, iR);
}

void Selection::formatString() const noexcept {
    formatSimple(SEM_STRING);
}

void Selection::format(SemanticType type) const noexcept {
    if(isTextSelection()) tL->tag(type, iL, iR);
    else if(isPhraseSelection()){
        tL->tag(type, iL, tL->numChars());
        Text* tc = tL->nextConstructAsserted()->frontTextAsserted();
        tc->tag(type, 0, tc->numChars());
    }
}

void Selection::formatSimple(SemanticType type) const noexcept {
    if(!tL->tags.empty() && tL->tags.back().index == iL)
        tL->tags.back().type = type;
    else
        tL->tags.push_back( SemanticTag(iL, type) );

    tL->tags.push_back( SemanticTag(iR, SEM_DEFAULT) );
}

SemanticType Selection::getFormat() const noexcept {
    return tL->getTypeLeftOf(iL);
}

bool Selection::containsInclusive(const Marker& other) const noexcept {
    return left.precedesInclusive(other) && other.precedesInclusive(right);
}

size_t Selection::topLevelChars() const noexcept {
    if(isTextSelection()){
        return right.index - left.index;
    }else{
        assert(isPhraseSelection());
        assert(left.text->nextTextAsserted() == right.text);
        return right.index + (left.text->numChars() - left.index);
    }
}

#ifdef QT_CORE_LIB
QImage Selection::toPng() const {
    constexpr double UPSCALE = 4;

    std::array<double, 4> dims = getDimensions();
    QImage image(UPSCALE*QSize(dims[2], dims[3]), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter qpainter(&image);
    Painter painter(qpainter, 0, 0, std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    painter.setZoom(UPSCALE);
    painter.setOffset(-dims[0], -dims[1]);

    paint(painter);

    return image;
}

void Selection::toSvg(QSvgGenerator& generator) const {
    constexpr double UPSCALE = 1;

    std::array<double, 4> dims = getDimensions();
    generator.setSize(UPSCALE*QSize(dims[2], dims[3]));
    generator.setViewBox(QRect(0, 0, UPSCALE*dims[2], UPSCALE*dims[3]));

    QPainter qpainter(&generator);
    qpainter.setRenderHint(QPainter::Antialiasing);
    Painter painter(qpainter, 0, 0, std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    painter.setZoom(UPSCALE);
    painter.setOffset(-dims[0], -dims[1]);

    paint(painter);
}
#endif

Phrase* Selection::phrase() const noexcept {
    assert(isPhraseSelection());
    return left.phrase();
}

size_t Selection::characterSpan() const noexcept {
    assert(isTextSelection());
    return iR-iL;
}

size_t Selection::textSpan() const noexcept {
    return right.index - left.index;
}

std::string Selection::selectedTextSelection() const {
    return std::string(tR->view(iL, characterSpan()));
}

std::string Selection::selectedPhrase() const {
    assert(isPhraseSelection());
    size_t serial_chars = (tL->numChars()-iL) + iR + tL->nextConstructInPhrase()->serialChars();
    for(Text* t = tL->nextTextInPhrase(); t != tR; t = t->nextTextInPhrase())
        serial_chars += t->numChars() + t->nextConstructInPhrase()->serialChars();

    std::string out;
    out.resize(serial_chars);
    size_t curr = 0;

    tL->writeString(out, curr, iL);
    tL->nextConstructAsserted()->writeString(out, curr);
    for(Text* t = tL->nextTextAsserted(); t != tR; t = t->nextTextAsserted()){
        t->writeString(out, curr);
        t->nextConstructAsserted()->writeString(out, curr);
    }
    tR->writeString(out, curr, 0, iR);

    return out;
}

std::string Selection::selectedLines() const {
    assert(!isPhraseSelection());

    size_t serial_chars = (tL->numChars()-iL) + iR + 1;
    for(Text* t = lL->back(); t != tL; t = t->prevTextAsserted())
        serial_chars += t->numChars() + t->prevConstructAsserted()->serialChars();
    for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted())
        serial_chars += l->serialChars() + 1;
    for(Text* t = lR->front(); t != tR; t = t->nextTextAsserted())
        serial_chars += t->numChars() + t->nextConstructAsserted()->serialChars();

    std::string out;
    out.resize(serial_chars);
    size_t curr = 0;

    tL->writeString(out, curr, iL);
    for(Text* t = tL; t != lL->back();){
        t->nextConstructAsserted()->writeString(out, curr);
        t = t->nextTextAsserted();
        t->writeString(out, curr);
    }
    out[curr++] = '\n';

    for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted()){
        l->writeString(out, curr);
        out[curr++] = '\n';
    }

    for(Text* t = lR->front(); t != tR; t = t->nextTextAsserted()){
        t->writeString(out, curr);
        t->nextConstructAsserted()->writeString(out, curr);
    }
    tR->writeString(out, curr, 0, iR);

    return out;
}

void Selection::searchText(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const {
    tL->search(str, hits, iL, iR, use_case, word);
}

void Selection::searchPhrase(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const {
    tL->search(str, hits, iL, tL->numChars(), use_case, word);

    tL->nextConstructAsserted()->search(str, hits, use_case, word);
    for(size_t i = tL->id+1; i < tR->id; i++){
        phrase()->text(i)->search(str, hits, use_case, word);
        phrase()->construct(i)->search(str, hits, use_case, word);
    }

    tR->search(str, hits, 0, iR, use_case, word);
}

void Selection::searchLines(const std::string& str, std::vector<Selection>& hits, bool use_case, bool word) const {
    tL->search(str, hits, iL, tL->numChars(), use_case, word);

    for(size_t i = tL->id+1; i < lL->numTexts(); i++){
        lL->construct(i-1)->search(str, hits, use_case, word);
        lL->text(i)->search(str, hits, use_case, word);
    }

    const std::vector<Line*> lines = getLines();
    for(size_t i = lL->id+1; i < lR->id; i++)
        lines[i]->search(str, hits, use_case, word);

    for(size_t i = 0; i < tR->id; i++){
        lR->text(i)->search(str, hits, use_case, word);
        lR->construct(i)->search(str, hits, use_case, word);
    }

    tR->search(str, hits, 0, iR, use_case, word);
}

#ifndef FORSCAPE_TYPESET_HEADLESS
std::array<double, 4> Selection::getDimensions() const noexcept {
    if(isTextSelection()) return getDimensionsText();
    else if(isPhraseSelection()) return getDimensionsPhrase();
    else return getDimensionsLines();
}

std::array<double, 4> Selection::getDimensionsText() const noexcept {
    double x = left.x();
    double y = tL->y;
    double h = tL->height();
    double w = right.x() - x;

    return {x, y, w, h};
}

std::array<double, 4> Selection::getDimensionsPhrase() const noexcept {
    double x = left.x();
    double w = right.x() - x;

    double above_center = std::max(tL->aboveCenter(), tL->nextConstructAsserted()->above_center);
    double under_center = std::max(tL->underCenter(), tL->nextConstructAsserted()->under_center);
    for(size_t i = tL->id+1; i < tR->id; i++){
        above_center = std::max(above_center, p->construct(i)->above_center);
        under_center = std::max(under_center, p->construct(i)->under_center);
    }

    double y = p->y + (p->above_center - above_center);
    double h = above_center+under_center;

    return {x, y, w, h};
}

std::array<double, 4> Selection::getDimensionsLines() const noexcept {
    double above_center = tL->aboveCenter();
    for(size_t i = tL->id; i < lL->numConstructs(); i++)
        above_center = std::max(above_center, lL->construct(i)->above_center);

    double x = lL->x;
    double y = lL->y + (lL->above_center - above_center);
    double h = above_center + lL->under_center +
               (lR->id - lL->id)*Model::LINE_VERTICAL_PADDING;
    double w = lL->width;

    const std::vector<Line*> lines = getLines();
    for(size_t i = lL->id+1; i < lR->id; i++){
        Line* l = lines[i];
        h += l->height();
        w = std::max(w, l->width);
    }

    double under_center = tR->underCenter();
    for(size_t i = tR->id; i-->0;)
        under_center = std::max(under_center, lR->construct(i)->under_center);

    h += lR->above_center + under_center;
    w = std::max(w, right.x());

    return {x, y, w, h};
}
#endif

#ifndef NDEBUG
bool Selection::inValidState() const noexcept {
    if(!left.inValidState()){
        std::cout << "Left marker invalid" << std::endl;
        return false;
    }

    if(!right.inValidState()){
        std::cout << "Right marker invalid" << std::endl;
        return false;
    }

    if(left.getModel() != right.getModel()){
        std::cout << "Markers are not from same model" << std::endl;
        return false;
    }

    if(left.phrase() != right.phrase() && (left.isNested() || right.isNested())){
        std::cout << "Markers are not on same level" << std::endl;
        return false;
    }

    if(isTextSelection()){
        if(iL > iR){
            std::cout << "Left: " << iL << ", Right: " << iR << std::endl;
            return false;
        }else{
            return true;
        }
    }else if(isPhraseSelection()){
        return tR->id > tL->id;
    }else{
        return lR->id > lL->id;
    }
}
#endif

#ifndef FORSCAPE_TYPESET_HEADLESS
bool Selection::contains(double x, double y) const noexcept {
    if(isTextSelection()) return containsText(x, y);
    else if(isPhraseSelection()) return containsPhrase(x, y);
    else return containsLine(x, y);
}

bool Selection::containsWithEmptyMargin(double x, double y) const noexcept {
    if(right == left){
        double x_sel = left.x();
        return tR->containsY(y) && x >= x_sel && x <= x_sel + EMPTY_SUBPHRASE_MARGIN;
    }else{
        return contains(x, y);
    }
}

bool Selection::containsText(double x, double y) const noexcept {
    return tR->containsY(y) && tR->containsXInBounds(x, iL, iR);
}

bool Selection::containsPhrase(double x, double y) const noexcept {
    return phrase()->containsY(y) && x >= xL && x <= xR;
}

bool Selection::containsLine(double x, double y) const noexcept {
    if((y < lL->y) || (y > lR->yBottom())) return false;

    Line* l = lL->nearestLine(y);

    if(l == lL) return x <= l->x + l->width + NEWLINE_EXTRA && x >= tL->xGlobal(iL);
    else if(l == lR) return x >= l->x && x <= tR->xGlobal(iR);
    else return (x >= l->x) & (x <= l->x + l->width + NEWLINE_EXTRA);
}

void Selection::paint(Painter& painter) const {
    if(isTextSelection()){
        tR->paintMid(painter, iL, iR);
    }else if(isPhraseSelection()){
        phrase()->paintMid(painter, tL, iL, tR, iR);
    }else{
        lL->paintAfter(painter, tL, iL);
        for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted())
            l->paint(painter);
        if(iR == 0 && tR == lR->front()) return;
        lR->paintUntil(painter, tR, iR);
    }
}

void Selection::paintSelectionText(Painter& painter, double xLeft, double xRight) const {
    if(iL!=iR){
        double y = tR->y;
        double h = tR->height();
        double x = std::max(tR->xGlobal(iL), xLeft);
        double w = std::min(tR->xGlobal(iR), xRight) - x;
        painter.drawSelection(x, y, w, h);

        const size_t lborder_char = tL->charIndexLeft(xLeft);
        Typeset::Marker m(tR, tR->charIndexLeft(xRight));
        if(!m.atTextEnd()) m.incrementGrapheme();
        const size_t rborder_char = m.index;

        tR->paintMid(painter, std::max(iL, lborder_char), std::min(iR, rborder_char));
    }
}

void Selection::paintSelectionPhrase(Painter& painter, double xLeft, double xRight) const {
    double y = phrase()->y;
    double h = phrase()->height();

    Text* text_left = phrase()->textLeftOf(xLeft);
    Text* text_right = phrase()->textLeftOf(xRight);
    size_t index_left;
    size_t index_right;

    if(text_left->id < tL->id){
        text_left = tL;
        index_left = iL;
    }else if(text_left->id == tL->id){
        index_left = std::max(iL, tL->charIndexLeft(xLeft));
    }else{
        index_left = tL->charIndexLeft(xLeft);
    }

    if(text_right->id > tR->id){
        text_right = tR;
        index_right = iR;
    }else{
        Typeset::Marker m(text_right, text_right->charIndexLeft(xRight));
        if(!m.atTextEnd()) m.incrementGrapheme();
        if(text_right->id == tR->id){
            index_right = std::min(iR, m.index);
        }else{
            index_right = m.index;
        }
    }

    if(text_left == text_right){
        if(text_left != tL){
            text_left = text_left->prevTextAsserted();
            index_left = text_left->numChars();
        }else{
            text_right = text_right->nextTextAsserted();
            index_right = 0;
        }
    }

    double x = tL->xGlobal(iL);
    double w = tR->xGlobal(iR) - x;
    painter.drawSelection(x, y, w, h);

    phrase()->paintMid(painter, text_left, index_left, text_right, index_right);
}

void Selection::paintSelectionLines(Painter& painter, double xLeft, double yT, double xRight, double yB) const {
    double y = lL->y;
    double h = lL->height();

    double x = tL->x + tL->xLocal(iL);
    double w = lL->width - tL->xPhrase(iL) + NEWLINE_EXTRA;
    painter.drawSelection(x, y, w, h);
    lL->paintAfter(painter, tL, iL);

    Line* l = lL->nextAsserted();
    Line* candidate = l->nearestAbove(yT);
    if(candidate->id > l->id) l = candidate;
    for(; l != lR; l = l->nextAsserted()){
        if(l->y > yB) return;
        painter.drawSelection(l->x, l->y, l->width + NEWLINE_EXTRA, l->height());
        l->paint(painter, xLeft, xRight);
    }

    if(iR == 0 && tR == lR->front()) return;

    x = lR->x;
    y = lR->y;
    w = tR->xGlobal(iR) - x;
    h = lR->height();
    painter.drawSelection(x, y, w, h);
    lR->paintUntil(painter, tR, iR);
}

void Selection::paintSelection(Painter& painter, double xLeft, double yT, double xRight, double yB) const {
    painter.setSelectionMode();

    if(isTextSelection()) paintSelectionText(painter, xLeft, xRight);
    else if(isPhraseSelection()) paintSelectionPhrase(painter, xLeft, xRight);
    else paintSelectionLines(painter, xLeft, yT, xRight, yB);

    painter.exitSelectionMode();
}

void Selection::paintError(Painter& painter) const {
    if(isTextSelection() && iL==iR) paintErrorSelectionless(painter);
    else if(isTextSelection()) paintErrorText(painter);
    else if(isPhraseSelection()) paintErrorPhrase(painter);
    else paintErrorLines(painter);
}

void Selection::paintErrorSelectionless(Painter& painter) const {
    painter.drawError(xR, tL->y, EMPTY_SUBPHRASE_MARGIN, tR->height());
}

void Selection::paintErrorText(Painter& painter) const {
    double x = tR->xGlobal(iL);
    double y = tR->y;
    double w = tR->xGlobal(iR) - x;
    double h = tR->height();

    painter.drawError(x, y, w, h);
}

void Selection::paintErrorPhrase(Painter& painter) const {
    double x = tL->xGlobal(iL);
    double y = phrase()->y;
    double w = tR->xGlobal(iR) - x;
    double h = phrase()->height();

    painter.drawError(x, y, w, h);
    phrase()->paintMid(painter, tL, iL, tR, iR);
}

void Selection::paintErrorLines(Painter& painter) const {
    double x = xL;
    double y = lL->y;
    double w = lL->width + NEWLINE_EXTRA - x;
    double h = lL->height();

    painter.drawError(x, y, w, h);
    lL->paintAfter(painter, tL, iL);

    const std::vector<Line*> lines = getLines();
    for(size_t i = lL->id+1; i < lR->id; i++){
        Line* l = lines[i];
        painter.drawError(l->x, l->y, l->width, l->height());
        l->paint(painter);
    }

    x = lR->x;
    y = lR->y;
    w = xR - x;
    h = lR->height();

    if(!(right.atFirstTextInPhrase() && right.atTextStart())){
        painter.drawError(x, y, w, h);
        lR->paintUntil(painter, tR, iR);
    }
}

void Selection::paintHighlight(Painter& painter) const {
    assert(!isTextSelection() || iL != iR);
    if(isTextSelection()) paintHighlightText(painter);
    else if(isPhraseSelection()) paintHighlightPhrase(painter);
    else assert(false);
}

void Selection::paintHighlightText(Painter& painter) const {
    double x = tR->xGlobal(iL);
    double y = tR->y;
    double w = tR->xGlobal(iR) - x;
    double h = tR->height();

    painter.drawHighlight(x, y, w, h);
}

void Selection::paintHighlightPhrase(Painter& painter) const {
    double x = tL->xGlobal(iL);
    double y = phrase()->y;
    double w = tR->xGlobal(iR) - x;
    double h = phrase()->height();

    painter.drawHighlight(x, y, w, h);
}

double Selection::yTop() const noexcept {
    if(isTextSelection()) return tL->y;
    else if(isPhraseSelection()) return phrase()->y;
    else return lL->y;
}

double Selection::yBot() const noexcept {
    if(isTextSelection()) return tR->yBot();
    else if(isPhraseSelection()) return phrase()->yBottom();
    else return lR->yBottom();
}

double Selection::yTopPhrase() const noexcept {
    return tL->getParent()->y;
}

double Selection::yBotPhrase() const noexcept {
    return tL->getParent()->yBottom();
}

bool Selection::overlapsY(double yT, double yB) const noexcept {
    const double yT_this = yTop();
    const double yB_this = yBot();
    return (yT_this >= yT && yT_this <= yB) || (yB_this >= yT && yB_this <= yB);
}

void Selection::mapConstructToParseNode(ParseNode pn) const noexcept {
    assert(isPhraseSelection());
    assert(tR->id == tL->id+1);

    tL->nextConstructAsserted()->pn = pn;
}
#endif

}

}
