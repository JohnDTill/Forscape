#ifndef FORSCAPE_SYMBOL_LINK_PASS_H
#define FORSCAPE_SYMBOL_LINK_PASS_H

#include "forscape_parse_tree.h"

namespace Forscape {

namespace Code {

class SymbolTableLinker{
private:
    std::vector<size_t> stack_frame;
    std::vector<size_t> old_flags;
    size_t stack_size = 0;
    size_t closure_depth = 0;
    ParseTree& parse_tree;

public:
    SymbolTableLinker(ParseTree& parse_tree) noexcept;
    void link() noexcept; //No user errors possible at link stage

private:
    void resolveStmt(ParseNode pn) noexcept;
    void resolveExpr(ParseNode pn) noexcept;

    //Statements
    void resolveAlgorithm(ParseNode pn) noexcept;
    void resolveAssignment(ParseNode pn) noexcept;
    void resolveBlock(ParseNode pn) noexcept;
    void resolveEWiseAssignment(ParseNode pn) noexcept;
    void resolveFor(ParseNode pn) noexcept;
    void resolveIf(ParseNode pn) noexcept;
    void resolveIfElse(ParseNode pn) noexcept;
    void resolveImport(ParseNode pn) noexcept;
    void resolveNamespace(ParseNode pn) noexcept;
    void resolveRangedFor(ParseNode pn) noexcept;
    void resolveReassignment(ParseNode pn) noexcept;
    void resolveSwitch(ParseNode pn) noexcept;

    //Expressions
    void resolveBig(ParseNode pn) noexcept;
    void resolveDefiniteIntegral(ParseNode pn) noexcept;
    void resolveDerivative(ParseNode pn) noexcept;

    //Helper
    void resolveDeclaration(ParseNode pn) noexcept;
    void resolveReference(ParseNode pn) noexcept;
    void resolveAllChildrenAsExpressions(ParseNode pn) noexcept;
    void increaseLexicalDepth() noexcept;
    void decreaseLexicalDepth() noexcept;
    void increaseClosureDepth(ParseNode pn) noexcept;
    void decreaseClosureDepth(ParseNode pn) noexcept;
};

}

}

#endif // FORSCAPE_SYMBOL_LINK_PASS_H
