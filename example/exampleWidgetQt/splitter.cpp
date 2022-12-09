#include "splitter.h"

#include <QVariant>

SplitterHandle::SplitterHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent) {}

void SplitterHandle::mouseDoubleClickEvent(QMouseEvent* e) {
    QSplitterHandle::mouseDoubleClickEvent(e);
    static_cast<Splitter*>(splitter())->emitDoubleClicked(static_cast<QSplitterHandle*>(this));
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

void Splitter::emitDoubleClicked(QSplitterHandle* sender) const {
    emit splitterDoubleClicked(sender->toolTipDuration());
}
