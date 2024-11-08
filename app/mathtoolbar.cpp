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
    setWindowTitle(tr("Typesetting toolbar"));

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

    addAction(new SettingsAction("Š", "\\settings", this));
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
    QAction* subscript = new EnclosedTypesetAction("A", SYNTAX_CMD "_", CONSTRUCT_STR SUBSCRIPT_STR OPEN_STR, CLOSE_STR, this);
    subscript->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus);
    addAction(subscript);
    QAction* superscript = new EnclosedTypesetAction("B", SYNTAX_CMD "^", CONSTRUCT_STR SUPERSCRIPT_STR OPEN_STR, CLOSE_STR, this);
    superscript->setShortcut(Qt::CTRL | Qt::Key_Plus);
    addAction(superscript);
    addAction(new TypesetAction("C", SYNTAX_CMD "^_", CONSTRUCT_STR DUALSCRIPT_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
}

void MathToolbar::setupAccents(){
    QToolButton* accents = new QToolButton(this);
    accents->setPopupMode(QToolButton::MenuButtonPopup);
    connect(accents, SIGNAL(triggered(QAction*)), accents, SLOT(setDefaultAction(QAction*)));
    addWidget(accents);

    accents->addAction(new EnclosedTypesetAction("K", SYNTAX_CMD "bar", CONSTRUCT_STR ACCENTBAR_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("L", SYNTAX_CMD "breve", CONSTRUCT_STR ACCENTBREVE_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("M", SYNTAX_CMD "dot", CONSTRUCT_STR ACCENTDOT_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("N", SYNTAX_CMD "ddot", CONSTRUCT_STR ACCENTDDOT_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("O", SYNTAX_CMD "dddot", CONSTRUCT_STR ACCENTDDDOT_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("P", SYNTAX_CMD "hat", CONSTRUCT_STR ACCENTHAT_STR OPEN_STR, CLOSE_STR, this));
    accents->addAction(new EnclosedTypesetAction("Q", SYNTAX_CMD "tilde", CONSTRUCT_STR ACCENTTILDE_STR OPEN_STR, CLOSE_STR, this));

    accents->setDefaultAction(accents->actions().front());
}

void MathToolbar::setupMisc(){
    addAction(new EnclosedTypesetAction("D", SYNTAX_CMD "frac", CONSTRUCT_STR FRACTION_STR OPEN_STR, CLOSE_STR CLOSE_STR, this));
    addAction(new TypesetAction("E", SYNTAX_CMD "mat",
        CONSTRUCT_STR "[2x2]" OPEN_STR CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR, this));
    addAction(new TypesetAction("F", SYNTAX_CMD "cases",
        CONSTRUCT_STR "{2" OPEN_STR CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR, this));
    addAction(new TypesetAction("G", SYNTAX_CMD "binom",
        CONSTRUCT_STR BINOMIAL_STR OPEN_STR "n" CLOSE_STR "k" CLOSE_STR, this));
    addAction(new EnclosedTypesetAction("I", SYNTAX_CMD "sqrt", CONSTRUCT_STR SQRT_STR OPEN_STR, CLOSE_STR, this));

    QToolButton* words = new QToolButton(this);
    words->setPopupMode(QToolButton::MenuButtonPopup);
    connect(words, SIGNAL(triggered(QAction*)), words, SLOT(setDefaultAction(QAction*)));
    addWidget(words);

    words->addAction(new TypesetAction("𐋏", SYNTAX_CMD "lim", CONSTRUCT_STR LIMIT_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    words->addAction(new EnclosedTypesetAction("Ħ", SYNTAX_CMD "max", CONSTRUCT_STR MAX_STR OPEN_STR, CLOSE_STR, this));
    words->addAction(new EnclosedTypesetAction("ħ", SYNTAX_CMD "min", CONSTRUCT_STR MIN_STR OPEN_STR, CLOSE_STR, this));
    words->addAction(new EnclosedTypesetAction("Ĥ", SYNTAX_CMD "sup", CONSTRUCT_STR SUP_STR OPEN_STR, CLOSE_STR, this));
    words->addAction(new EnclosedTypesetAction("ĥ", SYNTAX_CMD "inf", CONSTRUCT_STR INF_STR OPEN_STR, CLOSE_STR, this));

    words->setDefaultAction(words->actions().front());
}

void MathToolbar::setupBigSymbols(){
    QToolButton* bigs = new QToolButton(this);
    bigs->setPopupMode(QToolButton::MenuButtonPopup);
    connect(bigs, SIGNAL(triggered(QAction*)), bigs, SLOT(setDefaultAction(QAction*)));
    addWidget(bigs);

    bigs->addAction(new TypesetAction("∑", SYNTAX_CMD "sum2", CONSTRUCT_STR BIGSUM2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    bigs->addAction(new TypesetAction("∏", SYNTAX_CMD "prod2", CONSTRUCT_STR BIGPROD2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    bigs->addAction(new TypesetAction("∐", SYNTAX_CMD "coprod2", CONSTRUCT_STR BIGCOPROD2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    bigs->addAction(new TypesetAction("⋂", SYNTAX_CMD "intersection2",
        CONSTRUCT_STR BIGINTERSECTION2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    bigs->addAction(new TypesetAction("⋃", SYNTAX_CMD "union2", CONSTRUCT_STR BIGUNION2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));

    bigs->setDefaultAction(bigs->actions().front());
}

void MathToolbar::setupIntegrals(){
    QToolButton* ints = new QToolButton(this);
    ints->setPopupMode(QToolButton::MenuButtonPopup);
    connect(ints, SIGNAL(triggered(QAction*)), ints, SLOT(setDefaultAction(QAction*)));
    addWidget(ints);

    ints->addAction(new TypesetAction("ģ", SYNTAX_CMD "int2", CONSTRUCT_STR INTEGRAL2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    ints->addAction(new TypesetAction("Ģ", SYNTAX_CMD "iint1", CONSTRUCT_STR DOUBLEINTEGRAL1_STR OPEN_STR CLOSE_STR, this));
    ints->addAction(new TypesetAction("ġ", SYNTAX_CMD "iiint1", CONSTRUCT_STR TRIPLEINTEGRAL1_STR OPEN_STR CLOSE_STR, this));
    ints->addAction(new TypesetAction("Ġ", SYNTAX_CMD "oint2",
        CONSTRUCT_STR INTEGRALCONV2_STR OPEN_STR CLOSE_STR CLOSE_STR, this));
    ints->addAction(new TypesetAction("ę", SYNTAX_CMD "oiint1",
        CONSTRUCT_STR DOUBLEINTEGRALCONV1_STR OPEN_STR CLOSE_STR, this));
    ints->addAction(new TypesetAction("Ę", SYNTAX_CMD "oiiint1",
        CONSTRUCT_STR TRIPLEINTEGRALCONV1_STR OPEN_STR CLOSE_STR, this));

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

void MathToolbar::insertSettingsSlot() {
    emit insertSettings();
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

MathToolbar::SettingsAction::SettingsAction(const QString& text, const QString& tooltip, MathToolbar* parent)
    : QAction(text, parent) {
    setToolTip(tooltip);
    setFont(glyph_font);
    connect(this, SIGNAL(triggered(bool)), parent, SLOT(insertSettingsSlot()));
}
