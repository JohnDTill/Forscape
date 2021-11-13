#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <typeset_selection.h>

namespace Ui {
class SearchDialog;
}

namespace Hope {
namespace Typeset {
    class View;
}
}

using namespace Hope;

class SearchDialog : public QDialog{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget* parent,
                          Typeset::View* in,
                          Typeset::View* out,
                          QString fill_word);
    ~SearchDialog();

private slots:
    void on_closeButton_clicked();
    void on_findNextButton_clicked();
    void on_replaceAllButton_clicked();
    void on_findPrevButton_clicked();
    void on_replaceButton_clicked();
    void on_findEdit_textChanged(const QString &arg1);

    void on_findAllButton_clicked();

private:
    void populateHits();

    Ui::SearchDialog* ui;
    Typeset::View* in;
    Typeset::View* out;
    Typeset::Selection sel;
    std::vector<Hope::Typeset::Selection> hits;
    size_t index = std::numeric_limits<size_t>::max();
};

#endif // SEARCHDIALOG_H
