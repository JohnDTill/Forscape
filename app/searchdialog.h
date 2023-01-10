#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <typeset_selection.h>

class QSettings;

namespace Ui {
class SearchDialog;
}

namespace Forscape {
namespace Typeset {
    class View;
}
}

using namespace Forscape;

class SearchDialog : public QDialog{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget* parent, Typeset::View* in, Typeset::View* out, QSettings& settings);
    ~SearchDialog();
    void setWord(QString fill_word);
    void updateSelection();
    void saveSettings(QSettings& settings) const;

private slots:
    void on_closeButton_clicked();
    void on_findNextButton_clicked();
    void on_replaceAllButton_clicked();
    void on_findPrevButton_clicked();
    void on_replaceButton_clicked();
    void on_findEdit_textChanged(const QString& arg1);
    void on_replaceEdit_textChanged(const QString&);
    void on_findAllButton_clicked();
    void on_caseBox_stateChanged(int);
    void on_wordBox_stateChanged(int);
    void on_selectionBox_stateChanged(int);
    void populateHits();

private:
    virtual bool event(QEvent* event) override;
    virtual void closeEvent(QCloseEvent*) override;
    std::string searchStr() const;
    std::string replaceStr() const;
    void goToNext();
    void goToPrev();
    void populateHits(const std::string& str);
    void replace(const std::string& str);
    void replaceAll(const std::string& str);
    void updateInfo();

    Ui::SearchDialog* ui;
    Typeset::View* in;
    Typeset::View* out;
    Typeset::Selection sel;
    std::vector<Forscape::Typeset::Selection>& hits;
    size_t index = std::numeric_limits<size_t>::max();
};

#endif // SEARCHDIALOG_H
