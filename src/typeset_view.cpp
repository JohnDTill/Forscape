#include <typeset_view.h>

#include "forscape_program.h"
#include "forscape_serial.h"
#include "forscape_unicode.h"
#include "qt_compatability.h"
#include <typeset_command_pair.h>
#include <typeset_construct.h>
#include <typeset_integral_preference.h>
#include <typeset_keywords.h>
#include <typeset_line.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>
#include <typeset_settings.h>
#include <typeset_themes.h>

#include <chrono>

#include <QClipboard>
#include <QDrag>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define globalPosition globalPos
#endif

static std::chrono::time_point throttle_time =
        std::chrono::steady_clock::time_point{std::chrono::milliseconds{0}};

static constexpr auto THROTTLE_WINDOW = std::chrono::milliseconds(200);

#define THROTTLE(stmt) \
    auto time = std::chrono::steady_clock::now(); \
    if(time - throttle_time > THROTTLE_WINDOW){ \
        throttle_time = time; \
        stmt \
    }

namespace Forscape {

namespace Typeset {

static Code::ParseTree& parseTree() noexcept {
    return Program::instance()->parse_tree;
}

static Code::StaticPass& staticPass() noexcept {
    return Program::instance()->static_pass;
}

class View::VerticalScrollBar : public QScrollBar {
private:
    const View& view;
    bool vScrollActive() const noexcept {
        return singleStep() < maximum();
    }

    static constexpr int MAX_ERROR_HEIGHT = 10;
    static constexpr size_t V_PADDING = 15;
    static constexpr size_t H_PADDING = 6;

    int errorRegionPixelHeight() const noexcept {
        return height() - 2*V_PADDING;
    }

    int errorRegionPixelWidth() const noexcept {
        return width() - 2*H_PADDING;
    }

    static const Selection& getSelection(const Code::Error& err) noexcept {
        return err.selection;
    }

    static const Selection& getSelection(const Selection& sel) noexcept {
        return sel;
    }

    template<typename T>
    static void paintProportionally(
                QPainter& painter,
                const std::vector<T>& errors,
                const int error_region_height,
                const double model_height,
                const double w) alloc_except {
        //When the view is tall, we determine which rows to paint before painting
        static std::vector<bool> pixels; //GUI is single-threaded, so make this static
        if(error_region_height < 0) return;
        pixels.resize(error_region_height+V_PADDING);
        std::fill(pixels.begin(), pixels.end(), 0);

        const double scaling = error_region_height / model_height;
        for(const T& err : errors){
            const size_t pixel_start = getSelection(err).yTopPhrase() * scaling + V_PADDING;
            const size_t pixel_end = std::min(std::min(
                static_cast<size_t>(getSelection(err).yBotPhrase() * scaling) + V_PADDING,
                pixel_start+MAX_ERROR_HEIGHT), pixels.size()-1);
            for(size_t i = pixel_start; i <= pixel_end; i++) pixels[i] = true;
        }

        size_t start = 0;
        while(start < pixels.size()){
            if(pixels[start++]){
                size_t end = start;
                while(end < pixels.size() && pixels[end]) end++;
                painter.drawRect(H_PADDING, static_cast<int>(start), w, static_cast<int>(end-start));
                start = end+1;
            }
        }
    }

    void paintErrorsProportional(QPainter& painter) const {
        const Model& m = *view.getModel();
        const double model_height = m.height; //m.getHeight(); //EVENTUALLY: can make resizing smarter
        const int error_region_height = errorRegionPixelHeight();
        const int w = errorRegionPixelWidth();

        painter.setBrush(getColour(COLOUR_WARNINGBORDER));
        painter.setPen(getColour(COLOUR_WARNINGBORDER));
        paintProportionally(painter, m.warnings, error_region_height, model_height, w);

        painter.setBrush(getColour(COLOUR_ERRORBORDER));
        painter.setPen(getColour(COLOUR_ERRORBORDER));
        paintProportionally(painter, m.errors, error_region_height, model_height, w);

        painter.setBrush(getColour(COLOUR_HIGHLIGHTBORDER));
        painter.setPen(getColour(COLOUR_HIGHLIGHTBORDER));
        paintProportionally(painter, view.highlighted_words, error_region_height, model_height, w);
    }

    template<typename T>
    void paintAbsolute(QPainter& painter, const std::vector<T>& errors, const double w) const {
        for(const T& err : errors){
            int y = view.yScreen( getSelection(err).yTopPhrase() );
            int h = std::max(1, std::min<int>(MAX_ERROR_HEIGHT, view.yScreen(getSelection(err).yBotPhrase()) - y));
            painter.drawRect(H_PADDING, y, w, h);
        }
    }

