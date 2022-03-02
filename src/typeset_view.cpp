#include <typeset_view.h>

#include <typeset_command_pair.h>
#include <typeset_construct.h>
#include <typeset_line.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>

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

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Typeset {

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

QColor View::selection_box_color = QColor::fromRgb(0, 120, 215);
QColor View::selection_text_color = Qt::white;
QColor View::text_cursor_color = Qt::black;
QColor View::error_background_color = QColor::fromRgb(255, 180, 180);
QColor View::error_border_color = QColor::fromRgb(255, 50, 50);
QColor View::line_box_fill_color = QColor::fromRgb(240, 240, 240);
QColor View::line_box_border_color = Qt::lightGray;
QColor View::line_num_active_color = Qt::black;
QColor View::line_num_passive_color = Qt::darkGray;
QColor View::grouping_highlight_color = QColor::fromRgb(255, 0, 0);
QColor View::grouping_background_color = QColor::fromRgb(180, 238, 180);

View::View()
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

    v_scroll = new QScrollBar(Qt::Vertical, this);
    h_scroll = new QScrollBar(Qt::Horizontal, this);
    connect(v_scroll, SIGNAL(valueChanged(int)), this, SLOT(repaint()));
    connect(h_scroll, SIGNAL(valueChanged(int)), this, SLOT(repaint()));

    h_scroll->setStyleSheet(
        "background-color: #F9C929;"
        "alternate-background-color: #D5EAFF;"
    );

    h_scroll->setToolTip("Warning: line width exceeds editor width. Uncle Bob is displeased.");

    v_scroll->setCursor(Qt::ArrowCursor);
    h_scroll->setCursor(Qt::ArrowCursor);
}

View::~View(){
    if(model_owned) delete model;
}

void View::setFromSerial(const std::string& src){
    if(model_owned) delete model;
    model_owned = true;
    model = Model::fromSerial(src);
    controller = Controller(model);
    updateXSetpoint();
    handleResize();
    updateHighlighting();
    repaint();
}

Model* View::getModel() const noexcept{
    return model;
}

void View::setModel(Model* m, bool owned){
    if(model_owned) delete model;
    highlighted_words.clear();
    model_owned = owned;
    model = m;
    controller = Controller(model);
    updateXSetpoint();
    repaint();
    handleResize();
}

void View::runThread(View* console){
    if(!model->errors.empty()){
        Model* result = Code::Error::writeErrors(model->errors, this);
        result->calculateSizes();
        result->updateLayout();
        console->setModel(result);
    }else{
        model->runThread();
    }
}

void View::setLineNumbersVisible(bool show){
    if(show_line_nums == show) return;
    show_line_nums = show;
    update();
}

Controller& View::getController() noexcept{
    return controller;
}

void View::setReadOnly(bool read_only) noexcept{
    if(allow_write == !read_only) return;
    allow_write = !read_only;

    if(read_only) stopCursorBlink();
    else restartCursorBlink();
}

void View::rename(const std::vector<Selection>& targets, const std::string& name){
    model->rename(targets, name, controller);

    restartCursorBlink();
    ensureCursorVisible();
    repaint();
}

void View::dispatchClick(double x, double y, int xScreen, int yScreen, bool right_click, bool shift_held){
    if(right_click){
        resolveRightClick(x, y, xScreen, yScreen);
    }else if(recentlyDoubleClicked() | isInLineBox(x)){
        resolveTripleClick(y);
        mouse_hold_state = TripleClick;
    }else if(shift_held){
        resolveShiftClick(x, y);
    }else if(ALLOW_SELECTION_DRAG && controller.contains(x, y)){
        mouse_hold_state = ClickedOnSelection;
    }else{
        resolveClick(x, y);
        mouse_hold_state = SingleClick;
    }

    restartCursorBlink();
    updateHighlighting();
    repaint();
}

void View::dispatchRelease(double x, double y){
    if(mouse_hold_state == ClickedOnSelection)
        resolveClick(x, y);
    else if(mouse_hold_state == SelectionDrag)
        resolveSelectionDrag(x, y);

    mouse_hold_state = Hover;

    repaint();
}

void View::dispatchDoubleClick(double x, double y){
    if(!isInLineBox(x)){
        resolveWordClick(x, y);
        repaint();
    }

    mouse_hold_state = DoubleClick;
    updateDoubleClickTime();
}

