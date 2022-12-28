#ifndef FORSCAPE_COMPILER_H
#define FORSCAPE_COMPILER_H

#include "forscape_common.h"

namespace Forscape {

namespace Code {

class Compiler {
public:
    Compiler(Program* program);
    void compile();
    void clear() noexcept;

private:
    void compileFile(Typeset::Model* model);
    void resolveStmt(ParseNode pn);
    AstNode resolveExprTop(ParseNode pn);
    AstNode resolveExpr(ParseNode pn);

    //Statements
    void resolveImport(ParseNode pn);
    void resolvePrint(ParseNode pn);

    //Expressions
    template<bool write = false> AstNode resolveLValue(ParseNode pn);
    AstNode resolveScopeAccess(ParseNode pn);
    AstNode resolveString(ParseNode pn);

    AbstractSyntaxTree& ast; //Instructions to execute
    //std::vector<Value> //Values to use
    Typeset::Model* model;
    ParseTree* parse_tree;
    std::vector<Code::Error>& errors;
    std::vector<Code::Error>& warnings;
};

}

}

#endif // FORSCAPE_COMPILER_H
