#include "mathtoolbar.h"

#include "typeset_keywords.h"
#include "typeset_syntax.h"
#include <QFontDatabase>
#include <QHeaderView>
#include <QScrollBar>
#include <QTableWidget>
#include <QToolButton>

using namespace Forscape;
using namespace Typeset;

static QFont glyph_font;

MathToolbar::MathToolbar(QWidget* parent) : QToolBar(parent) {
    int id = QFontDatabase::addApplicationFont(":/fonts/toolbar_glyphs.otf");
    assert(id!=-1);
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    glyph_font = QFont(family);
    glyph_font.setPointSize(18);

    Qt::ToolBarAreas a;
    a.setFlag(Qt::ToolBarArea::TopToolBarArea);
    a.setFlag(Qt::ToolBarArea::BottomToolBarArea);
    setAllowedAreas(a);
    setupSymbolTable();

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    addAction(new TypesetAction("Š", "Insert a settings update", OPEN_STR SETTINGS_STR "", this));
    addSeparator();

    setupScripts();
    setupAccents();
    setupMisc();
    setupBigSymbols();
    setupIntegrals();
    setupGroupings();
}

void MathToolbar::setupSymbolTable(){
    //Set up dimensions
    static constexpr int symbol_table_cols = 12;
    static constexpr int VISIBLE_ROWS = 2;
    int row = 0;
    int col = 0;

    static constexpr int ENTRY_WIDTH = 24;

    //EVENTUALLY: the symbol table should react to changes in the map
    //            the codegenned keywords and size would then be unnecessary

    //Set properties
    symbol_table = new QTableWidget((FORSCAPE_NUM_KEYWORDS-1)/symbol_table_cols+1, symbol_table_cols, this);
    //QFont table_font = Globals::fonts[0];
    QFont table_font = QFontDatabase().font("JuliaMono", "Regular", ENTRY_WIDTH/VISIBLE_ROWS);
    symbol_table->setFont(table_font);
    symbol_table->setFocusPolicy(Qt::NoFocus);
    symbol_table->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    symbol_table->setMaximumHeight(ENTRY_WIDTH*VISIBLE_ROWS);
    symbol_table->verticalHeader()->setVisible(false);
    symbol_table->horizontalHeader()->setVisible(false);
    symbol_table->verticalHeader()->setMinimumSectionSize(5);
    symbol_table->horizontalHeader()->setMinimumSectionSize(5);
    symbol_table->verticalHeader()->setDefaultSectionSize(ENTRY_WIDTH);
    symbol_table->horizontalHeader()->setDefaultSectionSize(ENTRY_WIDTH);
    connect( symbol_table, SIGNAL(itemClicked(QTableWidgetItem*)),
             this, SLOT(symbolTableTriggered(QTableWidgetItem*)) );

    std::pair<QString, QString> keywords[] = {FORSCAPE_KEYWORD_LIST};
    for(auto pair : keywords){
        if(col >= symbol_table_cols){
            row++;
            col = 0;
        }

        QTableWidgetItem* item = new QTableWidgetItem(pair.second);
        item->setFlags(item->flags() & ~Qt::ItemFlag::ItemIsEditable);
        item->setToolTip(syntax_cmd + pair.first);
        symbol_table->setItem(row,col,item);
        col++;
    }

    //Make empty entries uneditable
    for(int i = col; i < symbol_table_cols; i++){
        QTableWidgetItem* item = new QTableWidgetItem("");
        item->setFlags(item->flags() & ~Qt::ItemFlag::ItemIsEditable);
        symbol_table->setItem(row,i,item);
    }

    symbol_table->setMinimumWidth(
                ENTRY_WIDTH*symbol_table_cols +
                symbol_table->verticalScrollBar()->width() + 2
                );

    addWidget(symbol_table);
}

void MathToolbar::setupScripts(){
    addAction(new TypesetAction("A", SYNTAX_CMD "_", OPEN_STR SUBSCRIPT_STR CLOSE_STR, this));
    addAction(new TypesetAction("B", SYNTAX_CMD "^", OPEN_STR SUPERSCRIPT_STR CLOSE_STR, this));
    addAction(new TypesetAction("C", SYNTAX_CMD "^_", OPEN_STR DUALSCRIPT_STR CLOSE_STR CLOSE_STR, this));
}