    void paintErrorsAbsolute(QPainter& painter) const {
        const Model& m = *view.getModel();
        const int w = errorRegionPixelWidth();

        painter.setBrush(getColour(COLOUR_WARNINGBORDER));
        painter.setPen(getColour(COLOUR_WARNINGBORDER));
        paintAbsolute(painter, m.warnings, w);

        painter.setBrush(getColour(COLOUR_ERRORBORDER));
        painter.setPen(getColour(COLOUR_ERRORBORDER));
        paintAbsolute(painter, m.errors, w);

        painter.setBrush(getColour(COLOUR_HIGHLIGHTBORDER));
        painter.setPen(getColour(COLOUR_HIGHLIGHTBORDER));
        paintAbsolute(painter, view.highlighted_words, w);
    }

public:
    VerticalScrollBar(Qt::Orientation orientation, QWidget* parent, const View& view) noexcept
        : QScrollBar(orientation, parent), view(view){}

protected:
    void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE{
        QScrollBar::paintEvent(event);

        QPainter painter(this);
        if(vScrollActive()) paintErrorsProportional(painter);
        else paintErrorsAbsolute(painter);
    }
};

enum MouseHoldState{
    Hover,
    SingleClick,
    DoubleClick,
    TripleClick,
    ClickedOnSelection,
};
static MouseHoldState mouse_hold_state = Hover;

static constexpr auto TRIPLE_CLICK_WINDOW = std::chrono::milliseconds(200);

static std::chrono::time_point double_click_time =
        std::chrono::steady_clock::time_point{std::chrono::milliseconds{0}};

static bool recentlyDoubleClicked() noexcept {
    return std::chrono::steady_clock::now() - double_click_time < TRIPLE_CLICK_WINDOW;
}

static void invalidateDoubleClickTime() noexcept {
    double_click_time = std::chrono::steady_clock::time_point{std::chrono::milliseconds{0}};
}

static void updateDoubleClickTime() noexcept {
    double_click_time = std::chrono::steady_clock::now();
}

std::list<View*> View::all_views;

View::View() noexcept
    : model(Model::fromSerial("")),
      controller(model) {
    setFocusPolicy(Qt::ClickFocus);
    cursor_blink_timer = new QTimer(this);
    connect(cursor_blink_timer, &QTimer::timeout, this, QOverload<>::of(&View::onBlink));
    if(allow_write) cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
    else show_cursor = false;
    setMouseTracking(true);

    v_scroll = new VerticalScrollBar(Qt::Vertical, this, *this);
    h_scroll = new QScrollBar(Qt::Horizontal, this);
    connect(v_scroll, SIGNAL(valueChanged(int)), this, SLOT(update()));
    connect(h_scroll, SIGNAL(valueChanged(int)), this, SLOT(update()));

    v_scroll->setCursor(Qt::ArrowCursor);
    h_scroll->setCursor(Qt::ArrowCursor);

    updateBackgroundColour();
    setAutoFillBackground(true);
    all_views.push_back(this);
    search_selection = controller.selection();

    setAcceptDrops(true);
}

View::~View() noexcept {
    all_views.remove(this);
}

void View::setFromSerial(const std::string& src, bool is_output){
    model = Model::fromSerial(src, is_output);
    controller = Controller(model);
    h_scroll->setValue(h_scroll->minimum());
    updateXSetpoint();
    handleResize();
    updateHighlightingFromCursorLocation();
    update();

    emit textChanged();
}

std::string View::toSerial() const alloc_except {
    return model->toSerial();
}

Model* View::getModel() const noexcept {
    return model;
}

void View::setModel(Model* m) noexcept {
    hover_node = NONE;
    highlighted_words.clear();
    model = m;
    controller = Controller(model);
    updateXSetpoint();
    update();
    v_scroll->update();
    handleResize();
}

void View::setLineNumbersVisible(bool show) noexcept {
    if(show_line_nums == show) return;
    show_line_nums = show;
    update();
}

Controller& View::getController() noexcept {
    return controller;
}

void View::setReadOnly(bool read_only) noexcept {
    if(allow_write == !read_only) return;
    allow_write = !read_only;

    if(read_only) stopCursorBlink();
    else restartCursorBlink();
}

void View::replaceAll(const std::vector<Selection>& targets, const std::string& name) alloc_except {
    model->rename(targets, name, controller);

    restartCursorBlink();
    ensureCursorVisible();
    update();
}

void View::dispatchClick(double x, double y, int xScreen, int yScreen, bool right_click, bool shift_held) alloc_except {
    if(right_click){
        resolveRightClick(x, y, xScreen, yScreen);
    }else if(recentlyDoubleClicked() || isInLineBox(x)){
        mouse_hold_state = TripleClick;
        resolveTripleClick(y);
    }else if(shift_held){
        resolveShiftClick(x, y);
    }else if(ALLOW_SELECTION_DRAG && controller.contains(x, y)){
        mouse_hold_state = ClickedOnSelection;
    }else{
        mouse_hold_state = SingleClick;
        resolveClick(x, y);
    }

    restartCursorBlink();
    updateHighlightingFromCursorLocation();
    update();
}

void View::dispatchRelease(int x, int y){
    if(mouse_hold_state == ClickedOnSelection)
        resolveClick(xModel(x), yModel(y));

    mouse_hold_state = Hover;

    update();
}

void View::dispatchDoubleClick(double x, double y){
    mouse_hold_state = DoubleClick;
    updateDoubleClickTime();

    if(!isInLineBox(x)){
        //EVENTUALLY: do something better than this hack to avoid highlight when double clicking link
        if(controller.atTextEnd())
            if(Construct* c = controller.anchor.text->nextConstructInPhrase())
                if(c->constructCode() == MARKERLINK)
                    return;

        resolveWordClick(x, y);
        update();
    }
}

void View::dispatchHover(double x, double y){
    setCursorAppearance(x, y);

    switch(mouse_hold_state){
        case Hover: resolveTooltip(x, y); break;
        case SingleClick:
            resolveShiftClick(x, y); update(); break;
        case DoubleClick: resolveWordDrag(x, y); update(); break;
        case TripleClick: resolveLineDrag(y); update(); break;
        case ClickedOnSelection:{
            mouse_hold_state = Hover;

            Qt::DropActions actions = Qt::CopyAction;
            if(allow_write) actions |= Qt::MoveAction;

            QDrag* drag = new QDrag(this);
            QMimeData* mimeData = new QMimeData;

            mimeData->setText(toQString(controller.selectedText()));
            drag->setMimeData(mimeData);
            drag->setPixmap(QPixmap(1, 1));
            drag->exec(actions);
            break;
        }
        default: break;
    }
}

void View::dispatchMousewheel(bool ctrl_held, bool up){
    if( ctrl_held ){
        if(up) zoomIn();
        else zoomOut();

        handleResize();
    }else{
        if(up) scrollUp();
        else scrollDown();
    }

    update();
}

void View::resolveClick(double x, double y) noexcept {
    Line* l = model->nearestLine(y);
    controller.clickTo(l, x, y);
    updateXSetpoint();

    model->updateLayout();
    v_scroll->update();

    emit textChanged();
}

void View::resolveShiftClick(double x, double y) noexcept {
    Phrase* p = controller.isTopLevel() ? model->nearestLine(y) : controller.phrase();
    controller.shiftClick(p, x);
    updateXSetpoint();
    ensureCursorVisible();
}

void View::resolveRightClick(double x, double y, int xScreen, int yScreen){
    QMenu menu;

    bool clicked_on_selection = controller.contains(x,y);

    if(!clicked_on_selection) resolveClick(x, y);

    #define append(str, cmd, enabled, visible) \
        if(visible){ \
        QAction* action = menu.addAction(str); \
        connect(action, SIGNAL(triggered()), this, SLOT(cmd())); \
        action->setEnabled(enabled); \
        }

    if(Construct* con = model->constructAt(x, y)){
        Subphrase* child = con->argAt(x, y);

        const std::vector<Construct::ContextAction>& actions = con->getContextActions(child);
        for(size_t i = 0; i < actions.size(); i++){
            const Construct::ContextAction& action = actions[i];
            QAction* q_action = menu.addAction(QString::fromStdString(action.name));

            connect(
                q_action,
                &QAction::triggered,
                [=]() { action.takeAction(con, controller, child); emit textChanged(); }
            );

            q_action->setEnabled( action.enabled );
        }

        switch (con->constructCode()) {
            case INTEGRAL1:
            case INTEGRAL2:
            case INTEGRALCONV1:
            case INTEGRALCONV2:
            case DOUBLEINTEGRAL1:
            case DOUBLEINTEGRALCONV1:
            case TRIPLEINTEGRAL1:
            case TRIPLEINTEGRALCONV1:
                connect(
                    menu.addAction(
                        integral_bounds_vertical ? "Show integral scripts to side" : "Show integral scripts vertically"),
                    &QAction::triggered,
                    this,
                    &Editor::changeIntegralPreference);
                break;
            default: break;
        }

        menu.addSeparator();
    }

    populateContextMenuFromModel(menu, x, y);

    menu.addSeparator();

    append("Undo", undo, model->undoAvailable(), allow_write)
    append("Redo", redo, model->redoAvailable(), allow_write)
    menu.addSeparator();
    append("Cut", cut, clicked_on_selection, allow_write)
    append("Copy", copy, clicked_on_selection, true)
    append("Paste", paste, true, allow_write)
    menu.addSeparator();
    append("Select All", selectAll, true, true)

    #ifndef TEST_QT
    menu.exec(QPoint(xScreen, yScreen));
    #endif
}

void View::resolveWordClick(double x, double y){
    if(Construct* c = model->constructAt(x, y)){
        c->onDoubleClick(controller, x - c->x, y - c->y);
        model->updateLayout();
        v_scroll->update();
        emit textChanged();
    }else{
        resolveClick(x, y);
        controller.moveToPrevWord();
        controller.selectNextWord();
    }

    restartCursorBlink();
    updateXSetpoint();
}

void View::resolveWordDrag(double x, double y) noexcept {
    bool forward = controller.isForward();

    resolveShiftClick(x, y);
    if(controller.isForward()){
        if(!forward){
            controller.consolidateToAnchor();
            controller.moveToPrevWord();
            resolveShiftClick(x, y);
        }
        if(controller.notAtTextEnd() ||
           controller.active.text != controller.phrase()->back())
        controller.selectNextWord();
    }else{
        if(forward){
            controller.consolidateToAnchor();
            controller.moveToNextWord();
            resolveShiftClick(x, y);
        }
        if(controller.notAtTextStart() || controller.active.text != controller.phrase()->front())
            controller.selectPrevWord();
    }

    updateXSetpoint();
}

void View::resolveTripleClick(double y) noexcept {
    Line* l = model->nearestLine(y);
    controller.selectLine(l);
    updateXSetpoint();

    invalidateDoubleClickTime();
}

void View::resolveLineDrag(double y) noexcept {
    Line* active_line = model->nearestLine(y);
    Line* anchor_line = controller.anchorLine();

    if(active_line->id >= anchor_line->id){
        controller.anchor.setToFrontOf(anchor_line);
        if(Line* l = active_line->next()) controller.active.setToFrontOf(l);
        else controller.active.setToBackOf(active_line);
    }else{
        controller.anchor.setToBackOf(anchor_line);
        controller.active.setToFrontOf(active_line);
    }

    ensureCursorVisible();
    updateXSetpoint();
}

void View::resolveTooltip(double, double) noexcept {
    //DO NOTHING
}

void View::resolveSelectionDrag(double x, double y){
    mouse_hold_state = Hover;

    if(controller.contains(x, y)) return;

    std::string str = controller.selectedText();
    Command* a = controller.deleteSelection();
    bool delete_first = (x <= controller.xActive());
    if(!controller.isPhraseSelection()){
        Line* line_bot;
        Line* line_top;
        if(controller.isForward()){
            line_top = controller.anchorLine();
            line_bot = controller.activeLine();
        }else{
            line_top = controller.activeLine();
            line_bot = controller.anchorLine();
        }
        if(y < line_top->y){
            delete_first = false;
        }else if(y > line_bot->yBottom()){
            delete_first = true;
        }else if (y < line_top->yBottom() && x <= controller.xActive()){
            delete_first = true;
        }else if (y >= line_bot->y && x > controller.xActive()){
            delete_first = true;
        }else{
            return;
        }
    }
    if(delete_first) a->redo(controller);
    Line* l = model->nearestLine(y);
    controller.clickTo(l, x, y);
    controller.consolidateLeft();
    Command* b = controller.insertSerialNoSelection(str);
    Command* cmd = delete_first ? new CommandPair(a,b) : new CommandPair(b,a);
    if(delete_first) a->undo(controller);
    model->mutate(cmd, controller);

    emit textChanged();
}

bool View::isInLineBox(double x) const noexcept {
    return x-h_scroll->value()/zoom < -MARGIN_LEFT;
}

void View::updateXSetpoint() noexcept {
    x_setpoint = controller.xActive();
}

double View::xModel(double xScreen) const noexcept {
    return xScreen/zoom - xOrigin();
}

double View::yModel(double yScreen) const noexcept {
    return yScreen/zoom - yOrigin();
}

double View::xScreen(double xModel) const noexcept {
    return zoom*(xModel + xOrigin());
}

double View::yScreen(double yModel) const noexcept {
    return zoom*(yModel + yOrigin());
}

void View::zoomIn() noexcept {
    zoom = std::min(ZOOM_MAX, zoom*ZOOM_DELTA);
}

void View::zoomOut() noexcept {
    zoom = std::max(ZOOM_MIN, zoom/ZOOM_DELTA);
}

void View::resetZoom() noexcept {
    zoom = ZOOM_DEFAULT;
}

void View::restartCursorBlink() noexcept {
    if(!allow_write) return;
    show_cursor = true;
    cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
}

void View::stopCursorBlink() noexcept {
    show_cursor = false;
    update();
    cursor_blink_timer->stop();
}

void View::updateHighlightingFromCursorLocation(){
    highlighted_words.clear();

    const std::pair<ParseNode, ParseNode> parse_nodes = controller.active.parseNodesAround();
    if(parse_nodes.second != NONE)
        populateHighlightWordsFromParseNode(parse_nodes.second);
    if(highlighted_words.empty() && parse_nodes.first != NONE)
        populateHighlightWordsFromParseNode(parse_nodes.first);

    update();
    updateAfterHighlightChange();
}

void View::populateHighlightWordsFromParseNode(ParseNode pn){
    const auto& parse_tree = model->parser.parse_tree;
    if(parse_tree.getOp(pn) == Code::OP_FILE_REF){
        ParseNode id = parse_tree.getCols(pn);
        if(id == 0) return;
        pn = id;
    }
    else if(parse_tree.getOp(pn) != Code::OP_IDENTIFIER) return;

    const Code::Symbol* const sym = parse_tree.getSymbol(pn);
    if(sym) sym->getModelOccurences(highlighted_words, getModel());
}

bool View::scrolledToBottom() const noexcept {
    return v_scroll->value() == v_scroll->maximum();
}

void View::scrollToBottom(){
    v_scroll->setValue(v_scroll->maximum());
    update();
}

bool View::lineNumbersShown() const noexcept {
    return show_line_nums;
}

void View::insertText(const std::string& str){
    controller.insertText(str);
}

void View::insertSerial(const std::string& str){
    controller.insertSerial(str);
}

size_t View::numLines() const noexcept {
    return model->numLines();
}

size_t View::currentLine() const noexcept {
    return controller.activeLine()->id;
}

void View::ensureCursorVisible() noexcept {
    handleResize();

    double xModel = controller.xActive();
    double yTopModel = controller.active.text->y;
    double yBotModel = yTopModel + controller.active.text->height();

    double xScreen = (xModel+xOrigin())*zoom;
    double yTopScreen = (yTopModel+yOrigin())*zoom;
    double yBotScreen = (yBotModel+yOrigin())*zoom;

    double translate_x = 0;
    if(xScreen < LINEBOX_WIDTH*zoom + MARGIN_LEFT + CURSOR_MARGIN){
        translate_x = xScreen - (LINEBOX_WIDTH*zoom + MARGIN_LEFT + CURSOR_MARGIN);
    }else if(xScreen > width() - v_scroll->width() - CURSOR_MARGIN){
        translate_x = xScreen - (width() - v_scroll->width() - CURSOR_MARGIN);
    }

    double translate_y = 0;
    if(yTopScreen < CURSOR_MARGIN){
        translate_y = yTopScreen - CURSOR_MARGIN;
    }else if(yBotScreen > height() - CURSOR_MARGIN){
        translate_y = yBotScreen - (height() - CURSOR_MARGIN);
    }

    v_scroll->setValue(v_scroll->value() + translate_y);
    h_scroll->setValue(h_scroll->value() + translate_x);
}

void View::updateModel() noexcept {
    model->updateLayout();
    update();
}

void View::goToLine(size_t line_num){
    if(line_num > model->lines.size()) return;
    controller.setBothToBackOf(model->lines[line_num]->back());
    ensureCursorVisible();
    restartCursorBlink();
    update();
}

void View::updateBackgroundColour() noexcept {
    if(dynamic_cast<LineEdit*>(this)) return; //EVENTUALLY: redesign themes to avoid this dumb hack
    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, getColour(COLOUR_BACKGROUND));
    setPalette(pal);
    repaint();
}

void View::updateAfterHighlightChange() noexcept {
    update();
    v_scroll->update();
}

double View::xOrigin() const noexcept {
    return getLineboxWidth() + MARGIN_LEFT - h_scroll->value()/zoom;
}

double View::yOrigin() const noexcept {
    return MARGIN_TOP - v_scroll->value()/zoom;
}

double View::getLineboxWidth() const noexcept {
    return show_line_nums*LINEBOX_WIDTH;
}

void View::keyPressEvent(QKeyEvent* e){
    handleKey(e->key(), e->modifiers(), toCppString(e->text()));
}

void View::mousePressEvent(QMouseEvent* e){
    if(focusWidget() != this) return;

    auto pos = e->pos();
    double click_x = xModel(pos.x());
    double click_y = yModel(pos.y());
    bool right_click = e->buttons() == Qt::RightButton;
    bool shift_held = e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier);

