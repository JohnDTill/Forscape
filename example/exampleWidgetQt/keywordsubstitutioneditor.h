#ifndef KEYWORDSUBSTITUTIONEDITOR_H
#define KEYWORDSUBSTITUTIONEDITOR_H

#include <QWidget>

class QFormLayout;
class QSettings;
class QValidator;

class KeywordSubstitutionEditor : public QWidget{
    Q_OBJECT

public:
    KeywordSubstitutionEditor(QSettings& settings, QWidget* parent = nullptr);
    ~KeywordSubstitutionEditor();
    void resetDefaults();
    void addSlot();

private slots:
    void remove();
    void updateKeyword();
    void updateResult();

private:
    class ModifiedLineEdit;
    class KeywordSubstitutionLabel : public QWidget {
    public:
        ModifiedLineEdit* edit;
        QString backup;

        KeywordSubstitutionLabel(KeywordSubstitutionEditor* parent = nullptr, const std::string& label = "");
    };

    KeywordSubstitutionLabel* getButtonLabel() const;
    ModifiedLineEdit* getBottomEdit() const;
    void populateWidgetFromMap();
    void load();
    void addRowForEntry(const std::string& keyword, const std::string& result);
    static std::vector<std::pair<std::string, std::string>> getSortedMap();

    QFormLayout* form;
    QSettings& settings;
    QValidator* validator;
    KeywordSubstitutionLabel* focused_label = nullptr;
};

#endif // KEYWORDSUBSTITUTIONEDITOR_H