void MathToolbar::setupAccents(){
    QToolButton* accents = new QToolButton(this);
    accents->setPopupMode(QToolButton::MenuButtonPopup);
    connect(accents, SIGNAL(triggered(QAction*)), accents, SLOT(setDefaultAction(QAction*)));
    addWidget(accents);

    //accents->addAction(new EnclosedTypesetAction("J", SYNTAX_CMD "vec", "⁜→⏴", "", this));
    accents->addAction(new EnclosedTypesetAction("K", SYNTAX_CMD "bar", OPEN_STR ACCENTBAR_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("L", SYNTAX_CMD "breve", "0", "", this));
    accents->addAction(new EnclosedTypesetAction("M", SYNTAX_CMD "dot", "1", "", this));
    accents->addAction(new EnclosedTypesetAction("N", SYNTAX_CMD "ddot", "2", "", this));
    accents->addAction(new EnclosedTypesetAction("O", SYNTAX_CMD "dddot", "3", "", this));
    accents->addAction(new EnclosedTypesetAction("P", SYNTAX_CMD "hat", "-", "", this));
    accents->addAction(new EnclosedTypesetAction("Q", SYNTAX_CMD "tilde", ".", "", this));

    accents->setDefaultAction(accents->actions().front());
}

void MathToolbar::setupMisc(){
    addAction(new EnclosedTypesetAction("D", SYNTAX_CMD "frac", OPEN_STR FRACTION_STR, CLOSE_STR CLOSE_STR, this));
    addAction(new TypesetAction("E", SYNTAX_CMD "mat", "", this));
    addAction(new TypesetAction("F", SYNTAX_CMD "cases", "", this));
    addAction(new TypesetAction("G", SYNTAX_CMD "binom", "nk", this));
    addAction(new EnclosedTypesetAction("I", SYNTAX_CMD "sqrt", "", CLOSE_STR, this));

    QToolButton* words = new QToolButton(this);
    words->setPopupMode(QToolButton::MenuButtonPopup);
    connect(words, SIGNAL(triggered(QAction*)), words, SLOT(setDefaultAction(QAction*)));
    addWidget(words);

    words->addAction(new TypesetAction("𐋏", SYNTAX_CMD "lim", "", this));
    words->addAction(new TypesetAction("Ħ", SYNTAX_CMD "max", "", this));
    words->addAction(new TypesetAction("ħ", SYNTAX_CMD "min", "", this));
    words->addAction(new TypesetAction("Ĥ", SYNTAX_CMD "sup", "", this));
    words->addAction(new TypesetAction("ĥ", SYNTAX_CMD "inf", "", this));

    words->setDefaultAction(words->actions().front());
}

void MathToolbar::setupBigSymbols(){
    QToolButton* bigs = new QToolButton(this);
    bigs->setPopupMode(QToolButton::MenuButtonPopup);
    connect(bigs, SIGNAL(triggered(QAction*)), bigs, SLOT(setDefaultAction(QAction*)));
    addWidget(bigs);

    bigs->addAction(new TypesetAction("∑", SYNTAX_CMD "sum2", "", this));
    bigs->addAction(new TypesetAction("∏", SYNTAX_CMD "prod2", "\t", this));
    bigs->addAction(new TypesetAction("∐", SYNTAX_CMD "coprod2", "\n", this));
    bigs->addAction(new TypesetAction("⋂", SYNTAX_CMD "intersection2", "", this));
    bigs->addAction(new TypesetAction("⋃", SYNTAX_CMD "union2", "", this));
    //bigs->addAction(new TypesetAction("⊎", SYNTAX_CMD "biguplus", "⁜⨄", this));

    bigs->setDefaultAction(bigs->actions().front());
}

void MathToolbar::setupIntegrals(){
    QToolButton* ints = new QToolButton(this);
    ints->setPopupMode(QToolButton::MenuButtonPopup);
    connect(ints, SIGNAL(triggered(QAction*)), ints, SLOT(setDefaultAction(QAction*)));
    addWidget(ints);

    ints->addAction(new TypesetAction("ģ", SYNTAX_CMD "int2", "\r", this));
    ints->addAction(new TypesetAction("Ģ", SYNTAX_CMD "iint1", "", this));
    ints->addAction(new TypesetAction("ġ", SYNTAX_CMD "iiint1", "", this));
    ints->addAction(new TypesetAction("Ġ", SYNTAX_CMD "oint2", "", this));
    ints->addAction(new TypesetAction("ę", SYNTAX_CMD "oiint1", "", this));
    ints->addAction(new TypesetAction("Ę", SYNTAX_CMD "oiiint1", " ", this));

    ints->setDefaultAction(ints->actions().front());
}

void MathToolbar::setupGroupings(){
    /*
    QToolButton* groups = new QToolButton(this);
    groups->setPopupMode(QToolButton::MenuButtonPopup);
    connect(groups, SIGNAL(triggered(QAction*)), groups, SLOT(setDefaultAction(QAction*)));
    addWidget(groups);

    groups->addAction(new EnclosedTypesetAction("‖⬚‖", SYNTAX_CMD "norm", "⁜‖⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("|⬚|", SYNTAX_CMD "abs", "⁜|⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("⌈⬚⌉", SYNTAX_CMD "ceil", "⁜⌈⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("⌊⬚⌋", SYNTAX_CMD "floor", "⁜⌊⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("(⬚)", SYNTAX_CMD "()", "⁜(⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("[⬚]", SYNTAX_CMD "[]", "⁜[⏴", "⏵", this));
    groups->addAction(new TypesetAction("R", SYNTAX_CMD "eval", "⁜┊⏴a⏵⏴b⏵", this));

    groups->setDefaultAction(groups->actions().front());
    */
}

void MathToolbar::insertText(){
    emit insertSerial(static_cast<TypesetAction*>(sender())->command);
}

void MathToolbar::insertWithSelection(){
    EnclosedTypesetAction* a = static_cast<EnclosedTypesetAction*>(sender());
    emit insertSerialSelection(a->command_start, a->command_end);
}

void MathToolbar::symbolTableTriggered(QTableWidgetItem* item){
    if(item->text().isEmpty()) return;
    emit insertFlatText(item->text());
}

MathToolbar::TypesetAction::TypesetAction(const QString& text,
                                             const QString& tooltip,
                                             const QString& command,
                                             MathToolbar* parent)
    : QAction(text, parent), command(command) {
    setToolTip(tooltip);
    setFont(glyph_font);
    connect(this, SIGNAL(triggered(bool)), parent, SLOT(insertText()));
}

MathToolbar::EnclosedTypesetAction::EnclosedTypesetAction(const QString& text,
                                                             const QString& tooltip,
                                                             const QString& command_start,
                                                             const QString& command_end,
                                                             MathToolbar* parent)
    : QAction(text, parent), command_start(command_start), command_end(command_end) {
    setToolTip(tooltip);
    setFont(glyph_font);
    connect(this, SIGNAL(triggered(bool)), parent, SLOT(insertWithSelection()));
}