    auto global_pos = e->globalPosition();
    dispatchClick(click_x, click_y, global_pos.x(), global_pos.y(), right_click, shift_held);
}

void View::mouseDoubleClickEvent(QMouseEvent* e){
    dispatchDoubleClick(xModel(e->x()), yModel(e->y()));
}

void View::mouseReleaseEvent(QMouseEvent* e){
    dispatchRelease(e->x(), e->y());
}

void View::mouseMoveEvent(QMouseEvent* e){
    dispatchHover(xModel(e->x()), yModel(e->y()));
}

void View::wheelEvent(QWheelEvent* e){
    bool ctrl_held = e->modifiers() == Qt::ControlModifier;
    bool up = e->angleDelta().y() > 0;

    dispatchMousewheel(ctrl_held, up);
}

void View::focusInEvent(QFocusEvent*){
    restartCursorBlink();
}

void View::focusOutEvent(QFocusEvent* e){
    if(e->reason() == Qt::TabFocusReason){
        setFocus(Qt::TabFocusReason);
        if(allow_write) controller.tab();
        ensureCursorVisible();
        updateHighlightingFromCursorLocation();
        update();
    }else if(e->reason() == Qt::BacktabFocusReason){
        setFocus(Qt::BacktabFocusReason);
        if(allow_write) controller.detab();
        ensureCursorVisible();
        updateHighlightingFromCursorLocation();
        update();
    }else if(!mock_focus){
        stopCursorBlink();
        mouse_hold_state = Hover;
        //EVENTUALLY: why was this here?
        //assert(mouse_hold_state == Hover);
    }
}

