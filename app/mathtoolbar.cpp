#include "mathtoolbar.h"

#include "typeset_keywords.h"
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

    std::pair<QString, QString> keywords[] = FORSCAPE_KEYWORD_LIST;
    for(auto pair : keywords){
        if(col >= symbol_table_cols){
            row++;
            col = 0;
        }

        QTableWidgetItem* item = new QTableWidgetItem(pair.second);
        item->setFlags(item->flags() & ~Qt::ItemFlag::ItemIsEditable);
        item->setToolTip('\\' + pair.first);
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
    addAction(new TypesetAction("A", "\\_", "", this));
    addAction(new TypesetAction("B", "\\^", "", this));
    addAction(new TypesetAction("C", "\\^_", "", this));
}

void MathToolbar::setupAccents(){
    QToolButton* accents = new QToolButton(this);
    accents->setPopupMode(QToolButton::MenuButtonPopup);
    connect(accents, SIGNAL(triggered(QAction*)), accents, SLOT(setDefaultAction(QAction*)));
    addWidget(accents);

    //accents->addAction(new EnclosedTypesetAction("J", "\\vec", "⁜→⏴", "", this));
    accents->addAction(new EnclosedTypesetAction("K", "\\bar", "/", "", this));
    accents->addAction(new EnclosedTypesetAction("L", "\\breve", "0", "", this));
    accents->addAction(new EnclosedTypesetAction("M", "\\dot", "1", "", this));
    accents->addAction(new EnclosedTypesetAction("N", "\\ddot", "2", "", this));
    accents->addAction(new EnclosedTypesetAction("O", "\\dddot", "3", "", this));
    accents->addAction(new EnclosedTypesetAction("P", "\\hat", "-", "", this));
    accents->addAction(new EnclosedTypesetAction("Q", "\\tilde", ".", "", this));

    accents->setDefaultAction(accents->actions().front());
}

void MathToolbar::setupMisc(){
    addAction(new EnclosedTypesetAction("D", "\\frac", "", "", this));
    addAction(new TypesetAction("E", "\\mat", "", this));
    addAction(new TypesetAction("F", "\\cases", "", this));
    addAction(new TypesetAction("G", "\\binom", "nk", this));
    addAction(new EnclosedTypesetAction("I", "\\sqrt", "", "", this));

    QToolButton* words = new QToolButton(this);
    words->setPopupMode(QToolButton::MenuButtonPopup);
    connect(words, SIGNAL(triggered(QAction*)), words, SLOT(setDefaultAction(QAction*)));
    addWidget(words);

    words->addAction(new TypesetAction("𐋏", "\\lim", "", this));
    words->addAction(new TypesetAction("Ħ", "\\max", "", this));
    words->addAction(new TypesetAction("ħ", "\\min", "", this));
    words->addAction(new TypesetAction("Ĥ", "\\sup", "", this));
    words->addAction(new TypesetAction("ĥ", "\\inf", "", this));

    words->setDefaultAction(words->actions().front());
}

void MathToolbar::setupBigSymbols(){
    QToolButton* bigs = new QToolButton(this);
    bigs->setPopupMode(QToolButton::MenuButtonPopup);
    connect(bigs, SIGNAL(triggered(QAction*)), bigs, SLOT(setDefaultAction(QAction*)));
    addWidget(bigs);

    bigs->addAction(new TypesetAction("∑", "\\sum2", "", this));
    bigs->addAction(new TypesetAction("∏", "\\prod2", "\t", this));
    bigs->addAction(new TypesetAction("∐", "\\coprod2", "\n", this));
    bigs->addAction(new TypesetAction("⋂", "\\intersection2", "", this));
    bigs->addAction(new TypesetAction("⋃", "\\union2", "", this));
    //bigs->addAction(new TypesetAction("⊎", "\\biguplus", "⁜⨄", this));

    bigs->setDefaultAction(bigs->actions().front());
}

void MathToolbar::setupIntegrals(){
    QToolButton* ints = new QToolButton(this);
    ints->setPopupMode(QToolButton::MenuButtonPopup);
    connect(ints, SIGNAL(triggered(QAction*)), ints, SLOT(setDefaultAction(QAction*)));
    addWidget(ints);

    ints->addAction(new TypesetAction("ģ", "\\int2", "\r", this));
    ints->addAction(new TypesetAction("Ģ", "\\iint1", "", this));
    ints->addAction(new TypesetAction("ġ", "\\iiint1", "", this));
    ints->addAction(new TypesetAction("Ġ", "\\oint2", "", this));
    ints->addAction(new TypesetAction("ę", "\\oiint1", "", this));
    ints->addAction(new TypesetAction("Ę", "\\oiiint1", " ", this));

    ints->setDefaultAction(ints->actions().front());
}

void MathToolbar::setupGroupings(){
    /*
    QToolButton* groups = new QToolButton(this);
    groups->setPopupMode(QToolButton::MenuButtonPopup);
    connect(groups, SIGNAL(triggered(QAction*)), groups, SLOT(setDefaultAction(QAction*)));
    addWidget(groups);

    groups->addAction(new EnclosedTypesetAction("‖⬚‖", "\\norm", "⁜‖⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("|⬚|", "\\abs", "⁜|⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("⌈⬚⌉", "\\ceil", "⁜⌈⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("⌊⬚⌋", "\\floor", "⁜⌊⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("(⬚)", "\\()", "⁜(⏴", "⏵", this));
    groups->addAction(new EnclosedTypesetAction("[⬚]", "\\[]", "⁜[⏴", "⏵", this));
    groups->addAction(new TypesetAction("R", "\\eval", "⁜┊⏴a⏵⏴b⏵", this));

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
