#include "searchdialog.h"
#include "ui_searchdialog.h"

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
    accept();
}


void SearchDialog::on_findNextButton_clicked(){
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
    if(!hits.empty()){
        index--;
        if(index >= hits.size()) index = hits.size()-1;
        in->getController() = hits[index];
        in->ensureCursorVisible();
    }
    in->highlighted_words = hits;
    in->repaint();
}


void SearchDialog::on_replaceAllButton_clicked(){
    QString search_word = ui->findEdit->text();
    std::string target = search_word.toStdString();
    QString replace_word = ui->replaceEdit->text();
    std::string replace = replace_word.toStdString();

    in->rename(hits, replace);
    hits.clear();
}


void SearchDialog::on_replaceButton_clicked(){
    if(hits.empty()) return;
    if(index >= hits.size()) index = 0;

    QString replace_word = ui->replaceEdit->text();
    std::string replace = replace_word.toStdString();

    in->getController() = hits[index];
    in->getController().insertText(replace);

    populateHits();
    index--;
    on_findNextButton_clicked();
}

void SearchDialog::populateHits(){
    QString word = ui->findEdit->text();
    if(word.isEmpty()){
        hits.clear();
    }else{
        std::string str = word.toStdString();

        hits = ui->selectionBox->isChecked() ?
               sel.findCaseInsensitive(str) :
               in->getModel()->findCaseInsensitive(str);

        index = std::numeric_limits<size_t>::max();
    }
}


void SearchDialog::on_findEdit_textChanged(const QString&){
    populateHits();
    in->highlighted_words = hits;
    in->repaint();
}


void SearchDialog::on_findAllButton_clicked(){
    QString search_word = ui->findEdit->text();
    out->setFromSerial("");

    Typeset::Model* m = out->getModel();
    if(search_word.isEmpty()){
        m->lastText()->str = "No input for search";
    }else{
        std::string word = search_word.toStdString();
        static constexpr std::string_view lead = "Search results for \"";
        m->lastText()->str = lead.data() + word + "\":";
        m->lastText()->tags.push_back( SemanticTag(lead.size()-1, SEM_STRING) );
        m->lastText()->tags.push_back( SemanticTag(lead.size()+word.size()+1, SEM_DEFAULT) );

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