void View::dispatchHover(double x, double y){
    setCursorAppearance(x, y);

    switch(mouse_hold_state){
        case Hover: resolveTooltip(x, y); break;
        case SingleClick: resolveShiftClick(x, y); repaint(); break;
        case DoubleClick: resolveWordDrag(x, y); repaint(); break;
        case TripleClick: resolveLineDrag(y); repaint(); break;
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

    repaint();
}

void View::resolveClick(double x, double y) noexcept{
    Line* l = model->nearestLine(y);
    controller.clickTo(l, x, y);
    updateXSetpoint();
}

void View::resolveShiftClick(double x, double y) noexcept{
    Phrase* p = controller.isTopLevel() ? model->nearestLine(y) : controller.phrase();
    controller.shiftClick(p, x);
    updateXSetpoint();
}

void View::resolveRightClick(double x, double y, int xScreen, int yScreen){
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
    Line* l = model->nearestLine(y);
    controller.selectLine(l);
    updateXSetpoint();

    invalidateDoubleClickTime();
}

void View::resolveLineDrag(double y) noexcept{
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

    updateXSetpoint();
}

void View::resolveTooltip(double x, double y) noexcept{
    if(model->is_output) return; //EVENTUALLY: find a better way to have different behaviours

    for(const Code::Error& err : model->errors){
        if(err.selection.containsWithEmptyMargin(x, y)){
            setTooltipError(err.message());
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
            const auto& symbol = symbol_table.symbols[lookup->second];
            QString tooltip = "<b>" + QString::fromStdString(c.selectedText()) + "</b> : "
                    + QString::fromStdString(model->type_resolver.typeString(symbol.type));
            if(symbol.comment != Code::ParseTree::EMPTY)
                tooltip += "<div style=\"color:green\">" + QString::fromStdString(symbol_table.parse_tree.str(symbol.comment));
            setToolTip(tooltip);
            return;
        }
    }

    clearTooltip();
}

void View::resolveSelectionDrag(double x, double y){
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

void View::updateXSetpoint(){
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
    zoom = std::min(ZOOM_MAX, zoom*ZOOM_DELTA);
}

void View::zoomOut() noexcept{
    zoom = std::max(ZOOM_MIN, zoom/ZOOM_DELTA);
}

void View::resetZoom() noexcept{
    zoom = ZOOM_DEFAULT;
}

void View::restartCursorBlink() noexcept{
    if(!allow_write) return;
    show_cursor = true;
    cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
}

void View::stopCursorBlink(){
    show_cursor = false;
    repaint();
    cursor_blink_timer->stop();
}

void View::updateHighlighting(){
    highlighted_words.clear();

    Controller c = model->idAt(controller.active);
    if(c.hasSelection()){
        auto& symbol_table = model->symbol_builder.symbol_table;
        symbol_table.getSymbolOccurences(c.getAnchor(), highlighted_words);
    }
}

bool View::scrolledToBottom() const noexcept{
    return v_scroll->value() == v_scroll->maximum();
}

void View::scrollToBottom(){
    v_scroll->setValue(v_scroll->maximum());
    repaint();
}

bool View::lineNumbersShown() const noexcept{
    return show_line_nums;
}

void View::ensureCursorVisible(){
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

void View::followLink(size_t line_num){
    if(line_num > model->lines.size()) return;
    controller.setBothToBackOf(model->lines[line_num]->back());
    ensureCursorVisible();
    restartCursorBlink();
    repaint();
}

double View::xOrigin() const{
    return getLineboxWidth() + MARGIN_LEFT - h_scroll->value()/zoom;
}

double View::yOrigin() const{
    return MARGIN_TOP - v_scroll->value()/zoom;
}

double View::getLineboxWidth() const noexcept{
    return show_line_nums*LINEBOX_WIDTH;
}

void View::recommend(){
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
    constexpr int Ctrl = Qt::ControlModifier;
    constexpr int Shift = Qt::ShiftModifier;
    constexpr int CtrlShift = Qt::ControlModifier | Qt::ShiftModifier;

    bool hide_recommender = true;

    switch (e->key() | e->modifiers()) {
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
        case Qt::Key_Left|Shift: controller.selectPrevChar(); updateXSetpoint(); restartCursorBlink(); update(); repaint(); break;
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
        case Qt::Key_Delete: if(allow_write){
            controller.del();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Delete|Ctrl: if(allow_write){
            controller.delWord();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Backspace: if(allow_write){
            controller.backspace();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Backspace|Ctrl: if(allow_write){
            controller.backspaceWord();
            updateXSetpoint();
            restartCursorBlink();
        }
            break;
        case Qt::Key_Return: if(focusWidget() != this) return; if(allow_write) controller.newline(); updateXSetpoint(); restartCursorBlink(); break;
        case Qt::Key_Return|Shift: if(allow_write) controller.newline(); updateXSetpoint(); restartCursorBlink(); break;
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
            std::string str = e->text().toStdString();
            if(!allow_write || str.empty() || e->modifiers().testFlag(Qt::ControlModifier)) return;

            if(insert_mode) controller.selectNextChar();
            controller.keystroke(str);
            updateXSetpoint();
            restartCursorBlink();
            recommend();
            hide_recommender = false;
    }

    if(hide_recommender && recommender->isVisible()){
        recommender->hide();
        setFocus();
    }
    ensureCursorVisible();
    updateHighlighting();
    repaint();

    emit textChanged();
}

void View::mousePressEvent(QMouseEvent* e){
    if(focusWidget() != this) return;

    double click_x = xModel(e->x());
    double click_y = yModel(e->y());
    bool right_click = e->buttons() == Qt::RightButton;
    bool shift_held = e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier);

    dispatchClick(click_x, click_y, e->globalX(), e->globalY(), right_click, shift_held);

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
        updateHighlighting();
        repaint();
    }else if(e->reason() == Qt::BacktabFocusReason){
        setFocus(Qt::BacktabFocusReason);
        if(allow_write) controller.detab();
        ensureCursorVisible();
        updateHighlighting();
        repaint();
    }else{
        stopCursorBlink();
        assert(mouse_hold_state == Hover);
    }

    if(focusWidget() != recommender) recommender->hide();
}

void View::resizeEvent(QResizeEvent* e){
    QWidget::resizeEvent(e);
    handleResize();
}

void View::onBlink() noexcept{
    show_cursor = !show_cursor;
    repaint();
    cursor_blink_timer->start(CURSOR_BLINK_INTERVAL);
}

void View::copy() const{
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(QString::fromStdString(str));
}

void View::cut(){
    if(!controller.hasSelection()) return;
    std::string str = controller.selectedText();
    QGuiApplication::clipboard()->setText(QString::fromStdString(str));
    controller.del();

    emit textChanged();
}

void View::paste(){
    std::string str = QGuiApplication::clipboard()->text().toStdString();
    model->mutate(controller.getInsertSerial(str), controller);

    emit textChanged();
}

void View::setCursorAppearance(double x, double y){
    if(mouse_hold_state != SelectionDrag){
        Construct* con = model->constructAt(x, y);
        if(con && con->constructCode() == MARKERLINK){
            QWidget::setCursor(Qt::PointingHandCursor);
        }else{
            QWidget::setCursor(isInLineBox(x) ? Qt::ArrowCursor : Qt::IBeamCursor);
        }
    }
}

void View::drawBackground(const QRect& rect){
    QPainter p(this);
    p.fillRect(rect, isEnabled() ? background_color : disabled_background_color);
}

void View::drawModel(double xL, double yT, double xR, double yB){
    QPainter qpainter(this);
    Painter painter(qpainter);
    painter.setZoom(zoom);
    painter.setOffset(xOrigin(), yOrigin());

    for(const Code::Error& e : model->errors)
        e.selection.paintError(painter);

    for(const Selection& c : highlighted_words)
        c.paintHighlight(painter);

    const Typeset::Marker& cursor = getController().active;

    model->paint(painter, xL, yT, xR, yB);
    model->paintGroupings(painter, cursor);
    controller.paintSelection(painter);
    if(show_cursor){
        if(insert_mode) controller.paintInsertCursor(painter);
        else controller.paintCursor(painter);
    }
}

void View::drawLinebox(double yT, double yB){
    if(!show_line_nums) return;

    QPainter p(this);
    p.setBrush(line_box_fill_color);
    p.setPen(line_box_border_color);
    p.drawRect(-1, -1, 1+LINEBOX_WIDTH*zoom, height()+2);

    QPainter qpainter(this);
    Painter painter(qpainter);
    painter.setZoom(zoom);
    painter.setOffset(LINEBOX_WIDTH - LINE_NUM_OFFSET, yOrigin() + 3);

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
        double y = l->y + l->above_center;
        size_t n = l->id+1;
        bool active = (l->id >= iL) & (l->id <= iR);
        painter.drawLineNumber(y, n, active);
    }
}

void View::fillInScrollbarCorner(){
    QPainter p(this);
    int x = h_scroll->width();
    int y = v_scroll->height();
    int w = v_scroll->width();
    int h = h_scroll->height();
    QBrush brush = v_scroll->palette().window();
    p.fillRect(x, y, w, h, brush);
}

void View::handleResize(){
    QSize sze = size();
    int display_width = sze.width() - v_scroll->width();
    int model_display_width = display_width - (getLineboxWidth() + MARGIN_LEFT + MARGIN_RIGHT)*zoom;
    v_scroll->move(display_width, 0);
    h_scroll->setFixedWidth(display_width);

    double model_width = model->width()*zoom;
    double model_heigth = (model->height() + MARGIN_TOP + MARGIN_BOT)*zoom;

    double w_diff_scrn = model_width - model_display_width;
    bool h_scroll_enabled = w_diff_scrn > 0;
    h_scroll->setVisible(h_scroll_enabled);
    int display_height = sze.height() - h_scroll_enabled*h_scroll->height();

    if(h_scroll_enabled){
        h_scroll->setPageStep(model_display_width);
        h_scroll->setMaximum(w_diff_scrn);
        h_scroll->move(0, display_height);
    }

    v_scroll->setFixedHeight(display_height);
    double h_diff_scrn = model_heigth - display_height;
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

void View::setTooltipError(const std::string& str){
    setToolTipDuration(std::numeric_limits<int>::max());
    setToolTip("<b>Error</b><div style=\"color:red\">" + QString::fromStdString(str));
}

void View::clearTooltip(){
    setToolTipDuration(1);
    setToolTip(" ");
}

void View::undo(){
    model->undo(controller);
    updateHighlighting();
    recommender->hide();
    repaint();

    emit textChanged();
}

void View::redo(){
    model->redo(controller);
    updateHighlighting();
    recommender->hide();
    repaint();

    emit textChanged();
}

void View::selectAll() noexcept{
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

    Controller c = model->idAt(controller.active);
    assert(c.hasSelection());

    auto& symbol_table = model->symbol_builder.symbol_table;
    std::vector<Typeset::Selection> occurences;
    symbol_table.getSymbolOccurences(c.getAnchor(), occurences);
    rename(occurences, name);

    emit textChanged();
}

void View::goToDef(){
    Controller c = model->idAt(controller.active);
    assert(c.hasSelection());

    auto& symbol_table = model->symbol_builder.symbol_table;
    auto lookup = symbol_table.occurence_to_symbol_map.find(c.getAnchor());
    assert(lookup != symbol_table.occurence_to_symbol_map.end());

    controller = symbol_table.getSel(lookup->second);
    restartCursorBlink();
    ensureCursorVisible();
    repaint();
}

void View::findUsages(){
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
    controller.selectPrevWord();
    std::string to_insert = item->text().toStdString();
    if(to_insert != controller.selectedText())
        controller.insertSerial(item->text().toStdString());
    else
        controller.consolidateToAnchor();
    model->performSemanticFormatting();
    updateXSetpoint();
    updateModel();
    recommender->hide();
    QTimer::singleShot(0, this, SLOT(setFocus())); //Delay 1 cycle to avoid whatever input activated item

    emit textChanged();
}

void View::paintEvent(QPaintEvent* event){
    QRect r = event->rect();
    double xL = xModel(r.x());
    double yT = yModel(r.y());
    double xR = xModel(r.x() + r.width());
    double yB = yModel(r.y() + r.height());

    drawBackground(event->rect());
    drawModel(xL, yT, xR, yB);
    drawLinebox(yT, yB);
    fillInScrollbarCorner();
    QWidget::paintEvent(event);
}

QImage View::toPng() const{
    return controller.selection().toPng();
}

}

}
