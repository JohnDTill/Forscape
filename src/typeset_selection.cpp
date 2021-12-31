#include "typeset_selection.h"

#include "typeset_construct.h"
#include "typeset_line.h"
#include "typeset_model.h"
#include "typeset_phrase.h"
#include "typeset_text.h"
#include <array>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

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
    //assert(inValidState());
    #ifndef NDEBUG
    if(!inValidState()) std::cout << "INVALID MARKER STATE" << std::endl;
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

std::string Selection::str() const{
    if(isTextSelection()) return selectedTextSelection();
    else if(isPhraseSelection()) return selectedPhrase();
    else return selectedLines();
}

std::string_view Selection::strView() const noexcept{
    assert(isTextSelection());
    assert(iR >= iL);
    return std::string_view(tL->str.data()+iL, characterSpan());
}

bool Selection::operator==(const Selection& other) const noexcept{
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
            if(A_text->str != B_text->str) return false;
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

        assert(false); //DO THIS - finish implementing if necessary
        return true;
    }
}

bool Selection::operator!=(const Selection& other) const noexcept{
    return !(*this == other);
}

size_t Selection::hashDesignedForIdentifiers() const noexcept{
    //The vast majority of identifiers will be text selections.
    if(isTextSelection()) return std::hash<std::string_view>()(strView());

    //Rare non-flat ids have additional text in a single script, e.g. n_a or x^*
    Construct* c = tL->nextConstructAsserted(); //This could fail on newlines
    Text* tc = c->frontTextAsserted(); //I think all id-constructs have args
    size_t h = std::hash<std::string_view>()( std::string_view(tL->str.data()+iL) );
    h ^= c->constructCode();
    h ^= std::hash<std::string>()(tc->str);

    return h;
}

bool Selection::isTopLevel() const noexcept{
    return tL->isTopLevel();
}

bool Selection::isNested() const noexcept{
    return tL->isNested();
}

bool Selection::isTextSelection() const noexcept{
    return tL == tR;
}

bool Selection::isPhraseSelection() const noexcept{
    return left.phrase() == right.phrase();
}

Line* Selection::getStartLine() const noexcept{
    return tL->getLine();
}

std::vector<Selection> Selection::findCaseInsensitive(const std::string& str) const{
    assert(!str.empty());

    if(isTextSelection()) return findCaseInsensitiveText(str);
    else if(isPhraseSelection()) return findCaseInsensitivePhrase(str);
    else return findCaseInsensitiveLines(str);
}

size_t Selection::getConstructArgSize() const noexcept{
    assert(isPhraseSelection());
    assert(tR->id == tL->id+1);

    return tL->nextConstructAsserted()->numArgs();
}

void Selection::formatBasicIdentifier() const noexcept{
    formatSimple(SEM_ID);
}

void Selection::formatComment() const noexcept{
    formatSimple(SEM_COMMENT);
}

void Selection::formatKeyword() const noexcept{
    formatSimple(SEM_KEYWORD);
}

void Selection::formatNumber() const noexcept{
    tL->tag(SEM_NUMBER, iL, iR);
}

void Selection::formatMutableId() const noexcept{
    tL->tag(SEM_ID_FUN_IMPURE, iL, iR);
}

void Selection::formatString() const noexcept{
    formatSimple(SEM_STRING);
}

void Selection::format(SemanticType type) const noexcept{
    if(isTextSelection()) tL->tag(type, iL, iR);
    else if(isPhraseSelection()){
        tL->tag(type, iL, tL->size());
        Text* tc = tL->nextConstructAsserted()->frontTextAsserted();
        tc->tag(type, 0, tc->size());
    }
}

void Selection::formatSimple(SemanticType type) const noexcept{
    if(!tL->tags.empty() && tL->tags.back().index == iL)
        tL->tags.back().type = type;
    else
        tL->tags.push_back( SemanticTag(iL, type) );

    tL->tags.push_back( SemanticTag(iR, SEM_DEFAULT) );
}

