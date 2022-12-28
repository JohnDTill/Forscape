#include "forscape_compiler.h"

#include "forscape_program.h"
#include "typeset_model.h"

template<bool remove_quotes = true>
static void removeEscapes(std::string& str) noexcept {
    assert(!remove_quotes || str.front() == '"');
    assert(!remove_quotes || str.back() == '"');

    size_t offset = remove_quotes;
    size_t i = remove_quotes;
    while(i < str.size()-1){
        str[i-offset] = str[i];

        if(str[i] == '\\'){
            switch (str[i+1]) {
                case 'n': str[i-offset] = '\n'; i++; offset++; break;
                case '"': str[i-offset] = '"'; i++; offset++; break;
                case '\\': str[i-offset] = '\\'; i++; offset++; break;
                default: break; //EVENTUALLY: the default could be an error
            }
        }

        i++;
    }
    if(i < str.size()) str[i-offset] = str[i];
    str.resize(str.size()-offset-remove_quotes);
}

namespace Forscape {

namespace Code {

Compiler::Compiler(Program* program)
    : ast(program->ast),
      model(program->program_entry_point),
      parse_tree(&model->parser.parse_tree),
      errors(program->errors),
      warnings(program->warnings){}

void Compiler::compile(){
    clear();
    compileFile(model);
}

void Compiler::clear() noexcept {
    Program::instance()->resetModels();
}

void Compiler::compileFile(Typeset::Model* model) {
    if(model->is_parsed) return;
    model->is_parsed = true;
    if(!model->errors.empty()){
        errors.insert(errors.end(), model->errors.cbegin(), model->errors.cend());
        return;
    }
    warnings.insert(warnings.end(), model->warnings.cbegin(), model->warnings.cend());
    ParseTree* cached_parse_tree = parse_tree;
    parse_tree = &model->parser.parse_tree;
    resolveStmt(parse_tree->root);
    parse_tree = cached_parse_tree;
}

void Compiler::resolveStmt(ParseNode pn) {
    switch (parse_tree->getOp(pn)) {
        //case OP_ALGORITHM: //DO THIS
        //case OP_ASSIGN: //DO THIS
        //case OP_CLASS: //DO THIS
        //case OP_EQUAL: //DO THIS
        case OP_IMPORT: resolveImport(pn); break;
        case OP_PRINT: resolvePrint(pn); break;
        //case OP_REASSIGN: //DO THIS
        default: assert(false);
    }
}

AstNode Compiler::resolveExprTop(ParseNode pn) {
    //DO THIS

    return resolveExpr(pn);
}

AstNode Compiler::resolveExpr(ParseNode pn) {
    switch (parse_tree->getOp(pn)) {
        //case OP_IDENTIFIER: //DO THIS
        case OP_SCOPE_ACCESS: return resolveScopeAccess(pn);
        case OP_STRING: return resolveString(pn);
    }
    assert(false);
}

void Compiler::resolveImport(ParseNode pn) {
    assert(parse_tree->getOp(pn) == OP_IMPORT);
    ParseNode file = parse_tree->child(pn);
    Typeset::Model* model = reinterpret_cast<Typeset::Model*>(parse_tree->getFlag(file));
    compileFile(model);
}

void Compiler::resolvePrint(ParseNode pn) {
    assert(parse_tree->getOp(pn) == OP_PRINT);

    //DO THIS
    /*
    ast.startArgList();
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
        KiCAS::AstNode arg = resolveExprTop(parse_tree.arg(pn, i));
        assert(ast.isNode(arg));
        ast.addArg(arg);
    }

    ast.printStmt();
    */
}

template<bool write>
AstNode Compiler::resolveLValue(ParseNode pn) {
    //DO THIS
    //This is complicated since the lvalue could refer to another file
}

AstNode Compiler::resolveScopeAccess(ParseNode pn) {
    assert(parse_tree->getOp(pn) == OP_SCOPE_ACCESS);

    ParseNode lhs = resolveLValue(parse_tree->lhs(pn));
    ParseNode rhs = parse_tree->rhs(pn);
    assert(parse_tree->getOp(rhs) == OP_IDENTIFIER);

    //DO THIS
    //resolve lhs
    //read rhs
}

AstNode Compiler::resolveString(ParseNode pn) {
    assert(parse_tree->getOp(pn) == OP_STRING);
    std::string str = parse_tree->getSelection(pn).str();
    removeEscapes(str);
    //DO THIS
    //return ast.getNodeForStringTakingOwnership(str);
}

}

}