void View::resizeEvent(QResizeEvent* e){
    QWidget::resizeEvent(e);
    handleResize();
}

void View::dragEnterEvent(QDragEnterEvent* event) {
    if(!allow_write) return;

    if(event->mimeData()->hasText()){
        event->setAccepted(true);
        event->setDropAction(event->source() == this ? Qt::DropAction::MoveAction : Qt::DropAction::CopyAction);
    }
}

void View::dragLeaveEvent(QDragLeaveEvent* event) {
    event->accept();
}

void View::dropEvent(QDropEvent* event) {
    event->acceptProposedAction();

    if(!rect().contains(event->pos())) return;

    const QPoint& p = event->pos();
    const double x = xModel(p.x());
    const double y = yModel(p.y());

    if(event->source() == this){
        resolveSelectionDrag(x, y);
        update();
    }else{
        const QMimeData* data = event->mimeData();
        assert(data->hasText());
        resolveClick(x, y);
        controller.deselect();
        paste(toCppString(data->text()));
        activateWindow();
    }
}

void View::dragMoveEvent(QDragMoveEvent* event) {
    const QPoint& p = event->pos();
    const bool can_drop = p.x() >= getLineboxWidth()*zoom && p.x() < width();
    event->setAccepted(can_drop);
}

void View::onBlink() noexcept {
    show_cursor = !show_cursor && (hasFocus() || mock_focus);
    update();
    cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
}

void View::copy() const {
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(toQString(str));
}

