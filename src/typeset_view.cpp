#include <typeset_view.h>

#include <hope_logging.h>
#include <typeset_command_pair.h>
#include <typeset_construct.h>
#include <typeset_line.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>
#include <typeset_themes.h>

#include <chrono>

#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
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

namespace Hope {

namespace Typeset {

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
    SelectionDrag,
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

    recommender->hide();
    connect(recommender, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(takeRecommendation(QListWidgetItem*)));
    connect(recommender, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(takeRecommendation(QListWidgetItem*)));
    recommender->setMinimumHeight(0);
    recommender->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    v_scroll = new VerticalScrollBar(Qt::Vertical, this, *this);
    h_scroll = new QScrollBar(Qt::Horizontal, this);
    connect(v_scroll, SIGNAL(valueChanged(int)), this, SLOT(update()));
    connect(h_scroll, SIGNAL(valueChanged(int)), this, SLOT(update()));

    v_scroll->setCursor(Qt::ArrowCursor);
    h_scroll->setCursor(Qt::ArrowCursor);

    updateBackgroundColour();
    setAutoFillBackground(true);
    all_views.push_back(this);
}

View::~View() noexcept {
    if(model_owned) delete model;
    all_views.remove(this);
}

void View::setFromSerial(const std::string& src, bool is_output){
    if(model_owned) delete model;
    model_owned = true;
    model = Model::fromSerial(src, is_output);
    controller = Controller(model);
    h_scroll->setValue(h_scroll->minimum());
    updateXSetpoint();
    handleResize();
    updateHighlightingFromCursorLocation();
    update();
}

std::string View::toSerial() const alloc_except {
    return model->toSerial();
}

Model* View::getModel() const noexcept {
    return model;
}

void View::setModel(Model* m, bool owned) noexcept {
    if(model_owned) delete model;
    highlighted_words.clear();
    model_owned = owned;
    model = m;
    controller = Controller(model);
    updateXSetpoint();
    update();
    handleResize();
}

void View::runThread(View* console){
    if(!model->errors.empty()){
        Model* result = Code::Error::writeErrors(model->errors, this);
        result->calculateSizes();
        result->updateLayout();
        console->setModel(result);
    }else{
        is_running = true;
        allow_write = false;
        model->runThread();
    }
}

void View::setLineNumbersVisible(bool show) noexcept {
    logger->info("{}setLineNumbersVisible({});", logPrefix(), show);

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
    }else if(recentlyDoubleClicked() | isInLineBox(x)){
        resolveTripleClick(y);
        mouse_hold_state = TripleClick;
    }else if(shift_held){
        logger->info("{}resolveShiftClick({}, {});", logPrefix(), x, y);
        resolveShiftClick(x, y);
    }else if(ALLOW_SELECTION_DRAG && controller.contains(x, y)){
        mouse_hold_state = ClickedOnSelection;
    }else{
        logger->info("{}dispatchClick({}, {}, {}, {}, {}, {});", logPrefix(), x, y, xScreen, yScreen, right_click, shift_held);
        resolveClick(x, y);
        mouse_hold_state = SingleClick;
    }

    restartCursorBlink();
    updateHighlightingFromCursorLocation();
    update();
}

void View::dispatchRelease(double x, double y){
    logger->info("{}dispatchRelease({}, {});", logPrefix(), x, y);

    if(mouse_hold_state == ClickedOnSelection)
        resolveClick(x, y);
    else if(mouse_hold_state == SelectionDrag)
        resolveSelectionDrag(x, y);

    mouse_hold_state = Hover;

    update();
}

void View::dispatchDoubleClick(double x, double y){
    logger->info("{}dispatchDoubleClick({}, {});", logPrefix(), x, y);

    if(!isInLineBox(x)){
        resolveWordClick(x, y);
        update();
    }

    mouse_hold_state = DoubleClick;
    updateDoubleClickTime();
}

