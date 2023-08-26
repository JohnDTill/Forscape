#include "projectbrowser.h"

ProjectBrowser::ProjectBrowser(QWidget* parent)
    : QTreeWidget(parent) {
    setHeaderHidden(true);
    setIndentation(10);
    setMinimumWidth(120);
    setRootIsDecorated(false);
}
