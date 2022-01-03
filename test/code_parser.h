#include <hope_parser.h>
#include <hope_scanner.h>
#include "report.h"
#include "typeset.h"

using namespace Hope;
using namespace Code;

static ParseTree eval(const std::string& src){
    Typeset::Model* model = Typeset::Model::fromSerial(src);
    model->clearFormatting();

    Code::Scanner scanner(model);
    scanner.scanAll();
    Code::Parser parser(scanner, model);
    parser.parseAll();
    delete model;

    return parser.parse_tree;
}

static bool testExpressionType(const std::string& src, Op type){
    bool passing = true;
    ParseTree parse_tree = eval(src);
    ParseNode root = parse_tree.back();
    assert(parse_tree.getOp(root) == OP_BLOCK);
    ParseNode stmt = parse_tree.arg(root, 0);
    assert(parse_tree.getOp(stmt) == OP_EXPR_STMT);
    ParseNode expr = parse_tree.child(stmt);
    if(parse_tree.getOp(expr) != type){
        std::cout << "Expected type " << type << ", got "
                  << parse_tree.getOp(expr) << std::endl;
        passing = false;
    }

    return passing;
}

static bool testFormatting(const std::string& src, const std::string& fmt){
    bool passing = true;
    Typeset::Model* model = Typeset::Model::fromSerial(src);
    model->clearFormatting();
    Code::Scanner scanner(model);
    scanner.scanAll();
    Code::Parser parser(scanner, model);
    parser.parseAll();
    if(model->toSerialWithSemanticTags() != fmt){
        std::cout << "Line " << __LINE__ << ", Parser semantic tag mismatch:\n";
        std::cout << "Expected: " << fmt << '\n'
                  << "Actual:   " << model->toSerialWithSemanticTags() << std::endl;
        passing = false;
    }

    delete model;

    return passing;
}

static bool testStatementType(const std::string& src, Op type){
    bool passing = true;
    ParseTree parse_tree = eval(src);
    ParseNode root = parse_tree.back();
    assert(parse_tree.getOp(root) == OP_BLOCK);
    ParseNode stmt = parse_tree.arg(root, 0);
    if(parse_tree.getOp(stmt) != type){
        std::cout << "Expected type " << type << ", got "
                  << parse_tree.getOp(stmt) << std::endl;
        passing = false;
    }

    return passing;
}

inline bool testParser() {
    bool passing = true;

    passing &= testExpressionType("2", OP_INTEGER_LITERAL);
    passing &= testExpressionType("x", OP_IDENTIFIER);
    passing &= testExpressionType("2.2", OP_DECIMAL_LITERAL);
    passing &= testExpressionType("2 + 2", OP_ADDITION);
    passing &= testExpressionType("2 + 2*1", OP_ADDITION);
    passing &= testExpressionType("3x", OP_IMPLICIT_MULTIPLY);

    passing &= testStatementType("2", OP_EXPR_STMT);
    passing &= testStatementType("x = 2", OP_EQUAL);
    passing &= testStatementType("x + y = 2.5", OP_EQUAL);
    passing &= testStatementType("3x + 2y = 6", OP_EQUAL);

    std::string input = "x + y = 2.5\n"
                        "3x + 2y = 6\n";

    std::string fmt =
        "<tag|6>x<tag|0> + <tag|6>y<tag|0> = <tag|4>2.5<tag|0>\n"
        "<tag|4>3<tag|6>x<tag|0> + <tag|4>2<tag|6>y<tag|0> = <tag|4>6<tag|0>\n";

    passing &= testFormatting(input, fmt);

    input = "12 = 0.5 //Test formatting\nx^2 = x2 //Obviously";

    fmt = "<tag|4>1<tag|0><tag|4>2<tag|0> = <tag|4>0.5<tag|0> <tag|1>//Test formatting<tag|0>\n"
          "<tag|6>x<tag|0>^<tag|4>2<tag|0> = <tag|6>x<tag|0><tag|4>2<tag|0> <tag|1>//Obviously<tag|0>";

    passing &= testFormatting(input, fmt);

    if(!allTypesetElementsFreed()){
        printf("Unfreed typeset elements\n");
        passing = false;
    }

    report("Code parser", passing);
    return passing;
}