void View::dispatchHover(double x, double y){
    setCursorAppearance(x, y);

    switch(mouse_hold_state){
        case Hover: resolveTooltip(x, y); break;
        case SingleClick:
            logger->info("{}dispatchHover({}, {}); //Drag to select", logPrefix(), x, y);
            resolveShiftClick(x, y); update(); break;
        case DoubleClick: resolveWordDrag(x, y); update(); break;
        case TripleClick: resolveLineDrag(y); update(); break;
        case ClickedOnSelection:
            mouse_hold_state = SelectionDrag;
            QWidget::setCursor(Qt::DragMoveCursor);
            break;
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

void View::resolveClick(double x, double y) noexcept{
    Line* l = model->nearestLine(y);
    controller.clickTo(l, x, y);
    updateXSetpoint();
}

void View::resolveShiftClick(double x, double y) noexcept {
    Phrase* p = controller.isTopLevel() ? model->nearestLine(y) : controller.phrase();
    controller.shiftClick(p, x);
    updateXSetpoint();
    ensureCursorVisible();
}

void View::resolveRightClick(double x, double y, int xScreen, int yScreen){
    logger->info("{}resolveRightClick({}, {}, {}, {});", logPrefix(), x, y, xScreen, yScreen);

    QMenu menu;

    bool clicked_on_selection = controller.contains(x,y);

    if(!clicked_on_selection) resolveClick(x, y);

    #define append(name, str, cmd, enabled, visible) \
        if(visible){ \
        QAction* name = menu.addAction(str); \
        connect(name, SIGNAL(triggered()), this, SLOT(cmd())); \
        name->setEnabled(enabled); \
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
                [=]() { action.takeAction(con, controller, child); }
            );

            q_action->setEnabled( action.enabled );
        }
    }

    Controller c = model->idAt(x, y);
    if(c.hasSelection()){
        auto& symbol_table = model->symbol_builder.symbol_table;
        auto lookup = symbol_table.occurence_to_symbol_map.find(c.getAnchor());
        append(renameAction, "Rename", rename, true, lookup!=symbol_table.occurence_to_symbol_map.end())
        append(gotoDefAction, "Go to definition", goToDef, true, lookup!=symbol_table.occurence_to_symbol_map.end())
        append(gotoDefAction, "Find usages", findUsages, true, console && lookup!=symbol_table.occurence_to_symbol_map.end())
        menu.addSeparator();
    }

    append(undoAction, "Undo", undo, model->undoAvailable(), allow_write)
    append(redoAction, "Redo", redo, model->redoAvailable(), allow_write)
    menu.addSeparator();
    append(cutAction, "Cut", cut, clicked_on_selection, allow_write)
    append(copyAction, "Copy", copy, clicked_on_selection, true)
    append(pasteAction, "Paste", paste, true, allow_write)
    menu.addSeparator();
    append(selectAllAction, "Select All", selectAll, true, true)

    #undef append

    menu.exec(QPoint(xScreen, yScreen));
}

void View::resolveWordClick(double x, double y){
    resolveClick(x, y);
    controller.moveToPrevWord();
    controller.selectNextWord();
    restartCursorBlink();
    updateXSetpoint();
}

