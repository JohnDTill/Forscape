#ifndef HOPE_INTERPRETER_H
#define HOPE_INTERPRETER_H

#include "hope_stack.h"
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Typeset {
    class Controller;
    class Model;
    class View;
}

namespace Code {

struct Error;
class ParseTree;

class Interpreter{
public:
    Interpreter(const ParseTree& parse_tree,
                Typeset::Model* model,
                Typeset::View* view,
                Typeset::View* console);
    Typeset::Model* interpret(ParseNode root);
    void stop();

private:
    const ParseTree& parse_tree;
    std::vector<Error>& errors;

    enum ExitMode{
        KEEP_GOING = 0,
        CONTINUE = 1,
        BREAK = 3,
        RETURN = 7,
        ERROR = 15,
    };
    ExitMode exit_mode = KEEP_GOING;

    Closure* active_closure = nullptr;
    Stack stack;

    Typeset::Model* output;
    Typeset::View* view;
    Typeset::View* console;

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
    void returnStmt(ParseNode pn);
    Value implicitMult(ParseNode pn, size_t start = 0);
    Value sum(ParseNode pn);
    Value prod(ParseNode pn);
    Value big(ParseNode pn, ParseNodeType type);
    Value cases(ParseNode pn);
    bool evaluateCondition(ParseNode pn);
    Value interpretExpr(ParseNode pn);
    Value unaryDispatch(ParseNode pn);
    Value binaryDispatch(ParseNode pn);
    Value binaryDispatch(ParseNodeType type, const Value& lhs, const Value& rhs, ParseNode op_node);
    void reassign(ParseNode lhs, ParseNode rhs);
    void reassignSubscript(ParseNode lhs, ParseNode rhs);
    void elementWiseAssignment(ParseNode pn);
    Value& read(ParseNode pn);
    Value& readLocal(ParseNode pn);
    Value& readGlobal(ParseNode pn);
    Value& readUpvalue(ParseNode pn);
    Value error(ErrorCode code, ParseNode pn);
    Value matrix(ParseNode pn);
    Value str(ParseNode pn) const;
    Value anonFun(ParseNode pn);
    Value call(ParseNode pn);
    void callStmt(ParseNode pn);
    void callAlg(Algorithm &alg, ParseNode call);
    Value elementAccess(ParseNode pn);
    typedef Eigen::ArithmeticSequence<Eigen::Index, Eigen::Index, Eigen::Index> Slice;
    Slice readSubscript(ParseNode pn, Eigen::Index max);
    static constexpr Eigen::Index INVALID = -1;
    Eigen::Index readIndex(ParseNode pn, Eigen::Index max);
    double readDouble(ParseNode pn);
    void printNode(const ParseNode& pn);
    void appendTo(Typeset::Model* m, const Value& val);
    static double dot(const Eigen::MatrixXd& a, const Eigen::MatrixXd& b) noexcept;
    static Eigen::MatrixXd hat(const Eigen::MatrixXd& a);
    Value pow(const Eigen::MatrixXd& a, double b, ParseNode pn);
    Value unitVector(ParseNode pn);
    static double pNorm(const Eigen::MatrixXd& a, double b) noexcept;
    static void removeEscapes(std::string& str) noexcept;

    static std::string formatted(double num){
        std::string str = std::to_string(num);

        auto dec = str.find('.');
        if(dec != std::string::npos){
            // Remove trailing zeroes
            str = str.substr(0, str.find_last_not_of('0')+1);
            // If the decimal point is now the last character, remove that as well
            if(str.find('.') == str.size()-1)
                str = str.substr(0, str.size()-1);
        }

        bool is_negative_zero = (str.front() == '-' && str.size() == 2 && str[1] == '0');
        if(is_negative_zero) return "0";

        return str;
    }

    std::vector<size_t> frames;
};

}

}

#endif // HOPE_INTERPRETER_H
