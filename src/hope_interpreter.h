#ifndef HOPE_INTERPRETER_H
#define HOPE_INTERPRETER_H

#include "hope_parse_tree.h"
#include "hope_stack.h"
#include "hope_static_pass.h"
#include <memory>
#include <readerwriterqueue/readerwriterqueue.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Hope {

class InterpreterOutput;

namespace Code {

class ParseTree;
class SymbolTable;

class Interpreter{
public:
    //This is for lock-free, single producer / single consumer message passing of print statements
    static constexpr size_t QUEUE_BLOCK_SIZE = 1024;
    moodycamel::ReaderWriterQueue<InterpreterOutput*, QUEUE_BLOCK_SIZE> message_queue;

    enum Instruction{
        RUN,
        PAUSE,
        STOP,
        REQUIRE_WORD_SIZE_FOR_ATOMIC_OPERATION = std::numeric_limits<size_t>::max(),
    };

    Instruction directive = RUN; //This can be set from a thread outside the interpreter

    enum Status {
        NORMAL = 0,
        CONTINUE = 1,
        BREAK = 3,
        RETURN = 7,
        RUNTIME_ERROR = 15,
        FINISHED = std::numeric_limits<size_t>::max(),
    };

    //These can be read from outside the interpreter
    Status status = NORMAL;
    ErrorCode error_code = NO_ERROR_FOUND;
    ParseNode error_node = ParseTree::EMPTY;

    void run(const ParseTree& parse_tree, SymbolTable symbol_table, const InstantiationLookup& inst_lookup);
    void runThread(const ParseTree& parse_tree, SymbolTable symbol_table, const InstantiationLookup& inst_lookup);
    void stop();

private:
    std::vector<size_t> frames;
    ParseTree parse_tree;
    InstantiationLookup inst_lookup;
    Closure* active_closure = nullptr;
    Stack stack;

    void reset() noexcept;
    Value error(ErrorCode code, ParseNode pn) noexcept;
    void interpretStmt(ParseNode pn);
    void printStmt(ParseNode pn);
    void assertStmt(ParseNode pn);
    void assignStmt(ParseNode pn);
    void whileStmt(ParseNode pn);
    void forStmt(ParseNode pn);
    void ifStmt(ParseNode pn);
    void ifElseStmt(ParseNode pn);
    void blockStmt(ParseNode pn);
    void algorithmStmt(ParseNode pn);
    void initClosure(Closure& closure, ParseNode val_cap, ParseNode ref_cap);
    void breakLocalClosureLinks(Closure& closure, ParseNode val_cap, ParseNode ref_cap);
    void returnStmt(ParseNode pn);
    void plotStmt(ParseNode pn);
    Value implicitMult(ParseNode pn, size_t start = 0);
    Value sum(ParseNode pn);
    Value prod(ParseNode pn);
    Value big(ParseNode pn, Op type);
    Value cases(ParseNode pn);
    bool evaluateCondition(ParseNode pn);
    Value interpretExpr(ParseNode pn);
    Value unaryDispatch(ParseNode pn);
    Value binaryDispatch(ParseNode pn);
    Value binaryDispatch(Op type, const Value& lhs, const Value& rhs, ParseNode op_node);
    void reassign(ParseNode lhs, ParseNode rhs);
    void reassignSubscript(ParseNode lhs, ParseNode rhs);
    void elementWiseAssignment(ParseNode pn);
    Value& read(ParseNode pn) noexcept;
    Value& readLocal(ParseNode pn) noexcept;
    Value& readGlobal(ParseNode pn) noexcept;
    Value& readClosedVar(ParseNode pn) const noexcept;
    Value matrix(ParseNode pn);
    Value str(ParseNode pn) const;
    Value anonFun(ParseNode pn);
    Value call(ParseNode pn);
    void callStmt(ParseNode pn);
    Value innerCall(ParseNode call, Closure& closure, ParseNode fn, bool expect, bool is_lambda);
    Value elementAccess(ParseNode pn);
    typedef Eigen::ArithmeticSequence<Eigen::Index, Eigen::Index, Eigen::Index> Slice;
    Slice readSubscript(ParseNode pn, Eigen::Index max);
    static constexpr Eigen::Index INVALID = -1;
    Eigen::Index readIndex(ParseNode pn, Eigen::Index max);
    double readDouble(ParseNode pn);
    void printNode(const ParseNode& pn);
    static double dot(const Eigen::MatrixXd& a, const Eigen::MatrixXd& b) noexcept;
    static Eigen::MatrixXd hat(const Eigen::MatrixXd& a);
    static Eigen::MatrixXd invHat(const Eigen::MatrixXd& a);
    Value pow(const Eigen::MatrixXd& a, double b, ParseNode pn);
    Value unitVector(ParseNode pn);
    Value finiteDiff(ParseNode pn);
    static double pNorm(const Eigen::MatrixXd& a, double b) noexcept;
    static void removeEscapes(std::string& str) noexcept;
    static std::string formatted(double num);
    Value factorial(ParseNode pn);
    Value binomial(ParseNode pn);
};

}

}

#endif // HOPE_INTERPRETER_H
