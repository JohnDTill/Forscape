#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>

class SplitterHandle : public QSplitterHandle {
public:
    SplitterHandle(Qt::Orientation orientation, QSplitter* parent = nullptr);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* e) override final;
    virtual void mouseMoveEvent(QMouseEvent* e) override final;
    virtual void mouseReleaseEvent(QMouseEvent* e) override final;
};

class Splitter : public QSplitter {
    Q_OBJECT

public:
    Splitter(Qt::Orientation orientation, QWidget* parent = nullptr) noexcept;

signals:
    void splitterDoubleClicked(int index);

protected:
    QSplitterHandle* createHandle() override final;
    friend SplitterHandle;
    void emitDoubleClicked(QSplitterHandle* sender);
};

#endif // SPLITTER_H
