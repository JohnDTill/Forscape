#include "splitter.h"

#include <QCoreApplication>
#include <QVariant>

SplitterHandle::SplitterHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent) {}

static bool allow_drag = true;

void SplitterHandle::mouseDoubleClickEvent(QMouseEvent* e) {
    static_cast<Splitter*>(splitter())->emitDoubleClicked(static_cast<QSplitterHandle*>(this));
    allow_drag = false;
    QWidget::setCursor(Qt::ArrowCursor);
}

void SplitterHandle::mouseMoveEvent(QMouseEvent* e) {
    if(allow_drag) QSplitterHandle::mouseMoveEvent(e);
}

void SplitterHandle::mouseReleaseEvent(QMouseEvent* e) {
    allow_drag = true;
    QWidget::setCursor(orientation() == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
}

Splitter::Splitter(Qt::Orientation orientation, QWidget* parent) noexcept
    : QSplitter(orientation, parent) {}

QSplitterHandle* Splitter::createHandle() {
    int index = 0;
    for(QSplitterHandle* h = handle(index); h != nullptr; h = handle(++index));
    SplitterHandle* handle = new SplitterHandle(orientation(), this);
    handle->setToolTipDuration(index);

    return handle;
}

void Splitter::emitDoubleClicked(QSplitterHandle* sender) {
    emit splitterDoubleClicked(sender->toolTipDuration());
}