void View::resolveWordDrag(double x, double y) noexcept{
    THROTTLE(logger->info("{}resolveWordDrag({}, {});", logPrefix(), x, y);)

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

void View::resolveTripleClick(double y) noexcept{
    logger->info("{}resolveTripleClick({});", logPrefix(), y);

    Line* l = model->nearestLine(y);
    controller.selectLine(l);
    updateXSetpoint();

    invalidateDoubleClickTime();
}

void View::resolveLineDrag(double y) noexcept{
    THROTTLE(logger->info("{}resolveLineDrag({});", logPrefix(), y);)

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

void View::resolveTooltip(double x, double y) noexcept{
    if(model->is_output) return; //EVENTUALLY: find a better way to have different behaviours

    for(const Code::Error& err : model->errors){
        if(err.selection.containsWithEmptyMargin(x, y)){
            THROTTLE(logger->info("{}resolveTooltip({}, {});", logPrefix(), x, y);)

            setTooltipError(err.message());
            return;
        }
    }

    for(const Code::Error& err : model->warnings){
        if(err.selection.containsWithEmptyMargin(x, y)){
            THROTTLE(logger->info("{}resolveTooltip({}, {});", logPrefix(), x, y);)

            setTooltipWarning(err.message());
            return;
        }
    }

    Controller c = model->idAt(x, y);
    if(c.hasSelection()){
        setToolTipDuration(std::numeric_limits<int>::max());
        const auto& symbol_table = model->symbol_builder.symbol_table;
        auto lookup = symbol_table.occurence_to_symbol_map.find(c.anchor);

        //EVENTUALLY enable the assertion when idAt accurately reports identifier
        //assert(lookup != symbol_table.occurence_to_symbol_map.end() || model->errors.size());
        if(lookup != symbol_table.occurence_to_symbol_map.end()){
            THROTTLE(logger->info("{}resolveTooltip({}, {});", logPrefix(), x, y);)

            const auto& symbol = symbol_table.symbols[lookup->second];
            QString tooltip = "<b>" + QString::fromStdString(c.selectedText()) + "</b> ∈ "
                    + QString::fromStdString(model->static_pass.typeString(symbol));
            if(symbol.comment != NONE)
                tooltip += "<div style=\"color:green\">" + QString::fromStdString(symbol_table.parse_tree.str(symbol.comment));
            setToolTip(tooltip);
            return;
        }
    }

    clearTooltip();
}

void View::resolveSelectionDrag(double x, double y){
    logger->info("{}resolveSelectionDrag({}, {});", logPrefix(), x, y);

    mouse_hold_state = Hover;

    if(controller.contains(x, y)) return;

    model->premutate();
    std::string str = controller.selectedText();
    Command* a = controller.deleteSelection();
    bool delete_first = x <= controller.xActive();
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

bool View::isInLineBox(double x) const noexcept{
    return x-h_scroll->value()/zoom < -MARGIN_LEFT;
}

void View::updateXSetpoint() noexcept {
    x_setpoint = controller.xActive();
}

double View::xModel(double xScreen) const noexcept{
    return xScreen/zoom - xOrigin();
}

double View::yModel(double yScreen) const noexcept{
    return yScreen/zoom - yOrigin();
}

double View::xScreen(double xModel) const noexcept{
    return zoom*(xModel + xOrigin());
}

double View::yScreen(double yModel) const noexcept{
    return zoom*(yModel + yOrigin());
}

void View::zoomIn() noexcept{
    logger->info("{}zoomIn();", logPrefix());
    zoom = std::min(ZOOM_MAX, zoom*ZOOM_DELTA);
}

void View::zoomOut() noexcept{
    logger->info("{}zoomOut();", logPrefix());
    zoom = std::max(ZOOM_MIN, zoom/ZOOM_DELTA);
}

void View::resetZoom() noexcept {
    logger->info("{}resetZoom();", logPrefix());
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

    Controller c = model->idAt(controller.active);
    if(c.hasSelection()){
        auto& symbol_table = model->symbol_builder.symbol_table;
        symbol_table.getSymbolOccurences(c.getAnchor(), highlighted_words);
    }

    update();
    updateAfterHighlightChange();
}

bool View::scrolledToBottom() const noexcept{
    return v_scroll->value() == v_scroll->maximum();
}

void View::scrollToBottom(){
    v_scroll->setValue(v_scroll->maximum());
    update();
}

bool View::lineNumbersShown() const noexcept{
    return show_line_nums;
}

void View::insertText(const std::string& str){
    logger->info("{}insertText({});", logPrefix(), cStr(str));
    controller.insertText(str);
}

void View::insertSerial(const std::string& str){
    logger->info("{}insertSerial({});", logPrefix(), cStr(str));
    controller.insertSerial(str);
}

size_t View::numLines() const noexcept{
    return model->numLines();
}

size_t View::currentLine() const noexcept{
    return controller.activeLine()->id;
}

void View::ensureCursorVisible() noexcept{
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

void View::updateModel() noexcept{
    model->calculateSizes();
    model->updateLayout();
    handleResize();
    update();
}

void View::goToLine(size_t line_num){
    logger->info("{}followLink({});", logPrefix(), line_num);

    if(line_num > model->lines.size()) return;
    controller.setBothToBackOf(model->lines[line_num]->back());
    ensureCursorVisible();
    restartCursorBlink();
    update();
}

bool View::isRunning() const noexcept{
    return is_running;
}

void View::reenable() noexcept{
    assert(model->interpreter.status == Code::Interpreter::FINISHED);
    is_running = false;
    allow_write = true;
}

void View::updateBackgroundColour() noexcept {
    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, getColour(COLOUR_BACKGROUND));
    setPalette(pal);
}

void View::updateAfterHighlightChange() noexcept{
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

void View::recommend() {
    auto suggestions = model->symbol_builder.symbol_table.getSuggestions(controller.active);
    if(suggestions.empty()){
        recommender->hide();
    }else{
        recommender->clear();
        for(const auto& suggestion : suggestions)
            recommender->addItem(QString::fromStdString(suggestion.str()));

        double x = xScreen(controller.xActive());
        double y = yScreen(controller.active.y() + controller.active.text->height());
        recommender->move(x, y);
        int full_list_height = recommender->sizeHintForRow(0) * recommender->count() + 2*recommender->frameWidth();
        static constexpr int MAX_HEIGHT = 250;
        if(full_list_height <= MAX_HEIGHT){
            recommender->setFixedHeight(full_list_height);
        }else{
            recommender->setFixedHeight(MAX_HEIGHT);
        }
        recommender->show();
    }
}

void View::keyPressEvent(QKeyEvent* e){
    handleKey(e->key(), e->modifiers(), e->text().toStdString());
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

    recommender->hide();
}

void View::mouseDoubleClickEvent(QMouseEvent* e){
    dispatchDoubleClick(xModel(e->x()), yModel(e->y()));
    recommender->hide();
}

void View::mouseReleaseEvent(QMouseEvent* e){
    dispatchRelease(xModel(e->x()), yModel(e->y()));
    recommender->hide();
}

void View::mouseMoveEvent(QMouseEvent* e){
    dispatchHover(xModel(e->x()), yModel(e->y()));
}

void View::wheelEvent(QWheelEvent* e){
    bool ctrl_held = e->modifiers() == Qt::ControlModifier;
    bool up = e->angleDelta().y() > 0;

    dispatchMousewheel(ctrl_held, up);
    recommender->hide();
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
    }else{
        stopCursorBlink();
        //EVENTUALLY: why was this here?
        //assert(mouse_hold_state == Hover);
    }

    if(focusWidget() != recommender) recommender->hide();
}

void View::resizeEvent(QResizeEvent* e){
    QWidget::resizeEvent(e);
    handleResize();
}

void View::onBlink() noexcept{
    show_cursor = !show_cursor && hasFocus();
    update();
    cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
}

void View::copy() const{
    logger->info("{}copy();", logPrefix());
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(QString::fromStdString(str));
}

void View::cut(){
    logger->info("{}cut();", logPrefix());
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(QString::fromStdString(str));
    controller.del();

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paste(){
    std::string str = QGuiApplication::clipboard()->text().toStdString();
    paste(str);
}

void View::del(){
    logger->info("{}del();", logPrefix());
    controller.del();

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::setCursorAppearance(double x, double y) {
    if(mouse_hold_state != SelectionDrag){
        Construct* con = model->constructAt(x, y);
        if(con && con->constructCode() == MARKERLINK){
            QWidget::setCursor(Qt::PointingHandCursor);
        }else{
            QWidget::setCursor(isInLineBox(x) ? Qt::ArrowCursor : Qt::IBeamCursor);
        }
    }
}

void View::drawModel(double xL, double yT, double xR, double yB) {
    Painter painter(qpainter);
    painter.setZoom(zoom);
    painter.setOffset(xOrigin(), yOrigin());

    QColor error_background = getColour(COLOUR_ERRORBACKGROUND);
    QColor error_border = getColour(COLOUR_ERRORBORDER);

    //EVENTUALLY: get rid of this hack
    setColour(COLOUR_ERRORBACKGROUND, getColour(COLOUR_WARNINGBACKGROUND));
    setColour(COLOUR_ERRORBORDER, getColour(COLOUR_WARNINGBORDER));

    for(const Code::Error& e : model->warnings)
        if(e.selection.overlapsY(yT, yB))
            e.selection.paintError(painter);

    setColour(COLOUR_ERRORBACKGROUND, error_background);
    setColour(COLOUR_ERRORBORDER, error_border);

    for(const Code::Error& e : model->errors)
        if(e.selection.overlapsY(yT, yB))
            e.selection.paintError(painter);

    for(const Selection& c : highlighted_words)
        if(c.overlapsY(yT, yB))
            c.paintHighlight(painter);

    const Typeset::Marker& cursor = getController().active;

    model->paint(painter, xL, yT, xR, yB);
    model->paintGroupings(painter, cursor);
    controller.paintSelection(painter, yT, yB);
    if(show_cursor){
        if(insert_mode) controller.paintInsertCursor(painter, yT, yB);
        else controller.paintCursor(painter);
    }
}

void View::drawLinebox(double yT, double yB) {
    if(!show_line_nums) return;

    qpainter.resetTransform();
    qpainter.setBrush(getColour(COLOUR_LINEBOXFILL));
    qpainter.setPen(getColour(COLOUR_LINEBOXBORDER));
    qpainter.drawRect(-1, -1, 1+LINEBOX_WIDTH*zoom, height()+2);

    Painter painter(qpainter);
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
    THROTTLE(logger->info("{}handleResize({}, {});", logPrefix(), w, h);)

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
    logger->info("{}scrollUp();", logPrefix());

    v_scroll->setValue( v_scroll->value() - v_scroll->pageStep()/10 );
}

void View::scrollDown(){
    logger->info("{}scrollDown();", logPrefix());

    v_scroll->setValue( v_scroll->value() + v_scroll->pageStep()/10 );
}

void View::setTooltipError(const std::string& str){
    setToolTipDuration(std::numeric_limits<int>::max());
    setToolTip("<b>Error</b><div style=\"color:red\">" + QString::fromStdString(str));
}

void View::setTooltipWarning(const std::string& str){
    setToolTipDuration(std::numeric_limits<int>::max());
    setToolTip("<b>Warning</b><div style=\"color:SandyBrown\">" + QString::fromStdString(str));
}

void View::clearTooltip(){
    setToolTipDuration(1);
    setToolTip(" ");
}

void View::undo(){
    logger->info("{}undo();", logPrefix());

    model->undo(controller);
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    recommender->hide();
    update();

    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::redo(){
    logger->info("{}redo();", logPrefix());

    model->redo(controller);
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    recommender->hide();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::selectAll() noexcept{
    logger->info("{}selectAll();", logPrefix());

    controller.selectAll();
}

void View::rename(){
    bool ok;
    QString text = QInputDialog::getText(
                this,
                tr("Rename"),
                tr("New name:"),
                QLineEdit::Normal,
                "",
                &ok
                );

    if(!ok) return;

    std::string name = text.toStdString();
    rename(name);
}

void View::goToDef(){
    logger->info("{}goToDef();", logPrefix());

    Controller c = model->idAt(controller.active);
    assert(c.hasSelection());

    auto& symbol_table = model->symbol_builder.symbol_table;
    auto lookup = symbol_table.occurence_to_symbol_map.find(c.getAnchor());
    assert(lookup != symbol_table.occurence_to_symbol_map.end());

    controller = symbol_table.getSel(lookup->second);
    restartCursorBlink();
    ensureCursorVisible();
    update();
}

void View::findUsages(){
    logger->info("{}findUsages();", logPrefix());

    assert(console);

    Controller c = model->idAt(controller.active);
    assert(c.hasSelection());

    auto& symbol_table = model->symbol_builder.symbol_table;
    std::vector<Typeset::Selection> occurences;
    symbol_table.getSymbolOccurences(c.getAnchor(), occurences);

    Model* m = new Model();
    m->is_output = true;
    console->setModel(m);
    size_t last_handled = std::numeric_limits<size_t>::max();
    for(const auto& entry : occurences){
        Line* target_line = entry.getStartLine();
        if(target_line->id != last_handled){
            last_handled = target_line->id;
            m->lastLine()->appendConstruct(new Typeset::MarkerLink(target_line, this));
            console->controller.moveToEndOfDocument();
            std::string line_snippet = target_line->toString();
            console->controller.insertSerial("  " + line_snippet + "\n");
        }
    }

    console->updateModel();
}

void View::takeRecommendation(QListWidgetItem* item){
    takeRecommendation(item->text().toStdString());
}

void View::handleKey(int key, int modifiers, const std::string& str){
    constexpr int Ctrl = Qt::ControlModifier;
    constexpr int Shift = Qt::ShiftModifier;
    constexpr int CtrlShift = Qt::ControlModifier | Qt::ShiftModifier;
    constexpr int Alt = Qt::AltModifier;

    bool hide_recommender = true;

    logger->info("{}handleKey({}, {}, {});", logPrefix(), key, modifiers, cStr(str));

    switch (key | modifiers) {
        case Qt::Key_Z|Ctrl: if(allow_write) model->undo(controller); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Y|Ctrl: if(allow_write) model->redo(controller); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Right: controller.moveToNextChar(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left: controller.moveToPrevChar(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Right|Ctrl: controller.moveToNextWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Left|Ctrl: controller.moveToPrevWord(); updateXSetpoint(); restartCursorBlink(); update(); break;
        case Qt::Key_Down:
            if(recommender->isVisible()){
                recommender->setCurrentRow(0);
                recommender->setFocus();
                hide_recommender = false;
            }else{
                controller.moveToNextLine(x_setpoint); restartCursorBlink(); update();
            }
            break;
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
            controller.keystroke(str);
            updateXSetpoint();
            restartCursorBlink();
            if(!model->is_output) recommend();
            hide_recommender = false;
    }

    if(hide_recommender && recommender->isVisible()){
        recommender->hide();
        setFocus();
    }
    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paste(const std::string& str){
    logger->info("{}paste({});", logPrefix(), cStr(str));
    model->mutate(controller.getInsertSerial(str), controller);

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::rename(const std::string& str){
    logger->info("{}rename({});", logPrefix(), cStr(str));

    Controller c = model->idAt(controller.active);
    assert(c.hasSelection());

    auto& symbol_table = model->symbol_builder.symbol_table;
    std::vector<Typeset::Selection> occurences;
    symbol_table.getSymbolOccurences(c.getAnchor(), occurences);
    replaceAll(occurences, str);

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::takeRecommendation(const std::string& str){
    logger->info("{}takeRecommendation({});", logPrefix(), cStr(str));

    controller.selectPrevWord();
    if(str != controller.selectedText())
        controller.insertSerial(str);
    else
        controller.consolidateToAnchor();
    model->performSemanticFormatting();
    updateXSetpoint();
    updateModel();
    recommender->hide();
    //EVENTUALLY: this isn't working. The recommender will need to be a custom class (it will need to be for typesetting anyway)
    QTimer::singleShot(0, this, SLOT(setFocus())); //Delay 1 cycle to avoid whatever input activated item

    ensureCursorVisible();
    updateHighlightingFromCursorLocation();
    update();
    v_scroll->update();
    qApp->processEvents();

    emit textChanged();
}

void View::paintEvent(QPaintEvent* event){
    qpainter.begin(this);
    qpainter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

    QRect r = event->rect();
    double xL = xModel(r.x());
    double yT = yModel(r.y());
    double xR = xModel(r.x() + r.width());
    double yB = yModel(r.y() + r.height());

    drawModel(xL, yT, xR, yB);
    drawLinebox(yT, yB);
    fillInScrollbarCorner();
    QWidget::paintEvent(event);

    qpainter.end();
}

QImage View::toPng() const{
    logger->info("{}toPng();", logPrefix());
    return controller.selection().toPng();
}

std::string_view Hope::Typeset::View::logPrefix() const noexcept{
    return model->is_output ? "console->" : "editor->";
}

LineEdit::LineEdit() : View() {
    allow_new_line = false;
    setLineNumbersVisible(false);
    v_scroll->hide();
    h_scroll->hide();
    v_scroll->setDisabled(true);
    h_scroll->setDisabled(true);
    model->is_output = true;

    zoom = 1;
}

}

}