void View::cut(){
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(toQString(str));
    controller.del();

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paste(){
    std::string str = toCppString(QGuiApplication::clipboard()->text());

    if(isIllFormedUtf8(str)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Clipboard content is not UTF-8 encoded.");
        messageBox.setFixedSize(500,200);
        return;
    }

    if(!Forscape::isValidSerial(str)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Clipboard content does not conform to Forscape encoding.");
        messageBox.setFixedSize(500,200);
        return;
    }

    paste(str);
}

void View::del(){
    controller.del();

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::clearModel() noexcept {
    model->clear();
    controller = Controller(model);
    updateModel();
}

void View::comment() {
    if(allow_write){
        controller.comment();
        updateXSetpoint();
        restartCursorBlink();
    }
}

void View::setCursorAppearance(double x, double y) {
    Construct* con = model->constructAt(x, y);
    if(con && con->constructCode() == MARKERLINK){
        QWidget::setCursor(Qt::PointingHandCursor);
    }else{
        QWidget::setCursor(isInLineBox(x) ? Qt::ArrowCursor : Qt::IBeamCursor);
    }
}

void View::drawModel(double xL, double yT, double xR, double yB) {
    model->updateLayout();

    Painter painter(qpainter, xL, yT, xR, yB);
    painter.setZoom(zoom);
    painter.setOffset(xOrigin(), yOrigin());

    //EVENTUALLY: get rid of this hack
    QColor error_background = getColour(COLOUR_ERRORBACKGROUND);
    QColor error_border = getColour(COLOUR_ERRORBORDER);

    setColour(COLOUR_ERRORBACKGROUND, getColour(COLOUR_BACKGROUND));
    setColour(COLOUR_ERRORBORDER, getColour(COLOUR_LINK));

    if(!search_selection.isEmpty())
        if(search_selection.overlapsY(yT, yB))
            search_selection.paintError(painter);

    //EVENTUALLY: get rid of this hack
    setColour(COLOUR_ERRORBACKGROUND, getColour(COLOUR_WARNINGBACKGROUND));
    setColour(COLOUR_ERRORBORDER, getColour(COLOUR_WARNINGBORDER));

    for(const Code::Error& e : model->warnings)
        if(e.selection.getModel() == model && e.selection.overlapsY(yT, yB))
            e.selection.paintError(painter);

    setColour(COLOUR_ERRORBACKGROUND, error_background);
    setColour(COLOUR_ERRORBORDER, error_border);

    for(const Code::Error& e : model->errors)
        if(e.selection.getModel() == model && e.selection.overlapsY(yT, yB))
            e.selection.paintError(painter);

    for(const Selection& c : highlighted_words)
        if(c.getModel() == model && c.overlapsY(yT, yB))
            c.paintHighlight(painter);

    #ifndef NDEBUG
    if(model_undo_stack_size != model->undo_stack.size()){
        model_undo_stack_size = model->undo_stack.size();
        hover_node = NONE;
    }else if(hover_node != NONE){
        const Typeset::Selection& sel = model->parser.parse_tree.getSelection(hover_node);
        sel.paintHighlight(painter);
    }
    #endif

    const Typeset::Marker& cursor = getController().active;

    model->paint(painter, xL, yT, xR, yB);
    if(allow_write) model->paintGroupings(painter, cursor);
    controller.paintSelection(painter, xL, yT, xR, yB);
    if(display_commas_in_numbers) model->paintNumberCommas(painter, xL, yT, xR, yB, controller.selection());
    if(show_cursor){
        if(insert_mode) controller.paintInsertCursor(painter, xL, yT, xR, yB);
        else controller.paintCursor(painter);
    }
}

void View::drawLinebox(double yT, double yB) {
    if(!show_line_nums) return;

    qpainter.resetTransform();
    qpainter.setBrush(getColour(COLOUR_LINEBOXFILL));
    qpainter.setPen(getColour(COLOUR_LINEBOXBORDER));
    qpainter.drawRect(-1, -1, 1+LINEBOX_WIDTH*zoom, height()+2);

    Painter painter(qpainter, 0, yT, std::numeric_limits<double>::max(), yB);
    painter.setZoom(zoom);
    painter.setOffset(LINEBOX_WIDTH - LINE_NUM_OFFSET, yOrigin());

    size_t iL;
    size_t iR;
    if(controller.isForward()){
        iL = controller.anchor.text->getLine()->id;
        Line* lR = controller.active.text->getLine();
        iR = lR->id - controller.atLineStart();
    }else{
        iL = controller.active.text->getLine()->id;
        iR = controller.anchor.text->getLine()->id;
    }

    Line* start = model->nearestLine(yT);
    Line* end = model->nearestLine(yB);

    for(size_t i = start->id; i <= end->id; i++){
        Line*& l = model->lines[i];
        size_t n = l->id+1;
        bool active = (l->id >= iL) & (l->id <= iR);
        painter.drawLineNumber(l->front()->y, n, active);
    }
}

void View::fillInScrollbarCorner(){
    if(!v_scroll->isVisible()) return; //EVENTUALLY: specialise the scrollbars

    int x = h_scroll->width();
    int y = v_scroll->height();
    int w = v_scroll->width();
    int h = h_scroll->height();

    QBrush brush = QGuiApplication::palette().window();
    qpainter.resetTransform();
    qpainter.fillRect(x, y, w, h, brush);
}

void View::handleResize(){
    QSize sze = size();
    handleResize(sze.width(), sze.height());
}

void View::handleResize(int w, int h){
    int display_width = w - v_scroll->width();
    int model_display_width = display_width - (getLineboxWidth() + MARGIN_LEFT + MARGIN_RIGHT)*zoom;
    v_scroll->move(display_width, 0);
    h_scroll->setFixedWidth(display_width);

    double model_width = model->getWidth()*zoom;
    double model_height = (model->getHeight() + MARGIN_TOP + MARGIN_BOT)*zoom;

    double w_diff_scrn = model_width - model_display_width;
    bool h_scroll_enabled = w_diff_scrn > 0;
    h_scroll->setVisible(h_scroll_enabled);
    int display_height = h - h_scroll_enabled*h_scroll->height();

    if(h_scroll_enabled){
        h_scroll->setPageStep(model_display_width);
        h_scroll->setMaximum(w_diff_scrn);
        h_scroll->move(0, display_height);
    }

    v_scroll->setFixedHeight(display_height);
    double h_diff_scrn = model_height - display_height;
    bool v_scroll_enabled = h_diff_scrn > 0;

    if(v_scroll_enabled){
        v_scroll->setPageStep(display_height);
        v_scroll->setMaximum(h_diff_scrn);
    }else{
        v_scroll->setMaximum(0);
    }
}

void View::scrollUp(){
    v_scroll->setValue( v_scroll->value() - v_scroll->pageStep()/10 );
}

void View::scrollDown(){
    v_scroll->setValue( v_scroll->value() + v_scroll->pageStep()/10 );
}

void View::undo(){
    model->undo(controller);
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();

    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::redo(){
    model->redo(controller);
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::selectAll() noexcept {
    controller.selectAll();
}

void View::changeIntegralPreference() noexcept {
    emit integralPreferenceChanged(!integral_bounds_vertical);
}

void View::handleKey(int key, int modifiers, const std::string& str){
    constexpr int Ctrl = Qt::ControlModifier;
    constexpr int Shift = Qt::ShiftModifier;
    constexpr int CtrlShift = Qt::ControlModifier | Qt::ShiftModifier;
    constexpr int Alt = Qt::AltModifier;

    switch (key | modifiers) {
        case Qt::Key_Z|Ctrl: if(allow_write) model->undo(controller); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Y|Ctrl: if(allow_write) model->redo(controller); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Right: controller.moveToNextChar(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left: controller.moveToPrevChar(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Right|Ctrl: controller.moveToNextWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left|Ctrl: controller.moveToPrevWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Down: controller.moveToNextLine(x_setpoint); restartCursorBlink(); update(); break;
        case Qt::Key_Up: controller.moveToPrevLine(x_setpoint); restartCursorBlink(); update(); break;
        case Qt::Key_Home: controller.moveToStartOfLine(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_End: controller.moveToEndOfLine(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_PageDown: controller.moveToNextPage(x_setpoint, height()/zoom); restartCursorBlink(); break;
        case Qt::Key_PageUp: controller.moveToPrevPage(x_setpoint, height()/zoom); restartCursorBlink(); break;
        case Qt::Key_Home|Ctrl: controller.moveToStartOfDocument(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_End|Ctrl: controller.moveToEndOfDocument(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Right|Shift: controller.selectNextChar(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left|Shift: controller.selectPrevChar(); updateXSetpoint(); restartCursorBlink(); update(); update(); break;
        case Qt::Key_Right|CtrlShift: controller.selectNextWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left|CtrlShift: controller.selectPrevWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Down|Shift: controller.selectNextLine(x_setpoint); restartCursorBlink(); update(); break;
        case Qt::Key_Up|Shift: controller.selectPrevLine(x_setpoint); restartCursorBlink(); update(); break;
        case Qt::Key_Home|Shift: controller.selectStartOfLine(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_End|Shift: controller.selectEndOfLine(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Home|CtrlShift: controller.selectStartOfDocument(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_End|CtrlShift: controller.selectEndOfDocument(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_PageDown|Qt::ShiftModifier: controller.selectNextPage(x_setpoint, height()/zoom); restartCursorBlink(); break;
        case Qt::Key_PageUp|Qt::ShiftModifier: controller.selectPrevPage(x_setpoint, height()/zoom); restartCursorBlink(); break;
        case Qt::Key_A|Ctrl: controller.selectAll(); update(); break;
        case 61|Qt::ControlModifier: zoomIn(); restartCursorBlink(); break;
        case Qt::Key_Minus|Qt::ControlModifier: zoomOut(); restartCursorBlink(); break;
        case Qt::Key_Insert: insert_mode = !insert_mode; break;
        case Qt::Key_Slash|Ctrl: comment(); break;
        case Qt::Key_Delete:
        case Qt::Key_Delete|Shift:
        if(allow_write){
            controller.del();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Delete|Ctrl:
        case Qt::Key_Delete|Ctrl|Shift:
        if(allow_write){
            controller.delWord();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Backspace:
        case Qt::Key_Backspace|Shift:
        if(allow_write){
            controller.backspace();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Backspace|Ctrl:
        case Qt::Key_Backspace|Ctrl|Shift:
        if(allow_write){
            controller.backspaceWord();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Return:
            if(focusWidget() != this || !allow_new_line) return;
            if(allow_write) controller.newline(); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Return|Shift:
            if(focusWidget() != this || !allow_new_line) return;
            if(allow_write) controller.newline(); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Tab: if(allow_write) controller.tab(); break;
        #ifdef __EMSCRIPTEN__
        case Qt::Key_Tab|Shift: if(allow_write) controller.detab(); break;
        #else
        case (Qt::Key_Tab|Shift)+1: if(allow_write) controller.detab(); break; //+1 offset glitch
        #endif
        case Qt::Key_C|Ctrl: copy(); break;
            #ifndef __EMSCRIPTEN__
        case Qt::Key_X|Ctrl: if(allow_write) cut(); updateXSetpoint(); restartCursorBlink(); break;
            #endif
        case Qt::Key_V|Ctrl: if(allow_write) paste(); updateXSetpoint(); restartCursorBlink(); break;
        default:
            bool alt_or_ctrl_pressed = (Alt|Ctrl) & modifiers;
            if(!allow_write || alt_or_ctrl_pressed || str.empty()) return;

            if(insert_mode) controller.selectNextChar();
            std::string_view typeset_cmd = controller.keystroke(str);
            updateXSetpoint();
            restartCursorBlink();

            if(typeset_cmd.empty()) recommend();
            else recommendTypeset(typeset_cmd.substr(1));
    }

    model->updateLayout();
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paste(const std::string& str){
    assert(isValidSerial(str));

    model->mutate(controller.getInsertSerial(str), controller);

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paintEvent(QPaintEvent* event){
    QWidget::paintEvent(event);

    qpainter.begin(this);
    qpainter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

    #ifdef _MSC_VER
    QRect r = event->rect();
    double xL = xModel(r.x());
    double yT = yModel(r.y());
    double xR = xModel(r.x() + r.width());
    double yB = yModel(r.y() + r.height());
    #else
    //EVENTUALLY: why doesn't event->rect() work on linux?
    double xL = xModel(0);
    double yT = yModel(0);
    double xR = xModel(width());
    double yB = yModel(height());
    #endif


    drawModel(xL, yT, xR, yB);
    drawLinebox(yT, yB);
    fillInScrollbarCorner();

    qpainter.end();
}

QImage View::toPng() const {
    return controller.selection().toPng();
}

void Console::paintEvent(QPaintEvent* event){
    View::paintEvent(event);
    QFrame::paintEvent(event);
}

LineEdit::LineEdit() : View() {
    allow_new_line = false;
    setLineNumbersVisible(false);
    v_scroll->hide();
    h_scroll->hide();
    v_scroll->setDisabled(true);
    h_scroll->setDisabled(true);
    model->is_output = true;

    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    zoom = 1;

    connect(this, SIGNAL(textChanged()), this, SLOT(fitToContentsVertically()));
    v_scroll->setVisible(false);
    h_scroll->setVisible(false);

    //EVENTUALLY: redesign the theme to avoid this dumb hack
    std::array<QColor, NUM_COLOUR_ROLES> backup = getColours();
    setPreset(PRESET_DEFAULT);

    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, getColour(COLOUR_BACKGROUND));
    setPalette(pal);
    setColours(backup);
}

void LineEdit::paintEvent(QPaintEvent* event){
    //EVENTUALLY: redesign the theme to avoid this dumb hack
    std::array<QColor, NUM_COLOUR_ROLES> backup = getColours();
    setPreset(PRESET_DEFAULT);

    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, getColour(COLOUR_BACKGROUND));
    setPalette(pal);

    v_scroll->setVisible(false);
    h_scroll->setVisible(false);

    View::paintEvent(event);
    QFrame::paintEvent(event);

    setColours(backup);
}

void LineEdit::wheelEvent(QWheelEvent* e){
    QWidget::wheelEvent(e);
}

void Forscape::Typeset::LineEdit::fitToContentsVertically() noexcept {
    QWidget::setFixedHeight(yScreen(model->getHeight()) + MARGIN_BOT);
    v_scroll->setVisible(false);
    h_scroll->setVisible(false);
    v_scroll->setValue(0);
}

Label::Label() : View() {
    setLineNumbersVisible(false);
    setReadOnly(true);
    v_scroll->setVisible(false);
    h_scroll->setVisible(false);
    model->is_output = true;

    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    zoomOut();
    zoomOut();
}

void Label::clear() noexcept {
    model->clear();

    //EVENTUALLY: Label shouldn't have a cursor
    controller.setBothToFrontOf(model->firstText());
}

void Label::appendSerial(const std::string& src){
    controller.moveToEndOfDocument();
    controller.insertSerial(src);
}

void Label::appendSerial(const std::string& src, SemanticType type) {
    Text* t = model->lastText();
    size_t index = t->numChars();
    appendSerial(src);
    t->tags.push_back(SemanticTag(index, type));
    model->lastText()->tagBack(SEM_DEFAULT);
}

void Label::fitToContents() noexcept {
    //EVENTUALLY: this spams logs, and you need to fix the class hierarchy so that the label
    // doesn't have scrollbars
    #ifndef TEST_QT
    QWidget::resize(
        xScreen(model->getWidth()) + MARGIN_RIGHT,
        yScreen(model->getHeight()) + MARGIN_BOT
    );
    #endif

    v_scroll->setVisible(false);
    h_scroll->setVisible(false);
}

bool Label::empty() const noexcept {
    return model->empty();
}

void Label::paintEvent(QPaintEvent* event){
    v_scroll->setVisible(false);
    h_scroll->setVisible(false);

    auto bg = getColour(COLOUR_BACKGROUND);
    setColour(COLOUR_BACKGROUND, palette().color(QPalette::ToolTipBase));
    View::paintEvent(event);
    setColour(COLOUR_BACKGROUND, bg);
    QFrame::paintEvent(event);
}

Recommender::Recommender() : View() {
    setLineNumbersVisible(false);
    setReadOnly(true);
    h_scroll->setVisible(false);
    model->is_output = true;

    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setWindowFlags(Qt::Popup);
}

void Recommender::clear() noexcept {
    model->clear();
    controller.setBothToFrontOf(model->firstText());
}

void Recommender::moveDown() noexcept {
    if(controller.atEnd()) controller.moveToStartOfDocument();
    else controller.moveToNextLine(0);

    controller.selectEndOfLine();
    ensureCursorVisible();
    update();
}

void Recommender::moveUp() noexcept {
    if(controller.getAnchor().getLine()->id > 0) controller.moveToPrevLine(0);
    else controller.moveToEndOfDocument();

    controller.moveToStartOfLine();
    controller.selectEndOfLine();
    ensureCursorVisible();
    update();
}

void Recommender::take() noexcept {
    if(recommend_typeset_phrase_size){
        std::string str = controller.selectedText();
        auto lookup = Keywords::lookup(str);
        editor->getController().anchor.index -= recommend_typeset_phrase_size;
        editor->insertSerial(lookup);
        editor->updateModel();
        hide();
    }else{
        editor->takeRecommendation(controller.selectedText());
    }
}

void Recommender::sizeToFit() {
    static constexpr double MAX_HEIGHT = 100;

    QWidget::setFixedWidth(MARGIN_LEFT + zoom*model->getWidth() + MARGIN_RIGHT + v_scroll->width());
    double proposed_height = MARGIN_TOP + zoom*model->getHeight() + MARGIN_BOT;
    QWidget::setFixedHeight(std::min(MAX_HEIGHT, proposed_height));
    v_scroll->setFixedHeight(height());

    h_scroll->setVisible(false);

    int display_width = width() - v_scroll->width();
    v_scroll->move(display_width, 0);

    if(proposed_height > MAX_HEIGHT){
        //v_scroll->setPageStep(height());
        //v_scroll->setMaximum(proposed_height - MAX_HEIGHT);
    }else{
        //v_scroll->setMaximum(0);
    }
}

void Forscape::Typeset::Recommender::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key::Key_Down:
            moveDown();
            return;

        case Qt::Key::Key_Up:
            moveUp();
            return;

        case Qt::Key::Key_Return:
            take();
            return;

        default:
            editor->setFocus();
            editor->keyPressEvent(e);
    }
}

void Recommender::mousePressEvent(QMouseEvent* e){
    if(e->button() == Qt::MiddleButton){
        take();
        return;
    }

    if(e->pos().x() < 0 || e->pos().y() < 0 || e->pos().x() > width() || e->pos().y() > height()){
        QFrame::mousePressEvent(e);
        return;
    }

    double click_y = yModel(e->pos().y());

    Line* l = model->nearestLine(click_y);
    controller.setBothToFrontOf(l->front());
    controller.selectEndOfLine();
    ensureCursorVisible();
    update();
}

void Recommender::mouseDoubleClickEvent(QMouseEvent* e){
    if(e->pos().x() < 0 || e->pos().y() < 0 || e->pos().x() > width() || e->pos().y() > height())
        QFrame::mouseDoubleClickEvent(e);
    else
        take();
}

void Recommender::focusOutEvent(QFocusEvent* event){
    hide();
    editor->mock_focus = false;
}

void Recommender::wheelEvent(QWheelEvent* e){
    if(e->angleDelta().y() > 0) moveUp();
    else moveDown();
}

void Recommender::paintEvent(QPaintEvent* event){
    sizeToFit();

    QFrame::paintEvent(event);
    View::paintEvent(event);
}

Recommender* Editor::recommender = nullptr;

static Tooltip* tooltip = nullptr;

Editor::Editor(){
    if(tooltip == nullptr){
        tooltip = new Tooltip();
        tooltip->setWindowFlags(Qt::ToolTip);
        tooltip->setDisabled(true);
        tooltip->setFocusPolicy(Qt::NoFocus);
        tooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
        recommender = new Recommender();
    }

    tooltip_timer = new QTimer(this);
    tooltip_timer->setSingleShot(true);
    connect(tooltip_timer, SIGNAL(timeout()), this, SLOT(showTooltipParseNode()));
}

Editor::~Editor() {
    recommender->setParent(nullptr);
}

void Editor::runThread(){
    console->setModel(Typeset::Model::fromSerial("", true));

    if(!Program::instance()->noErrors()){
        Model* result = Code::Error::writeErrors(Program::instance()->error_stream.getErrors(), this);
        console->setModel(result);
    }else{
        //EVENTUALLY: print warnings to console

        is_running = true;
        allow_write = false;
        Program::instance()->runThread();
    }
}

bool Editor::isRunning() const noexcept {
    return is_running;
}

void Editor::reenable() noexcept {
    assert(Program::instance()->interpreter.status == Code::Interpreter::FINISHED);
    is_running = false;
    allow_write = true;
}

void Editor::clickLink(Model* model, size_t line) {
    emit goToModel(model, line);
}

void Editor::focusOutEvent(QFocusEvent* event){
    View::focusOutEvent(event);
    if(event->isAccepted()){
        tooltip_timer->stop();
        clearTooltip();
    }
}

void Editor::keyPressEvent(QKeyEvent* e) {
    auto v_scroll_val = v_scroll->value();
    View::keyPressEvent(e);
    if(v_scroll_val == v_scroll->value()) return;
    hover_node = NONE;
    clearTooltip();
}

void Editor::leaveEvent(QEvent* event){
    tooltip->editor = this;
    View::leaveEvent(event);
    if(tooltip->isHidden() || !tooltip->rect().contains(tooltip->mapFromGlobal(QCursor::pos())))
        clearTooltip();
}

void Editor::mousePressEvent(QMouseEvent* e){
    clearTooltip();
    View::mousePressEvent(e);
}

void Forscape::Typeset::Editor::wheelEvent(QWheelEvent* e) {
    auto v_scroll_val = v_scroll->value();
    auto old_zoom = zoom;
    View::wheelEvent(e);
    if(v_scroll_val == v_scroll->value() && old_zoom == zoom) return;
    hover_node = NONE;
    clearTooltip();
}

void Editor::resolveTooltip(double x, double y) noexcept {
    for(const Code::Error& err : model->errors){
        if(err.selection.containsWithEmptyMargin(x, y)){
            setTooltipError(std::string(err.tooltipMessage()));
            showTooltip();
            return;
        }
    }

    for(const Code::Error& err : model->warnings){
        if(err.selection.containsWithEmptyMargin(x, y)){
            setTooltipWarning(std::string(err.tooltipMessage()));
            showTooltip();
            return;
        }
    }

    hover_node = model->parseNodeAt(x, y);
    #ifndef NDEBUG
    if(hover_node == NONE) clearTooltip();
    else tooltip_timer->start(TOOLTIP_DELAY_MILLISECONDS);
    #else
    if(hover_node == NONE) clearTooltip();
    else switch( model->parseTree().getOp(hover_node) ){
        case Code::OP_IDENTIFIER:
        case Code::OP_SETTINGS_UPDATE:
            tooltip_timer->start(TOOLTIP_DELAY_MILLISECONDS);
            break;
        default: clearTooltip();

    }
    #endif

    #ifndef NDEBUG
    update();
    #endif
}

void Editor::populateContextMenuFromModel(QMenu& menu, double x, double y) {
    contextNode = model->parseNodeAt(x, y);
    if(contextNode == NONE) return;

    switch (model->parseTree().getOp(contextNode)) {
        case Code::OP_IDENTIFIER:{
            bool global_result_valid = model->is_imported;
            const Code::ParseTree* parse_tree = global_result_valid ? &parseTree() : &model->parseTree();
            auto sym_ptr = parse_tree->getSymbol(contextNode + model->parse_node_offset * global_result_valid);
            if(!sym_ptr) return;
            const Code::Symbol& sym = *sym_ptr;
            if(!sym.tied_to_file) append("Rename", rename, true, true)
            append("Go to definition", goToDef, true, true)
            append("Find usages", findUsages, true, true)
            menu.addSeparator();
            break;
        }
        case Code::OP_FILE_REF:
            append("Go to file", goToFile, true, true)
            //append("Find usages", findUsages, true, true) //EVENTUALLY: figure out cross-file usages
            menu.addSeparator();
            break;
        case Code::OP_INTEGER_LITERAL:
            if(model->parseTree().getSelection(contextNode).strView().size() < 4) break;
            append(
                display_commas_in_numbers ? "Don't show commas" : "Show commas in numbers",
                showCommasInLargeNumbers, true, true);
            menu.addSeparator();
            break;
        case Code::OP_DECIMAL_LITERAL:
            if(model->parseTree().getSelection(contextNode).strView().find('.') < 4) break;
            append(
                display_commas_in_numbers ? "Don't show commas" : "Show commas in numbers",
                showCommasInLargeNumbers, true, true);
            menu.addSeparator();
            break;
    }
}

void Editor::setTooltipError(const std::string& str){
    //EVENTUALLY: this is silly, there should be a better check
    if(tooltip->toSerial() == "Error\n"+str) return;

    tooltip->clear();
    tooltip->appendSerial("Error\n");
    tooltip->appendSerial(str, SEM_ERROR);
    tooltip->fitToContents();
}

void Editor::setTooltipWarning(const std::string& str){
    //EVENTUALLY: this is silly, there should be a better check
    if(tooltip->toSerial() == "Warning\n"+str) return;

    tooltip->clear();
    tooltip->appendSerial("Warning\n");
    tooltip->appendSerial(str, SEM_WARNING);
    tooltip->fitToContents();
}

void Editor::clearTooltip(){
    tooltip->clear();
    tooltip->hide();
    repaint();
    tooltip_timer->stop();
}

void Editor::rename(){
    QInputDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(tr("Rename"));
    dialog.setLabelText(tr("New name:"));
    dialog.setInputMode(QInputDialog::InputMode::TextInput);
    if(dialog.exec() != QDialog::Accepted) return;

    const std::string name = toCppString(dialog.textValue());

    if(isIllFormedUtf8(name)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Name is not UTF-8 encoded.");
        messageBox.setFixedSize(500,200);
        return;
    }

    if(!Forscape::isValidSerial(name)){
        QMessageBox messageBox;
        messageBox.critical(nullptr, "Error", "Name does not conform to Forscape encoding.");
        messageBox.setFixedSize(500,200);
        return;
    }

    rename(name);
}

void Editor::goToDef(){
    assert(contextNode != NONE && model->parseTree().getOp(contextNode) == Code::OP_IDENTIFIER);
    const Typeset::Selection& sel = model->parseTree().getSymbol(contextNode)->firstOccurence();
    emit goToSelection(sel);
}

void Editor::findUsages(){
    assert(console);

    assert(contextNode != NONE && model->parseTree().getOp(contextNode) == Code::OP_IDENTIFIER);
    std::vector<Typeset::Selection> occurences;
    const Code::Symbol& sym = *model->parseTree().getSymbol(contextNode);
    sym.getAllOccurences(occurences);

    std::map<std::string, std::vector<Typeset::Selection>> file_usages;
    for(auto it = occurences.crbegin(); it != occurences.crend(); it++){
        const auto& sel = *it;
        Model* m = sel.getModel();
        auto result = file_usages.insert({m->path.filename().u8string(), {sel}});
        if(!result.second) result.first->second.push_back(sel);
    }

    Model* m = new Model();
    m->is_output = true;
    console->setModel(m);
    size_t last_handled = std::numeric_limits<size_t>::max();
    for(const auto& map_entry : file_usages){
        console->appendSerial(map_entry.first + ":\n");
        for(const auto& sel : map_entry.second){
            Line* target_line = sel.getStartLine();
            if(target_line->id != last_handled){
                last_handled = target_line->id;
                console->appendSerial("   ");
                m->lastLine()->appendConstruct(new Typeset::MarkerLink(target_line, this, target_line->parent));
                std::string line_snippet = target_line->toString();
                console->appendSerial("  " + line_snippet + "\n");
            }
        }
    }

    console->updateModel();
}

void Editor::goToFile() {
    assert(contextNode != NONE && parseTree().getOp(contextNode) == Code::OP_FILE_REF);
    Typeset::Model* referenced = parseTree().getModel(contextNode + model->parse_node_offset);
    emit goToModel(referenced, 0);
}

void Editor::showTooltipParseNode(){
    if(this->hover_node == NONE) return;

    //EVENTUALLY: dealing with errors here is a pain
    bool global_result_valid = model->is_imported;
    const Code::ParseTree* parse_tree = global_result_valid ? &parseTree() : &model->parseTree();
    const ParseNode hover_node = this->hover_node + model->parse_node_offset * global_result_valid;
    switch(parse_tree->getOp(hover_node)){
        case Code::OP_IDENTIFIER:{
            const auto symbol = parse_tree->getSymbol(hover_node);
            if(!symbol) return;

            tooltip->clear();
            tooltip->appendSerial(parse_tree->str(hover_node) + " ∈ " + staticPass().typeString(*symbol));

            if(symbol->comment != NONE){
                std::string comment = model->parser.parse_tree.str(symbol->comment);
                if(!isIllFormedUtf8(comment)){
                    tooltip->appendSerial("\n");
                    tooltip->appendSerial(model->parser.parse_tree.str(symbol->comment), SEM_COMMENT);
                }
            }

            tooltip->fitToContents();
            break;
        }

        case Code::OP_SETTINGS_UPDATE:{
            Settings* settings = debug_cast<Settings*>(parse_tree->getLeft(hover_node).text->nextConstructAsserted());

            QPointF point = QWidget::mapFromGlobal(QCursor::pos());
            std::string str = settings->getTooltip(xModel(point.x()) - settings->x, yModel(point.y()) - settings->y);
            if(str.empty()) return;

            tooltip->clear();
            tooltip->appendSerial(str);
            tooltip->fitToContents();
            break;
        }

        #ifndef NDEBUG
        default:
            tooltip->clear();
            tooltip->appendSerial("ParseNode: " + std::to_string(hover_node));
            tooltip->fitToContents();
        #endif
    }

    showTooltip();
}

void Editor::showTooltip(){
    if(tooltip->isVisible()) return;

    tooltip->updateModel();
    tooltip->fitToContents();
    tooltip->move(QCursor::pos());
    tooltip->show();
}

void Editor::showCommasInLargeNumbers() {
    emit setCommasInLargeNumbers(!display_commas_in_numbers);
}

void Editor::rename(const std::string& str){
    assert(isValidSerial(str));
    assert(contextNode != NONE && model->parseTree().getOp(contextNode) == Code::OP_IDENTIFIER);
    std::vector<Typeset::Selection> occurences;
    const Code::Symbol& sym = *model->parseTree().getSymbol(contextNode);

    //EVENTUALLY: could optimise for single-file case
    //sym.getLocalOccurences(occurences);
    //Model* m = occurences.front().getModel();
    //Controller c(m);
    //m->rename(occurences, str, c);
    //occurences.clear();

    sym.getAllOccurences(occurences);

    //Ugly heroics to make sure the final cursor is in the replaced word and valid
    int final_absolute_index_in_phrase = 0;
    int num_chars_added_in_replacement = (int)str.size() - (int)occurences.front().topLevelChars();
    Phrase* p = nullptr;

    if(!occurences.empty()){
        FORSCAPE_UNORDERED_MAP<Model*, std::vector<Typeset::Selection>> map;
        for(const Typeset::Selection& sel : occurences){
            auto result = map.insert({sel.getModel(), {sel}});
            if(!result.second) result.first->second.push_back(sel);
        }

        for(auto& entry : map){
            Model* m = entry.first;

            if(m == model){
                for(const auto& sel : entry.second) if(sel.containsInclusive(controller.anchor) && p==nullptr){
                    final_absolute_index_in_phrase += sel.right.absoluteIndexInPhrase();
                    p = sel.left.phrase();
                }
                for(const auto& sel : entry.second) if(sel.left.phrase() == p && sel.left.precedesInclusive(controller.anchor))
                    final_absolute_index_in_phrase += num_chars_added_in_replacement;
            }

            Controller c(m);
            m->rename(entry.second, str, c);
            emit modelChanged(m);
        }
    }

    assert(p != nullptr);
    Marker m = Marker::fromAbsoluteIndex(*p, final_absolute_index_in_phrase);
    controller.anchor = controller.active = m;
    hover_node = NONE;

    restartCursorBlink();
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void Editor::recommend() {
    populateSuggestions();

    if(suggestions.empty()){
        recommender->hide();
        setFocus();
    }else{
        recommender->clear();
        std::string str = suggestions.front();
        for(size_t i = 1; i < suggestions.size(); i++) str += '\n' + suggestions[i];
        recommender->setFromSerial(str, true);
        recommender->getController().moveToStartOfDocument();
        recommender->getController().selectEndOfLine();

        recommender->editor = this;
        recommender->setParent(this);
        recommender->setWindowFlags(Qt::Popup);

        double x = xScreen(controller.xActive());
        double y = yScreen(controller.active.y() + controller.active.text->height());
        recommender->move(x, y);
        QPointF global = mapToGlobal(pos());
        recommender->move(global.x() + x, global.y() + y);

        recommender->updateModel();
        recommender->sizeToFit();

        recommender->show();
        mock_focus = true;
        recommender->setFocus();

        recommender->recommend_typeset_phrase_size = 0;
    }
}

void Editor::recommendTypeset(std::string_view phrase) {
    suggestions.clear();
    for(const auto& entry : Keywords::map){
        const std::string& candidate = entry.first;
        if(candidate.size() >= phrase.size() && std::string_view(candidate.data(), phrase.size()) == phrase)
            suggestions.push_back(candidate);
    }
    std::sort(suggestions.begin(), suggestions.end());

    if(suggestions.empty()){
        recommender->hide();
        setFocus();
    }else{
        recommender->clear();
        std::string str = suggestions.front();
        for(size_t i = 1; i < suggestions.size(); i++) str += '\n' + suggestions[i];
        recommender->setFromSerial(str, true);
        recommender->getController().moveToStartOfDocument();
        recommender->getController().selectEndOfLine();

        recommender->editor = this;
        recommender->setParent(this);
        recommender->setWindowFlags(Qt::Popup);

        double x = xScreen(controller.xActive());
        double y = yScreen(controller.active.y() + controller.active.text->height());
        recommender->move(x, y);
        QPointF global = mapToGlobal(pos());
        recommender->move(global.x() + x, global.y() + y);

        recommender->updateModel();
        recommender->sizeToFit();

        recommender->show();
        mock_focus = true;
        recommender->setFocus();

        recommender->recommend_typeset_phrase_size = phrase.size() + 1;
    }
}

void Editor::populateSuggestions() {
    recommend_without_hint = false;
    filename_start = nullptr;
    recommender->should_erase = true;
    suggestions.clear();

    for(const Code::Error& err : model->errors){
        if(err.code == Code::EXPECTED_FILEPATH && err.selection.left == controller.anchor){
            suggestFileNames(); return;
        }else if(err.code == Code::FILE_NOT_FOUND && err.selection.right == controller.anchor){
            suggestFileNames(err.selection); return;
        }else if(err.code == Code::MODULE_FIELD_NOT_FOUND && err.selection.right == controller.anchor){
            suggestModuleFields(err); return;
        }
    }

    model->symbol_builder.symbol_table.getSuggestions(controller.active, suggestions);
}

void Editor::suggestFileNames() {
    recommend_without_hint = true;
    Program::instance()->getFileSuggestions(suggestions, model);
    std::sort(suggestions.begin(), suggestions.end());
    suggestions.erase(std::unique(suggestions.begin(), suggestions.end()), suggestions.end());
}

void Editor::suggestFileNames(const Selection& sel) {
    if(!sel.isTextSelection()) return;
    filename_start = &sel.left;
    Program::instance()->getFileSuggestions(suggestions, sel.strView(), model);
    std::sort(suggestions.begin(), suggestions.end());
    suggestions.erase(std::unique(suggestions.begin(), suggestions.end()), suggestions.end());
}

void Editor::suggestModuleFields(const Code::Error& err) {
    const Typeset::Selection& sel = err.selection;
    recommender->should_erase = !sel.isEmpty();
    size_t flag = err.flag;
    const auto& lexical_map = *reinterpret_cast<FORSCAPE_UNORDERED_MAP<Typeset::Selection, size_t>*>(flag);
    for(const auto& entry : lexical_map)
        if(entry.first.startsWith(sel))
            suggestions.push_back(entry.first.str());
    std::sort(suggestions.begin(), suggestions.end());
}

void Editor::takeRecommendation(const std::string& str){
    bool recommend_again = false;

    if(recommend_without_hint){
        if(controller.charLeft() != ' ')
            controller.insertSerial(' ' + str);
        else
            controller.insertSerial(str);
    }else if(filename_start){
        controller.anchor = *filename_start;
        controller.insertSerial(str);
        recommend_again = (str[str.size()-1] == '/');
    }else{
        //EVENTUALLY: mechanism for deciding when to erase the recommender candidate is janky
        std::string copy = str;

        if(recommender->should_erase) controller.selectPrevWord();
        else if(controller.charLeft() == 't') copy = ' ' + copy;
        if(str != controller.selectedText())
            controller.insertSerial(copy);
        else
            controller.consolidateToAnchor();
    }

    model->performSemanticFormatting();
    updateXSetpoint();
    updateModel();

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();

    if(!recommend_again){
        recommender->hide();
        setFocus();
    } else recommend();
}

void Tooltip::leaveEvent(QEvent* event) {
    if(!editor->rect().contains(editor->mapFromGlobal(QCursor::pos())))
        editor->clearTooltip();
}

}

}
