#ifndef TYPESET_VIEW_H
#define TYPESET_VIEW_H

#include "typeset_controller.h"
#include "typeset_painter.h"

#include <QListWidget>

class QScrollBar;

namespace Hope {

namespace Typeset {

class MarkerLink;
class Model;

class View : public Widget {
    Q_OBJECT

public:
    View();
    virtual ~View() override;
    void setFromSerial(const std::string& src);
    Model* getModel() const noexcept;
    void setModel(Model* m, bool owned = true);
    void runThread(View* output);
    void setLineNumbersVisible(bool show);
    Controller& getController() noexcept;
    void setReadOnly(bool read_only) noexcept;
    void replaceAll(const std::vector<Selection>& targets, const std::string& name);
    void ensureCursorVisible();
    void updateModel() noexcept;
    void zoomIn() noexcept;
    void zoomOut() noexcept;
    void resetZoom() noexcept;
    void updateHighlighting();
    bool scrolledToBottom() const noexcept;
    void scrollToBottom();
    bool lineNumbersShown() const noexcept;
    void insertText(const std::string& str);
    void insertSerial(const std::string& str);

    static QColor selection_box_color;
    static QColor selection_text_color;
    static QColor text_cursor_color;
    static QColor error_background_color;
    static QColor error_border_color;
    static QColor line_box_fill_color;
    static QColor line_box_border_color;
    static QColor line_num_active_color;
    static QColor line_num_passive_color;
    static QColor grouping_highlight_color;
    static QColor grouping_background_color;
    QColor background_color = Qt::white;
    QColor disabled_background_color = QPalette().color(QPalette::Disabled, QPalette::Window);

    std::vector<Typeset::Selection> highlighted_words;
    View* console = nullptr;

private:
    void dispatchClick(double x, double y, int xScreen, int yScreen, bool right_click, bool shift_held);
    void dispatchRelease(double x, double y);
    void dispatchDoubleClick(double x, double y);
    void dispatchHover(double x, double y);
    void dispatchMousewheel(bool ctrl_held, bool up);
    void resolveClick(double x, double y) noexcept;
    void resolveShiftClick(double x, double y) noexcept;
    void resolveRightClick(double x, double y, int xScreen, int yScreen);
    void resolveWordClick(double x, double y);
    void resolveWordDrag(double x, double y) noexcept;
    void resolveTripleClick(double y) noexcept;
    void resolveLineDrag(double y) noexcept;
    void resolveTooltip(double x, double y) noexcept;
    void resolveSelectionDrag(double x, double y);
    bool isInLineBox(double x) const noexcept;
    void drawModel(double xL, double yT, double xR, double yB);
    void updateXSetpoint();
    double xModel(double xScreen) const noexcept;
    double yModel(double yScreen) const noexcept;
    double xScreen(double xModel) const noexcept;
    double yScreen(double yModel) const noexcept;
    void restartCursorBlink() noexcept;
    void stopCursorBlink();
    void followLink(size_t line_num);
    double xOrigin() const;
    double yOrigin() const;
    double getLineboxWidth() const noexcept;
    void recommend();

    static constexpr double ZOOM_DEFAULT = 2.0;
    static constexpr double ZOOM_MAX = 5.0; static_assert(ZOOM_DEFAULT <= ZOOM_MAX);
    static constexpr double ZOOM_MIN = 0.75; static_assert(ZOOM_DEFAULT >= ZOOM_MIN);
    static constexpr double ZOOM_DELTA = 1.1; static_assert(ZOOM_DELTA > 1);
    static constexpr size_t CURSOR_BLINK_INTERVAL = 600;
    static constexpr bool ALLOW_SELECTION_DRAG = true;
    static constexpr double LINEBOX_WIDTH = 25;
    static constexpr double LINE_NUM_OFFSET = 5;
    static constexpr double MARGIN_LEFT = 10;
    static constexpr double MARGIN_RIGHT = 10;
    static constexpr double MARGIN_TOP = 10;
    static constexpr double MARGIN_BOT = 10;
    static constexpr double CURSOR_MARGIN = 10;

    QListWidget* recommender = new QListWidget(this);

    Model* model;
    Controller controller;
    double x_setpoint = 0;
    bool show_line_nums = true;
    bool allow_write = true;
    bool insert_mode = false;
    bool show_cursor = true;
    bool model_owned = true;
    friend MarkerLink;
    public: double zoom = ZOOM_DEFAULT;

//Qt specific code
protected:
    void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;
    virtual void keyPressEvent(QKeyEvent* e) override final;
    virtual void mousePressEvent(QMouseEvent* e) override final;
    virtual void mouseDoubleClickEvent(QMouseEvent* e) override final;
    virtual void mouseReleaseEvent(QMouseEvent* e) override final;
    virtual void mouseMoveEvent(QMouseEvent* e) override final;
    virtual void wheelEvent(QWheelEvent* e) override final;
    virtual void focusInEvent(QFocusEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
    virtual void resizeEvent(QResizeEvent* e) override;
    QTimer* cursor_blink_timer;
    void onBlink() noexcept;

private:
    void setCursorAppearance(double x, double y);
    void drawBackground(const QRect& rect);
    void drawLinebox(double yT, double yB);
    void fillInScrollbarCorner();
    void handleResize();
    void handleResize(int w, int h);
    void scrollUp();
    void scrollDown();
    void setTooltipError(const std::string& str);
    void clearTooltip();

    QScrollBar* v_scroll;
    QScrollBar* h_scroll;

signals:
    void textChanged();

public slots:
    void undo();
    void redo();
    void copy() const;
    void cut();
    void paste();
    void del();
    void selectAll() noexcept;

private slots:
    void rename();
    void goToDef();
    void findUsages();
    void takeRecommendation(QListWidgetItem* item);

private:
    void handleKey(int key, int modifiers, const std::string& str);
    void paste(const std::string& str);
    void rename(const std::string& str);
    void takeRecommendation(const std::string& str);
    std::string_view logPrefix() const noexcept;

public:
    QImage toPng() const;
};

}

}

#endif // TYPESET_VIEW_H
