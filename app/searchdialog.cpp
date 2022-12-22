#include "searchdialog.h"
#include "ui_searchdialog.h"

#include <mainwindow.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>
#include <typeset_phrase.h>
#include <typeset_text.h>
#include <typeset_view.h>
#include <QBitArray>

#ifndef NDEBUG
#include <iostream>
#endif

#define SEARCH_SETTINGS "search_pref"
enum Setting {
    CaseSensitive,
    WordOnly,
    InSelection,
};

SearchDialog::SearchDialog(QWidget* parent, Typeset::View* in, Typeset::View* out, QSettings& settings) :
    QDialog(parent, Qt::WindowSystemMenuHint), ui(new Ui::SearchDialog), in(in), out(out), hits(in->highlighted_words) {
    ui->setupUi(this);
    setWindowFlag(Qt::WindowCloseButtonHint);

    if(settings.contains(SEARCH_SETTINGS)){
        auto saved = settings.value(SEARCH_SETTINGS).toBitArray();
        ui->caseBox->setChecked(saved[CaseSensitive]);
        ui->wordBox->setChecked(saved[WordOnly]);
        ui->selectionBox->setChecked(saved[InSelection]);
    }

    setWindowTitle("Search / Replace");
    setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
    resize(width(), 1);

    //EVENTUALLY: support multi-file search, e.g. find in project
}

SearchDialog::~SearchDialog(){
    delete ui;
}

void SearchDialog::setWord(QString fill_word){
    ui->findEdit->setText(fill_word);
    populateHits();
}

void SearchDialog::updateSelection(){
    sel = in->getController().selection();
    in->search_selection = sel;
    bool has_sel = !sel.isEmpty();
    ui->selectionBox->setVisible(has_sel);
    ui->selectionOnlyLabel->setVisible(has_sel);
}

void SearchDialog::saveSettings(QSettings& settings) const{
    QBitArray to_save(3);
    to_save[CaseSensitive] = ui->caseBox->isChecked();
    to_save[WordOnly] = ui->wordBox->isChecked();
    to_save[InSelection] = ui->selectionBox->isChecked();
    settings.setValue(SEARCH_SETTINGS, to_save);
}

void SearchDialog::on_closeButton_clicked(){
    accept();
}


void SearchDialog::on_findNextButton_clicked(){
    goToNext();
}


void SearchDialog::on_findPrevButton_clicked(){
    goToPrev();
}

void SearchDialog::on_replaceAllButton_clicked(){
    replaceAll(replaceStr());
}

void SearchDialog::on_replaceButton_clicked(){
    if(!hits.empty()) replace(replaceStr());
}

void SearchDialog::populateHits(){
    populateHits(searchStr());
}

bool SearchDialog::event(QEvent* event){
    if(event->type() == QEvent::WindowActivate) populateHits();
    return QDialog::event(event);
}

void SearchDialog::closeEvent(QCloseEvent*){
    in->search_selection.left = in->search_selection.right;
    hide();
}

void SearchDialog::populateHits(const std::string& str){
    hits.clear();
    in->getController().deselect();
    if(!str.empty()){
        bool use_sel = ui->selectionBox->isVisible() && ui->selectionBox->isChecked();
        bool use_case = ui->caseBox->isChecked();
        bool word = ui->wordBox->isChecked();
        Typeset::Model& m = *in->getModel();

        if(use_sel) sel.search(str, hits, use_case, word);
        else m.search(str, hits, use_case, word);

        index = 0;
        if(!hits.empty() && hasFocus()){
            in->getController() = hits[index];
            in->ensureCursorVisible();
        }
    }

    updateInfo();
    in->updateAfterHighlightChange();
}

void SearchDialog::replace(const std::string& str){
    if(index >= hits.size()) index = 0;

    in->getController() = hits[index];
    in->getController().insertText(str);

    size_t backup = index-1;
    populateHits(); //Repopulate instead of erase because maybe substitution causes hits
    index = backup;
    goToNext();
}

void SearchDialog::replaceAll(const std::string& str){
    in->replaceAll(hits, str);
    populateHits();
}

void SearchDialog::updateInfo(){
    ui->infoLabel->setText(
        ui->findEdit->text().isEmpty() ?
        "" :
        hits.empty() ?
        "No hits" :
        QString::number(index+1) + " / " + QString::number(hits.size())
    );
}

void SearchDialog::on_findEdit_textChanged(const QString&){
    populateHits();
}

void SearchDialog::on_findAllButton_clicked(){
    out->setFromSerial("");
    std::string search_str = searchStr();

    Typeset::Model* m = out->getModel();
    if(search_str.empty()){
        m->lastText()->setString("No input for search");
    }else{
        static constexpr std::string_view lead = "Search results for \"";
        m->lastText()->setString(lead.data() + search_str + "\":");
        m->lastText()->tags.push_back( SemanticTag(lead.size()-1, SEM_STRING) );
        m->lastText()->tags.push_back( SemanticTag(lead.size()+search_str.size()+1, SEM_DEFAULT) );

        if(hits.empty()){
            m->appendLine();
            Typeset::Text* t = m->lastText();
            t->setString("NO RESULTS");
            t->tags.push_back( SemanticTag(0, SEM_ERROR) );
        }

        for(const Typeset::Selection& sel : hits){
            m->appendLine();
            Typeset::Text* t = m->lastText();
            t->getParent()->appendConstruct( new Typeset::MarkerLink(sel.getStartLine(), in, in->getModel()) );
        }
    }

    out->getController().moveToStartOfDocument();
    out->updateModel();
}

std::string SearchDialog::searchStr() const{
    return ui->findEdit->text().toStdString();
}

std::string SearchDialog::replaceStr() const{
    return ui->replaceEdit->text().toStdString();
}

void SearchDialog::goToNext(){
    if(!hits.empty()){
        if(++index >= hits.size()) index = 0;
        updateInfo();
        in->getController() = hits[index];
        in->ensureCursorVisible();
        in->update();
    }
}

void SearchDialog::goToPrev(){
    if(!hits.empty()){
        if(index-- == 0) index = hits.size()-1;
        updateInfo();
        in->getController() = hits[index];
        in->ensureCursorVisible();
        in->update();
    }
}

void SearchDialog::on_caseBox_stateChanged(int){
    populateHits();
}

void SearchDialog::on_wordBox_stateChanged(int){
    populateHits();
}

void SearchDialog::on_selectionBox_stateChanged(int){
    populateHits();
}

