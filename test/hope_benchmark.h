#include "report.h"
#include "typeset.h"
#include "hope_scanner.h"
#include "hope_serial.h"
#include "hope_parser.h"
#include "hope_symbol_build_pass.h"
#include "hope_interpreter.h"

using namespace Hope;
using namespace Code;

static constexpr size_t ITER_SERIAL_VALIDATION = 50000;
static constexpr size_t ITER_MODEL_LOAD_DELETE = 100;
static constexpr size_t ITER_SCANNER = 5000;
static constexpr size_t ITER_PARSER = 500;
static constexpr size_t ITER_SYMBOL_TABLE = 500;
static constexpr size_t ITER_INTERPRETER = 500;
static constexpr size_t ITER_CALC_SIZE = 30;
static constexpr size_t ITER_LAYOUT = 500;
static constexpr size_t ITER_PAINT = 500;
static constexpr size_t ITER_LOOP = 1;

void runBenchmark(){
    std::string src = readFile("../test/interpreter_scripts/in/root_finding_terse.txt");

    startClock();
    for(size_t i = 0; i < ITER_SERIAL_VALIDATION; i++)
        if(!isValidSerial(src)) exit(1);
    report("Serial validation", ITER_SERIAL_VALIDATION);

    startClock();
    for(size_t i = 0; i < ITER_MODEL_LOAD_DELETE; i++){
        Typeset::Model* m = Typeset::Model::fromSerial(src);
        delete m;
    }
    report("Load / delete", ITER_MODEL_LOAD_DELETE);

    Typeset::Model* m = Typeset::Model::fromSerial(src);
    Code::Scanner scanner(m);

    startClock();
    for(size_t i = 0; i < ITER_SCANNER; i++)
        scanner.scanAll();
    report("Scanner", ITER_SCANNER);

    Code::Parser parser(scanner, m);

    startClock();
    for(size_t i = 0; i < ITER_PARSER; i++)
        parser.parseAll();
    report("Parser", ITER_PARSER);

    startClock();
    for(size_t i = 0; i < ITER_SYMBOL_TABLE; i++){
        Code::ParseTree parse_tree = parser.parse_tree;
        Code::SymbolTableBuilder sym_table(parse_tree, m);
        sym_table.resolveSymbols();
    }
    report("SymbolTable", ITER_SYMBOL_TABLE);

    Code::ParseNode root = parser.parse_tree.back();
    Code::SymbolTableBuilder sym_table(parser.parse_tree, m);
    sym_table.resolveSymbols();
    Code::Interpreter interpreter;

    startClock();
    for(size_t i = 0; i < ITER_INTERPRETER; i++)
        interpreter.run(parser.parse_tree, sym_table.symbol_table, root);
    report("Interpreter", ITER_INTERPRETER);

    m = Typeset::Model::fromSerial(src);

    startClock();
    for(size_t i = 0; i < ITER_CALC_SIZE; i++)
        m->calculateSizes();
    report("Calculate size", ITER_CALC_SIZE);

    startClock();
    for(size_t i = 0; i < ITER_LAYOUT; i++)
        m->updateLayout();
    report("Update layout", ITER_LAYOUT);

    Typeset::View view;
    //view.show();

    view.resize(QSize(1920, 1080));
    QImage img(view.size(), QImage::Format_RGB32);
    QPainter painter(&img);
    view.setModel(m);

    startClock();
    for(size_t i = 0; i < ITER_PAINT; i++)
        view.render(&painter);
    report("Paint", ITER_PAINT);

    m = Typeset::Model::fromSerial("for(i ← 0; i ≤ 200; i ← i + 1)\n    print(i, \"\\n\")");

    startClock();
    for(size_t i = 0; i < ITER_LOOP; i++)
        m->run();
    report("Print Loop", ITER_LOOP);

    recordResults();
}
