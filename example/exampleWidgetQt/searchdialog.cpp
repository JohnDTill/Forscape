#include "searchdialog.h"
#include "ui_searchdialog.h"

#include <hope_logging.h>
#include <typeset_markerlink.h>
#include <typeset_model.h>
#include <typeset_phrase.h>
#include <typeset_text.h>
#include <typeset_view.h>

#ifndef NDEBUG
#include <iostream>
#endif

SearchDialog::SearchDialog(QWidget* parent, Typeset::View* in, Typeset::View* out, QString fill_word) :
    QDialog(parent, Qt::WindowSystemMenuHint), ui(new Ui::SearchDialog), in(in), out(out){
    ui->setupUi(this);
    sel = in->getController().selection();
    ui->findEdit->setText(fill_word);
    setWindowFlag(Qt::WindowCloseButtonHint);

    setWindowTitle("Search / Replace");
    setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
    resize(width(), 1);
}

SearchDialog::~SearchDialog(){
    delete ui;

    in->highlighted_words.clear();
    in->repaint();
}

void SearchDialog::on_closeButton_clicked(){
    logger->info("on_closeButton_clicked()");

    accept();
}


void SearchDialog::on_findNextButton_clicked(){
    logger->info("on_findNextButton_clicked()");

    if(!hits.empty()){
        index++;
        if(index >= hits.size()) index = 0;
        in->getController() = hits[index];
        in->ensureCursorVisible();
    }
    in->highlighted_words = hits;
    in->repaint();
}


void SearchDialog::on_findPrevButton_clicked(){
    logger->info("on_findPrevButton_clicked()");
    goToNext();
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

void SearchDialog::populateHits(const std::string& str){
    logger->info("populateHits({:s})", cStr(str));

    if(str.empty()){
        hits.clear();
    }else{
        hits = ui->selectionBox->isChecked() ?
               sel.findCaseInsensitive(str) :
               in->getModel()->findCaseInsensitive(str);

        index = std::numeric_limits<size_t>::max();
    }
}

void SearchDialog::replace(const std::string& str){
    logger->info("replace({:s})", cStr(str));

    if(index >= hits.size()) index = 0;

    in->getController() = hits[index];
    in->getController().insertText(str);

    populateHits();
    index--;
    goToNext();
}

void SearchDialog::replaceAll(const std::string& str){
    logger->info("replaceAll({:s})", cStr(str));

    in->replaceAll(hits, str);
    hits.clear();
}

void SearchDialog::on_findEdit_textChanged(const QString&){
    populateHits();
    in->highlighted_words = hits;
    in->repaint();
}


void SearchDialog::on_findAllButton_clicked(){
    logger->info("on_findAllButton_clicked()");

    out->setFromSerial("");
    std::string search_str = searchStr();

    Typeset::Model* m = out->getModel();
    if(search_str.empty()){
        m->lastText()->str = "No input for search";
    }else{
        static constexpr std::string_view lead = "Search results for \"";
        m->lastText()->str = lead.data() + search_str + "\":";
        m->lastText()->tags.push_back( SemanticTag(lead.size()-1, SEM_STRING) );
        m->lastText()->tags.push_back( SemanticTag(lead.size()+search_str.size()+1, SEM_DEFAULT) );

        if(hits.empty()){
            m->appendLine();
            Typeset::Text* t = m->lastText();
            t->str = "NO RESULTS";
            t->tags.push_back( SemanticTag(0, SEM_ERROR) );
        }

        for(const Typeset::Selection& sel : hits){
            m->appendLine();
            Typeset::Text* t = m->lastText();
            t->getParent()->appendConstruct( new Typeset::MarkerLink(sel.getStartLine(), in) );
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
        index--;
        if(index >= hits.size()) index = hits.size()-1;
        in->getController() = hits[index];
        in->ensureCursorVisible();
    }
    in->highlighted_words = hits;
    in->repaint();
}

