#include "report.h"
#include "typeset.h"
#include "forscape_scanner.h"
#include "forscape_serial.h"
#include "forscape_parser.h"
#include "forscape_symbol_build_pass.h"
#include "forscape_interpreter.h"

using namespace Forscape;
using namespace Code;

static constexpr size_t ITER_SERIAL_VALIDATION = 50000;
static constexpr size_t ITER_MODEL_LOAD_DELETE = 5000;
static constexpr size_t ITER_SCANNER = 500000;
static constexpr size_t ITER_PARSER = 500000;
static constexpr size_t ITER_SYMBOL_TABLE = 50000;
static constexpr size_t ITER_INTERPRETER = 5000;
static constexpr size_t ITER_CALC_SIZE = 5000000;
static constexpr size_t ITER_LAYOUT = 10000000;
static constexpr size_t ITER_PAINT = 1500;
static constexpr size_t ITER_LOOP = 10;
static constexpr size_t ITER_PRINT_SIZE = 100;
static constexpr size_t ITER_PRINT_LAYOUT = 100;
static constexpr size_t ITER_PRINT_PAINT = 30;

void runBenchmark(){
    std::string src = readFile("../test/interpreter_scripts/in/root_finding_terse.π");

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

    Code::SymbolTableBuilder sym_table(parser.parse_tree, m);
    sym_table.resolveSymbols();
    Code::StaticPass static_pass(parser.parse_tree, sym_table.symbol_table, m->errors, m->warnings);
    static_pass.resolve();
    Code::Interpreter interpreter;

    startClock();
    for(size_t i = 0; i < ITER_INTERPRETER; i++)
        interpreter.run(parser.parse_tree, sym_table.symbol_table, static_pass.instantiation_lookup);
    assert(interpreter.error_code == NO_ERROR_FOUND);
    report("Interpreter", ITER_INTERPRETER);

    #ifndef FORSCAPE_TYPESET_HEADLESS
    m = Typeset::Model::fromSerial(src);

    startClock();
    for(size_t i = 0; i < ITER_CALC_SIZE; i++)
        m->calculateSizes();
    report("Calculate size", ITER_CALC_SIZE);

    startClock();
    for(size_t i = 0; i < ITER_LAYOUT; i++)
        m->updateLayout();
    report("Update layout", ITER_LAYOUT);

    Typeset::Console view;
    //view.show();

    view.resize(QSize(1920*2, 1080*2));
    QImage img(view.size(), QImage::Format_RGB32);
    QPainter painter(&img);
    view.setModel(m);

    startClock();
    for(size_t i = 0; i < ITER_PAINT; i++) view.render(&painter);
    report("Paint", ITER_PAINT);
    #endif

    #define N_PRINTS "1000000"

    m = Typeset::Model::fromSerial("for(i ← 0; i ≤ " N_PRINTS "; i ← i + 1)\n    print(i, \"\\n\")");

    startClock();
    for(size_t i = 0; i < ITER_LOOP; i++)
        m->run();
    report("Print " N_PRINTS, ITER_LOOP);

    #ifndef FORSCAPE_TYPESET_HEADLESS
    Typeset::Model* temp = m;
    m = Typeset::Model::fromSerial(m->run(), true);
    delete temp;

    startClock();
    for(size_t i = 0; i < ITER_PRINT_SIZE; i++)
        m->calculateSizes();
    report("Print output size", ITER_PRINT_SIZE);

    startClock();
    for(size_t i = 0; i < ITER_PRINT_LAYOUT; i++)
        m->updateLayout();
    report("Print output layout", ITER_PRINT_LAYOUT);

    view.resize(QSize(1920*2, 1080*2));
    view.setModel(m);

    startClock();
    for(size_t i = 0; i < ITER_PRINT_PAINT; i++) view.render(&painter);
    report("Print output Paint", ITER_PRINT_PAINT);
    #endif

    recordResults();
}
