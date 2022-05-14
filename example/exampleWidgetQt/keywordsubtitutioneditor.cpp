#include "keywordsubtitutioneditor.h"

#include <typeset_keywords.h>
#include <typeset_view.h>
#include <cassert>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>

class KeywordSubtitutionEditor::ModifiedLineEdit : public QLineEdit {
public:
    ModifiedLineEdit(QWidget* parent = nullptr)
        : QLineEdit(parent){}

protected:
    void focusInEvent(QFocusEvent* event) override final{
        KeywordSubstitutionLabel* label = static_cast<KeywordSubstitutionLabel*>(parentWidget());
        KeywordSubtitutionEditor* editor = static_cast<KeywordSubtitutionEditor*>(label->parentWidget());
        editor->focused_label = label;

        QLineEdit::focusInEvent(event);
    }
};

KeywordSubtitutionEditor::KeywordSubtitutionEditor(QSettings& settings, QWidget* parent)
    : QWidget(parent),
      form(new QFormLayout(this)),
      settings(settings),
      validator(validator = new QRegExpValidator(QRegExp("^[a-zA-Z0-9_]*"), this)) {
    setLayout(form);

    if(settings.contains("KEYWORD_SHORTCUTS")) load();
    else populateDefaults();
}

KeywordSubtitutionEditor::~KeywordSubtitutionEditor(){
    const auto to_save = getSortedMap();
    QMap<QString, QVariant> map;
    for(const auto& entry : to_save)
        map.insert(QString::fromStdString(entry.first), QString::fromStdString(entry.second));
    settings.setValue("KEYWORD_SHORTCUTS", map);
}

void KeywordSubtitutionEditor::resetDefaults(){
    Hope::Typeset::Keywords::reset();
    for(auto item : form->children()) delete item;
    delete form;
    form = new QFormLayout(this);
    setLayout(form);
    populateDefaults();
}

void KeywordSubtitutionEditor::addSlot(){
    if(!getBottomEdit()->text().isEmpty()) addRowForEntry("", "");
    getBottomEdit()->setFocus();
}

void KeywordSubtitutionEditor::remove(){
    KeywordSubstitutionLabel* label = getButtonLabel();
    Hope::Typeset::Keywords::map.erase(label->backup.toStdString());
    form->removeRow(label);
}

void KeywordSubtitutionEditor::updateKeyword(){
    KeywordSubstitutionLabel* label = focused_label;
    ModifiedLineEdit* edit = label->edit;
    const QString& new_keyword = edit->text();
    std::string keyword = new_keyword.toStdString();

    if(keyword.empty()){
        QMessageBox::warning(this, "Keyword rejected", "Keyword cannot be empty");
        edit->setText(label->backup);
    }else if(new_keyword == label->backup){
        return;
    }else if(Hope::Typeset::Keywords::map.find(keyword) != Hope::Typeset::Keywords::map.end()){
        QMessageBox::warning(this, "Keyword rejected", "Key \"" + new_keyword + "\" already exists");
        edit->setText(label->backup);
    }else{
        auto result = form->itemAt(form->indexOf(label) + 1);
        assert(dynamic_cast<Hope::Typeset::LineEdit*>(result->widget()));
        Hope::Typeset::Keywords::map[keyword] = static_cast<Hope::Typeset::LineEdit*>(result->widget())->toSerial();
        Hope::Typeset::Keywords::map.erase(label->backup.toStdString());
        label->backup = new_keyword;
    }
}

void KeywordSubtitutionEditor::updateResult(){
    QWidget* sender = focusWidget();
    assert(dynamic_cast<Hope::Typeset::LineEdit*>(sender));
    auto result_edit = static_cast<Hope::Typeset::LineEdit*>(sender);
    auto upcast_label = form->labelForField(sender);
    assert(dynamic_cast<KeywordSubstitutionLabel*>(upcast_label));
    KeywordSubstitutionLabel* label = static_cast<KeywordSubstitutionLabel*>(upcast_label);
    Hope::Typeset::Keywords::map[label->backup.toStdString()] = result_edit->toSerial();
}

void KeywordSubtitutionEditor::populateDefaults(){
    for(const auto& entry : getSortedMap())
        addRowForEntry(entry.first, entry.second);
}

void KeywordSubtitutionEditor::load(){
    assert(settings.contains("KEYWORD_SHORTCUTS"));
    auto map = settings.value("KEYWORD_SHORTCUTS").toMap();

    Hope::Typeset::Keywords::map.clear();
    for(auto it = map.constKeyValueBegin(); it != map.constKeyValueEnd(); it++){
        std::string keyword = it->first.toStdString();
        std::string result = it->second.toString().toStdString();

        auto op = Hope::Typeset::Keywords::map.insert({keyword, result});
        assert(op.second); //Should not have saved with duplicates
        addRowForEntry(keyword, result);
    }
}

void KeywordSubtitutionEditor::addRowForEntry(const std::string& keyword, const std::string& result){
    auto result_edit = new Hope::Typeset::LineEdit();
    result_edit->setFromSerial(result, true);
    result_edit->setToolTip("Result");
    connect(result_edit, SIGNAL(textChanged()), this, SLOT(updateResult()));
    form->addRow(new KeywordSubstitutionLabel(this, keyword), result_edit);
}

std::vector<std::pair<std::string, std::string>> KeywordSubtitutionEditor::getSortedMap(){
    typedef std::pair<std::string, std::string> Entry;
    std::vector<Entry> entries;
    for(const auto& entry : Hope::Typeset::Keywords::map)
        entries.push_back(entry);

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){return a.first < b.first;});

    return entries;
}

KeywordSubtitutionEditor::KeywordSubstitutionLabel* KeywordSubtitutionEditor::getButtonLabel() const{
    QWidget* sender = focusWidget();
    assert(dynamic_cast<KeywordSubstitutionLabel*>(sender->parentWidget()));
    return static_cast<KeywordSubstitutionLabel*>(sender->parentWidget());
}

KeywordSubtitutionEditor::ModifiedLineEdit* KeywordSubtitutionEditor::getBottomEdit() const{
    auto label = form->itemAt(form->count()-2)->widget();
    assert(dynamic_cast<KeywordSubstitutionLabel*>(label));
    return static_cast<KeywordSubstitutionLabel*>(label)->edit;
}

KeywordSubtitutionEditor::KeywordSubstitutionLabel::KeywordSubstitutionLabel(
        KeywordSubtitutionEditor* parent, const std::string& label)
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
