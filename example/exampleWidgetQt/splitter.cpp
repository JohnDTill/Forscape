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
    handle->setProperty("index", index);

    return handle;

    //return new SplitterHandle(orientation(), this);
}

void Splitter::emitDoubleClicked(QSplitterHandle* sender) const {
    emit splitterDoubleClicked(sender->property("index").toInt());

    //DO THIS: why doesn't this work?
    //int index = 0;
    //for(QSplitterHandle* h = handle(index); h != nullptr; h = handle(++index))
    //    if(h == sender) emit splitterDoubleClicked(index);
    //assert(false);
}