#ifdef QT_CORE_LIB
QImage Selection::toPng() const{
    constexpr double UPSCALE = 4;

    std::array<double, 4> dims = getDimensions();
    QImage image(UPSCALE*QSize(dims[2], dims[3]), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter qpainter(&image);
    Painter painter(qpainter);
    painter.setZoom(UPSCALE);
    painter.setOffset(-dims[0], -dims[1]);

    paint(painter);

    return image;
}

void Selection::toSvg(QSvgGenerator& generator) const{
    constexpr double UPSCALE = 1;

    std::array<double, 4> dims = getDimensions();
    generator.setSize(UPSCALE*QSize(dims[2], dims[3]));
    generator.setViewBox(QRect(0, 0, UPSCALE*dims[2], UPSCALE*dims[3]));

    QPainter qpainter(&generator);
    qpainter.setRenderHint(QPainter::Antialiasing);
    Painter painter(qpainter);
    painter.setZoom(UPSCALE);
    painter.setOffset(-dims[0], -dims[1]);

    paint(painter);
}
#endif

Phrase* Selection::phrase() const noexcept{
    assert(isPhraseSelection());
    return left.phrase();
}

size_t Selection::characterSpan() const noexcept{
    assert(isTextSelection());
    return iR-iL;
}

std::string Selection::selectedTextSelection() const {
    return tR->substr(iL, characterSpan());
}

std::string Selection::selectedPhrase() const{
    assert(isPhraseSelection());
    size_t serial_chars = (tL->size()-iL) + iR + tL->nextConstructInPhrase()->serialChars();
    for(Text* t = tL->nextTextInPhrase(); t != tR; t = t->nextTextInPhrase())
        serial_chars += t->size() + t->nextConstructInPhrase()->serialChars();

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

std::string Selection::selectedLines() const{
    assert(!isPhraseSelection());

    size_t serial_chars = (tL->size()-iL) + iR + 1;
    for(Text* t = lL->back(); t != tL; t = t->prevTextAsserted())
        serial_chars += t->size() + t->prevConstructAsserted()->serialChars();
    for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted())
        serial_chars += l->serialChars() + 1;
    for(Text* t = lR->front(); t != tR; t = t->nextTextAsserted())
        serial_chars += t->size() + t->nextConstructAsserted()->serialChars();

    std::string out;
    out.resize(serial_chars);
    size_t curr = 0;

    for(Text* t = tL; t != lL->back(); t = t->nextTextAsserted()){
        t->writeString(out, curr, iL);
        t->nextConstructAsserted()->writeString(out, curr);
    }
    lL->back()->writeString(out, curr, iL);
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

std::vector<Selection> Selection::findCaseInsensitiveText(const std::string& target) const{
    std::vector<Selection> hits;

    const std::string& str = tL->str;
    size_t stop = iR-target.size();
    auto result = str.find(target, iL);
    while(result != std::string::npos && result <= stop){
        hits.push_back( Selection(tL, result, result+target.size()) );
        result = target.find(str, result+target.size());
    }

    return hits;
}

std::vector<Selection> Selection::findCaseInsensitivePhrase(const std::string& target) const{
    std::vector<Selection> hits;

    const std::string& strL = tL->str;
    auto result = strL.find(target, iL);
    while(result != std::string::npos){
        hits.push_back( Selection(tL, result, result+target.size()) );
        result = strL.find(target, result+target.size());
    }

    tL->nextConstructAsserted()->findCaseInsensitive(target, hits);
    for(size_t i = tL->id+1; i < tR->id; i++){
        phrase()->text(i)->findCaseInsensitive(target, hits);
        phrase()->construct(i)->findCaseInsensitive(target, hits);
    }

    const std::string& strR = tR->str;
    size_t stop = iR-target.size();
    result = strR.find(target);
    while(result != std::string::npos && result <= stop){
        hits.push_back( Selection(tR, result, result+target.size()) );
        result = target.find(strR, result+target.size());
    }

    return hits;
}

std::vector<Selection> Selection::findCaseInsensitiveLines(const std::string& target) const{
    std::vector<Selection> hits;

    const std::string& strL = tL->str;
    auto result = strL.find(target, iL);
    while(result != std::string::npos){
        hits.push_back( Selection(tL, result, result+target.size()) );
        result = strL.find(target, result+target.size());
    }

    for(size_t i = tL->id+1; i < lL->numTexts(); i++){
        lL->construct(i-1)->findCaseInsensitive(target, hits);
        lL->text(i)->findCaseInsensitive(target, hits);
    }

    const std::vector<Line*> lines = getLines();
    for(size_t i = lL->id+1; i < lR->id; i++)
        lines[i]->findCaseInsensitive(target, hits);

    for(size_t i = 0; i < tR->id; i++){
        lR->text(i)->findCaseInsensitive(target, hits);
        lR->construct(i)->findCaseInsensitive(target, hits);
    }

    const std::string& strR = tR->str;
    size_t stop = iR-target.size();
    result = strR.find(target);
    while(result != std::string::npos && result <= stop){
        hits.push_back( Selection(tR, result, result+target.size()) );
        result = target.find(strR, result+target.size());
    }

    return hits;
}

#ifndef HOPE_TYPESET_HEADLESS
std::array<double, 4> Selection::getDimensions() const noexcept{
    if(isTextSelection()) return getDimensionsText();
    else if(isPhraseSelection()) return getDimensionsPhrase();
    else return getDimensionsLines();
}

std::array<double, 4> Selection::getDimensionsText() const noexcept{
    double x = left.x();
    double y = tL->y;
    double h = tL->height();
    double w = right.x() - x;

    return {x, y, w, h};
}

std::array<double, 4> Selection::getDimensionsPhrase() const noexcept{
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

std::array<double, 4> Selection::getDimensionsLines() const noexcept{
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
bool Selection::inValidState() const{
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

#ifndef HOPE_TYPESET_HEADLESS
bool Selection::contains(double x, double y) const noexcept{
    if(isTextSelection()) return containsText(x, y);
    else if(isPhraseSelection()) return containsPhrase(x, y);
    else return containsLine(x, y);
}

bool Selection::containsText(double x, double y) const noexcept{
    return tR->containsY(y) && tR->containsXInBounds(x, iL, iR);
}

bool Selection::containsPhrase(double x, double y) const noexcept{
    return phrase()->containsY(y) && x >= xL && x <= xR;
}

bool Selection::containsLine(double x, double y) const noexcept{
    if((y < lL->y) | (y > lR->yBottom())) return false;

    Line* l = lL->nearestLine(y);

    if(l == lL) return x <= l->x + l->width + NEWLINE_EXTRA && x >= tL->xGlobal(iL);
    else if(l == lR) return x >= l->x && x <= tR->xGlobal(iR);
    else return (x >= l->x) & (x <= l->x + l->width + NEWLINE_EXTRA);
}

void Selection::paint(Painter& painter) const{
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

void Selection::paintSelectionText(Painter& painter, bool forward) const{
    if(iL!=iR){
        double y = tR->y;
        double h = tR->height();

        if(forward){
            double x = tR->xGlobal(iL);
            double w = tR->xGlobal(iR) - x;
            painter.drawSelection(x, y, w, h);
        }else{
            int x = tR->xGlobal(iL);
            int xEnd = tR->xGlobal(iR);
            painter.drawSelection(x, y, xEnd-x, h);
        }

        tR->paintMid(painter, iL, iR, forward);
    }
}

void Selection::paintSelectionPhrase(Painter& painter, bool forward) const{
    double y = phrase()->y;
    double h = phrase()->height();

    if(forward){
        double x = tL->xGlobal(iL);
        double w = tR->xGlobal(iR) - x;
        painter.drawSelection(x, y, w, h);
    }else{
        int x = tL->xGlobal(iL);
        int xEnd = tR->xGlobal(iR);
        painter.drawSelection(x, y, xEnd-x, h);
    }
    phrase()->paintMid(painter, tL, iL, tR, iR, forward);
}

void Selection::paintSelectionLines(Painter& painter, bool forward) const {
    double y = lL->y;
    double h = lL->height();

    if(forward){
        double x = tL->x + tL->xLocal(iL);
        double w = lL->width - tL->xPhrase(iL) + NEWLINE_EXTRA;
        painter.drawSelection(x, y, w, h);
    }else{
        int x = tL->x + tL->xLocal(iL);
        int xEnd = lL->x + lL->width + NEWLINE_EXTRA;
        painter.drawSelection(x, y, xEnd-x, h);
    }
    lL->paintAfter(painter, tL, iL, forward);

    for(Line* l = lL->nextAsserted(); l != lR; l = l->nextAsserted()){
        painter.drawSelection(l->x, l->y, l->width + NEWLINE_EXTRA, l->height());
        l->paint(painter, forward);
    }

    if(iR == 0 && tR == lR->front()) return;

    double x = lR->x;
    y = lR->y;
    double w = tR->xGlobal(iR) - x;
    h = lR->height();
    painter.drawSelection(x, y, w, h);
    lR->paintUntil(painter, tR, iR, forward);
}

void Selection::paintSelection(Painter& painter, bool forward) const{
    painter.setSelectionMode();

    if(isTextSelection()) paintSelectionText(painter, forward);
    else if(isPhraseSelection()) paintSelectionPhrase(painter, forward);
    else paintSelectionLines(painter, forward);
}

void Selection::paintError(Painter& painter) const{
    if(isTextSelection() && iL==iR) paintErrorSelectionless(painter);
    else if(isTextSelection()) paintErrorText(painter);
    else if(isPhraseSelection()) paintErrorPhrase(painter);
    else paintErrorLines(painter);
}

void Selection::paintErrorSelectionless(Painter& painter) const{
    painter.drawError(xR, tL->y, 6, tR->height());
}

void Selection::paintErrorText(Painter& painter) const{
    double x = tR->xGlobal(iL);
    double y = tR->y;
    double w = tR->xGlobal(iR) - x;
    double h = tR->height();

    painter.drawError(x, y, w, h);
}

void Selection::paintErrorPhrase(Painter &painter) const{
    double x = tL->xGlobal(iL);
    double y = phrase()->y;
    double w = tR->xGlobal(iR) - x;
    double h = phrase()->height();

    painter.drawError(x, y, w, h);
    phrase()->paintMid(painter, tL, iL, tR, iR);
}

void Selection::paintErrorLines(Painter& painter) const{
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

    double x_right = xR;
    x = x_right;
    y = lR->y;
    w = x_right - x;
    h = lR->height();

    painter.drawError(x, y, w, h);
    lR->paintUntil(painter, tR, iR);
}

void Selection::paintHighlight(Painter& painter) const{
    assert(!isTextSelection() || iL != iR);
    if(isTextSelection()) paintHighlightText(painter);
    else if(isPhraseSelection()) paintHighlightPhrase(painter);
    else assert(false);
}

void Selection::paintHighlightText(Painter& painter) const{
    double x = tR->xGlobal(iL);
    double y = tR->y;
    double w = tR->xGlobal(iR) - x;
    double h = tR->height();

    painter.drawHighlight(x, y, w, h);
}

void Selection::paintHighlightPhrase(Painter& painter) const{
    double x = tL->xGlobal(iL);
    double y = phrase()->y;
    double w = tR->xGlobal(iR) - x;
    double h = phrase()->height();

    painter.drawHighlight(x, y, w, h);
}
#endif

}

}
