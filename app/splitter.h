#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>

class SplitterHandle : public QSplitterHandle {
public:
    SplitterHandle(Qt::Orientation orientation, QSplitter* parent = nullptr);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* e) override final;
};

class Splitter : public QSplitter {
    Q_OBJECT

public:
    Splitter(Qt::Orientation orientation, QWidget* parent = nullptr) noexcept;

signals:
    void splitterDoubleClicked(int index) const;

protected:
    QSplitterHandle* createHandle() override final;
    friend SplitterHandle;
    void emitDoubleClicked(QSplitterHandle* sender) const;
};

#endif // SPLITTER_H
