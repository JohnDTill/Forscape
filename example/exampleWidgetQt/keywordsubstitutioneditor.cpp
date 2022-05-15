#include "keywordsubstitutioneditor.h"

#include <hope_common.h>
#include <typeset_keywords.h>
#include <typeset_view.h>
#include <cassert>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
using Hope::Typeset::Keywords;

class KeywordSubstitutionEditor::ModifiedLineEdit : public QLineEdit {
public:
    ModifiedLineEdit(QWidget* parent = nullptr)
        : QLineEdit(parent){}

protected:
    void focusInEvent(QFocusEvent* event) override final{
        KeywordSubstitutionLabel* label = debug_cast<KeywordSubstitutionLabel*>(parentWidget());
        KeywordSubstitutionEditor* editor = debug_cast<KeywordSubstitutionEditor*>(label->parentWidget());
        editor->focused_label = label;

        QLineEdit::focusInEvent(event);
    }
};

KeywordSubstitutionEditor::KeywordSubstitutionEditor(QSettings& settings, QWidget* parent)
    : QWidget(parent),
      form(new QFormLayout(this)),
      settings(settings),
      validator(validator = new QRegExpValidator(QRegExp("^[a-zA-Z0-9_]*"), this)) {
    setLayout(form);

    if(settings.contains("KEYWORD_SHORTCUTS")) load();
    populateWidgetFromMap();
}

KeywordSubstitutionEditor::~KeywordSubstitutionEditor(){
    QList<QVariant> list;
    for(const auto& entry : Keywords::map){
        list.append(QString::fromStdString(entry.first));
        list.append(QString::fromStdString(entry.second));
    }
    settings.setValue("KEYWORD_SHORTCUTS", list);
}

void KeywordSubstitutionEditor::resetDefaults(){
    Keywords::reset();
    for(auto item : form->children()) delete item;
    delete form;
    form = new QFormLayout(this);
    setLayout(form);
    populateWidgetFromMap();
}

void KeywordSubstitutionEditor::addSlot(){
    if(!getBottomEdit()->text().isEmpty()) addRowForEntry("", "");
    getBottomEdit()->setFocus();
}

void KeywordSubstitutionEditor::remove(){
    KeywordSubstitutionLabel* label = getButtonLabel();
    Keywords::map.erase(label->backup.toStdString());
    form->removeRow(label);
}

void KeywordSubstitutionEditor::updateKeyword(){
    KeywordSubstitutionLabel* label = focused_label;
    ModifiedLineEdit* edit = label->edit;
    const QString& new_keyword = edit->text();
    std::string keyword = new_keyword.toStdString();

    if(keyword.empty()){
        QMessageBox::warning(this, "Keyword rejected", "Keyword cannot be empty");
        edit->setText(label->backup);
    }else if(new_keyword == label->backup){
        return;
    }else if(Keywords::map.find(keyword) != Keywords::map.end()){
        QMessageBox::warning(this, "Keyword rejected", "Key \"" + new_keyword + "\" already exists");
        edit->setText(label->backup);
    }else{
        auto result = form->itemAt(form->indexOf(label) + 1);
        Keywords::map[keyword] = debug_cast<Hope::Typeset::LineEdit*>(result->widget())->toSerial();
        Keywords::map.erase(label->backup.toStdString());
        label->backup = new_keyword;
    }
}

void KeywordSubstitutionEditor::updateResult(){
    QWidget* sender = focusWidget();
    auto result_edit = debug_cast<Hope::Typeset::LineEdit*>(sender);
    auto upcast_label = form->labelForField(sender);
    KeywordSubstitutionLabel* label = debug_cast<KeywordSubstitutionLabel*>(upcast_label);
    Keywords::map[label->backup.toStdString()] = result_edit->toSerial();
}

void KeywordSubstitutionEditor::populateWidgetFromMap(){
    for(const auto& entry : getSortedMap())
        addRowForEntry(entry.first, entry.second);
}

void KeywordSubstitutionEditor::load(){
    assert(settings.contains("KEYWORD_SHORTCUTS"));
    auto loaded = settings.value("KEYWORD_SHORTCUTS").toList();
    assert(loaded.size()%2 == 0);

    Keywords::map.clear();
    for(int i = 0; i < loaded.size(); i += 2){
        std::string keyword = loaded[i].toString().toStdString();
        std::string result = loaded[i+1].toString().toStdString();

        auto op = Keywords::map.insert({keyword, result});
        assert(op.second); //Should not have saved with duplicates
    }
}

void KeywordSubstitutionEditor::addRowForEntry(const std::string& keyword, const std::string& result){
    auto result_edit = new Hope::Typeset::LineEdit();
    result_edit->setFromSerial(result, true);
    result_edit->setToolTip("Result");
    connect(result_edit, SIGNAL(textChanged()), this, SLOT(updateResult()));
    form->addRow(new KeywordSubstitutionLabel(this, keyword), result_edit);
}

std::vector<std::pair<std::string, std::string>> KeywordSubstitutionEditor::getSortedMap(){
    typedef std::pair<std::string, std::string> Entry;
    std::vector<Entry> entries;
    for(const auto& entry : Keywords::map)
        entries.push_back(entry);

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){return a.first < b.first;});

    return entries;
}

KeywordSubstitutionEditor::KeywordSubstitutionLabel* KeywordSubstitutionEditor::getButtonLabel() const{
    QWidget* sender = focusWidget();
    return debug_cast<KeywordSubstitutionLabel*>(sender->parentWidget());
}

KeywordSubstitutionEditor::ModifiedLineEdit* KeywordSubstitutionEditor::getBottomEdit() const{
    auto label = form->itemAt(form->count()-2)->widget();
    return debug_cast<KeywordSubstitutionLabel*>(label)->edit;
}

KeywordSubstitutionEditor::KeywordSubstitutionLabel::KeywordSubstitutionLabel(
        KeywordSubstitutionEditor* parent, const std::string& label)
    : QWidget(parent) {
    auto layout = new QHBoxLayout(this);
    setLayout(layout);
    auto* remove = new QPushButton(this);
    remove->setText("✕");
    remove->setFixedSize(20, 20);
    connect(remove, SIGNAL(clicked()), parent, SLOT(remove()));
    layout->addWidget(remove);
    edit = new ModifiedLineEdit(this);
    edit->setValidator(parent->validator);
    edit->setToolTip("Keyword");
    backup = QString::fromStdString(label);
    edit->setText(backup);
    connect(edit, SIGNAL(editingFinished()), parent, SLOT(updateKeyword()));
    layout->addWidget(edit);
}
