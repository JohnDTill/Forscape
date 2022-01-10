#include "hope_symbol_build_pass.h"

#include <code_parsenode_ops.h>
#include "typeset_model.h"
#include <algorithm>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

const std::unordered_map<std::string_view, Op> SymbolTableBuilder::predef {
    {"π", OP_PI},
    {"e", OP_EULERS_NUMBER},
    {"φ", OP_GOLDEN_RATIO},
    {"c", OP_SPEED_OF_LIGHT},
    {"ℎ", OP_PLANCK_CONSTANT},
    {"ℏ", OP_REDUCED_PLANCK_CONSTANT},
    {"σ", OP_STEFAN_BOLTZMANN_CONSTANT},
    {"I", OP_IDENTITY_AUTOSIZE},
    //{"Γ", _}, //DO THIS - should have the error function
};

SymbolTableBuilder::SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model)
    : errors(model->errors), parse_tree(parse_tree), symbol_table(model->parser.symbol_table) {}

void SymbolTableBuilder::resolveSymbols(){
    reset();

    ParseNode n = parse_tree.root;
    for(size_t i = 0; i < parse_tree.getNumArgs(n); i++)
        resolveStmt(parse_tree.arg(n, i));

    decreaseLexicalDepth();
}

void SymbolTableBuilder::reset() noexcept{
    ids.clear();
    map.clear();
    symbol_id_index.clear();
    lexical_depth = GLOBAL_DEPTH;
    closure_depth = 0;
    active_scope_id = 0;
}

Scope& SymbolTableBuilder::activeScope() noexcept{
    return symbol_table.scopes[active_scope_id];
}

void SymbolTableBuilder::addScope(){
    active_scope_id++;
}

void SymbolTableBuilder::closeScope() noexcept{
    active_scope_id++;
}

Symbol& SymbolTableBuilder::lastSymbolOfId(const Id& identifier) noexcept{
    return symbol_table.symbols[identifier.back()];
}

Symbol& SymbolTableBuilder::lastSymbolOfId(IdIndex index) noexcept{
    return lastSymbolOfId(ids[index]);
}

size_t SymbolTableBuilder::lastSymbolIndexOfId(IdIndex index) const noexcept{
    return ids[index].back();
}

Symbol& SymbolTableBuilder::lastDefinedSymbol() noexcept{
    return symbol_table.symbols.back();
}

Symbol* SymbolTableBuilder::symbolFromSelection(const Typeset::Selection& sel) noexcept{
    auto lookup = map.find(sel);
    return lookup == map.end() ? nullptr : &lastSymbolOfId(lookup->second);
}

size_t SymbolTableBuilder::symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept{
    auto lookup = map.find(sel);
    return lookup == map.end() ? NONE : lastSymbolIndexOfId(lookup->second);
}

void SymbolTableBuilder::resolveStmt(ParseNode pn){
    switch (parse_tree.getOp(pn)) {
    case OP_EQUAL: resolveEquality(pn); break;
    case OP_ASSIGN: resolveAssignment(pn); break;
    case OP_BLOCK: resolveBlock(pn); break;
    case OP_ALGORITHM: resolveAlgorithm(pn); break;
    case OP_PROTOTYPE_ALG: resolvePrototype(pn); break;

        case OP_WHILE:
        case OP_IF:
            resolveConditional1(pn); break;

        case OP_IF_ELSE: resolveConditional2(pn); break;
        case OP_FOR: resolveFor(pn); break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveExpr(ParseNode pn){
    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER: resolveReference(pn); break;
        case OP_LAMBDA: resolveLambda(pn); break;
        case OP_SUBSCRIPT_ACCESS: resolveSubscript(pn); break;

        case OP_SUMMATION:
        case OP_PRODUCT:
            resolveBig(pn);
            break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveEquality(ParseNode pn){
    if(parse_tree.getNumArgs(pn) > 2){
        errors.push_back(Error(parse_tree.getSelection(pn), ErrorCode::TYPE_ERROR));
    }else{
        resolveExpr(parse_tree.rhs(pn));
        defineLocalScope(parse_tree.lhs(pn));
    }
}

void SymbolTableBuilder::resolveAssignment(ParseNode pn){
    assert(parse_tree.getNumArgs(pn) == 2);

    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    switch(parse_tree.getOp( lhs )){
        case OP_IDENTIFIER:
            resolveExpr(rhs);
            resolveAssignmentId(pn);
            break;

        case OP_SUBSCRIPT_ACCESS:
            if(map.find(parse_tree.getSelection(lhs)) != map.end()){
                resolvePotentialIdSub(lhs);
                resolveExpr(rhs);
                resolveAssignmentId(pn);
            }else{
                parse_tree.setOp(pn, OP_REASSIGN);
                resolveAssignmentSubscript(pn, lhs, rhs);
            }
            break;

        default:
            errors.push_back(Error(parse_tree.getSelection(lhs), ASSIGN_EXPRESSION));
    }
}

void SymbolTableBuilder::resolveAssignmentId(ParseNode pn){
    ParseNode id = parse_tree.lhs(pn);
    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getOp(id) != OP_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    auto lookup = map.find(c);
    if(lookup == map.end()){
        makeEntry(c, id, false);
    }else{
        size_t sym_index = lastSymbolIndexOfId(lookup->second);
        Symbol& sym = symbol_table.symbols[sym_index];
        sym.is_reassigned = true;
        parse_tree.setOp(pn, OP_REASSIGN);
        resolveReference(id, c, sym_index);
    }
}

void SymbolTableBuilder::resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs){
    ParseNode id = parse_tree.arg(lhs, 0);

    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getOp(id) != OP_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    size_t symbol_index = symbolIndexFromSelection(c);
    if(symbol_index != NONE){
        symbol_table.symbols[symbol_index].is_reassigned = true;
        resolveReference(id, c, symbol_index);
    }else{
        errors.push_back(Error(c, BAD_READ));
    }

    bool only_trivial_slice = true;
    std::vector<ParseNode> undefined_vars;
    size_t num_subscripts = parse_tree.getNumArgs(lhs) - 1;
    for(size_t i = 0; i < num_subscripts; i++){
        ParseNode sub = parse_tree.arg(lhs, i+1);
        Op type = parse_tree.getOp(sub);
        const Typeset::Selection& sel = parse_tree.getSelection(sub);
        if(type == OP_IDENTIFIER && map.find(sel) == map.end()){
            undefined_vars.push_back(sub);
        }else{
            only_trivial_slice &= (type == OP_SLICE) && parse_tree.getNumArgs(sub) == 1;
            resolveExpr(sub);
        }
    }

    if(!undefined_vars.empty() & only_trivial_slice){
        if(num_subscripts > 2){
            const Typeset::Selection& sel = parse_tree.getSelection(parse_tree.arg(lhs, 3));
            errors.push_back(Error(sel, INDEX_OUT_OF_RANGE));
            return;
        }

        parse_tree.setOp(pn, OP_ELEMENTWISE_ASSIGNMENT);
        increaseLexicalDepth();
        size_t vars_start = symbol_table.symbols.size();
        for(ParseNode pn : undefined_vars){
            bool success = defineLocalScope(pn, false);
            assert(success);
        }
        resolveExpr(rhs);

        for(size_t i = 0; i < undefined_vars.size(); i++){
            Symbol& sym = symbol_table.symbols[vars_start + i];
            ParseNode var = undefined_vars[undefined_vars.size()-1];
            sym.is_ewise_index = true;
            if(!sym.is_used){
                errors.push_back(Error(parse_tree.getSelection(var), UNUSED_ELEM_AGN_INDEX));
                sym.is_used = true;
            }
        }

        decreaseLexicalDepth();
    }else if(!undefined_vars.empty()){
        for(ParseNode pn : undefined_vars)
            errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ_OR_SUBSCRIPT));
    }else{
        resolveExpr(rhs);
    }
}

bool SymbolTableBuilder::resolvePotentialIdSub(ParseNode pn){
    if(parse_tree.getNumArgs(pn) != 2) return false;

    ParseNode id = parse_tree.lhs(pn);
    ParseNode sub = parse_tree.rhs(pn);

    if(parse_tree.getOp(id) != OP_IDENTIFIER || parse_tree.getOp(sub) != OP_IDENTIFIER)
        return false;

    const Typeset::Selection& id_sel = parse_tree.getSelection(id);
    const Typeset::Selection& id_sub = parse_tree.getSelection(sub);

    auto lookup_id = map.find(id_sel);
    auto lookup_sub = map.find(id_sub);

    if(lookup_id != map.end() && lookup_sub != map.end()) return false;

    parse_tree.setOp(pn, OP_IDENTIFIER);

    return true;
}

void SymbolTableBuilder::resolveReference(ParseNode pn){
    Typeset::Selection c = parse_tree.getSelection(pn);

    auto lookup = map.find(c);
    if(lookup == map.end()){
        auto lookup = predef.find(c.isTextSelection() ? c.strView() : c.str());
        if(lookup != predef.end()){
            Op read_type = lookup->second;
            parse_tree.setOp(pn, read_type);
            c.format(SEM_PREDEF);
            return;
        }
        errors.push_back(Error(c, BAD_READ));
    }else{
        size_t id_index = lookup->second;
        resolveReference(pn, c, ids[id_index].back());
    }
}

void SymbolTableBuilder::resolveReference(ParseNode pn, const Typeset::Selection& c, size_t sym_id){
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    Symbol& sym = symbol_table.symbols[sym_id];
    parse_tree.setFlag(pn, sym.flag);
    sym.is_used = true;

    sym.document_occurences.push_back(c);
    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    //DO THIS
    //activeScope().subscopes.back().usages.push_back(Scope::Usage(sym_id, pn, Scope::READ));
}

void SymbolTableBuilder::resolveConditional1(ParseNode pn){
    resolveExpr( parse_tree.arg(pn, 0) );
    resolveBody( parse_tree.arg(pn, 1) );
}

void SymbolTableBuilder::resolveConditional2(ParseNode pn){
    resolveExpr( parse_tree.arg(pn, 0) );
    resolveBody( parse_tree.arg(pn, 1) );
    resolveBody( parse_tree.arg(pn, 2) );
}

void SymbolTableBuilder::resolveFor(ParseNode pn){
    increaseLexicalDepth();
    resolveStmt(parse_tree.arg(pn, 0));
    resolveExpr(parse_tree.arg(pn, 1));
    resolveStmt(parse_tree.arg(pn, 2));
    resolveStmt(parse_tree.arg(pn, 3));
    decreaseLexicalDepth();
}

void SymbolTableBuilder::resolveBody(ParseNode pn){
    increaseLexicalDepth();
    resolveStmt(pn);
    decreaseLexicalDepth();

    switch (parse_tree.getOp(pn)) {
        case OP_EQUAL:
        case OP_ASSIGN:{
            Typeset::Selection c = parse_tree.getSelection(pn);
            errors.push_back(Error(c, UNUSED_VAR));
        }
    }
}

void SymbolTableBuilder::resolveBlock(ParseNode pn){
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveStmt( parse_tree.arg(pn, i) );
}

void SymbolTableBuilder::resolveDefault(ParseNode pn){
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveExpr(parse_tree.arg(pn, i));
}

void SymbolTableBuilder::resolveLambda(ParseNode pn){
    increaseClosureDepth(pn);

    ParseNode params = parse_tree.arg(pn, 2);
    assert(parse_tree.getOp(params) == OP_LIST);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++)
        defineLocalScope( parse_tree.arg(params, i) );

    ParseNode rhs = parse_tree.arg(pn, 3);
    resolveExpr(rhs);

    decreaseClosureDepth();
}

void SymbolTableBuilder::resolveAlgorithm(ParseNode pn){
    ParseNode name = parse_tree.arg(pn, 0);
    ParseNode captured = parse_tree.arg(pn, 1);
    ParseNode params = parse_tree.arg(pn, 3);
    ParseNode body = parse_tree.arg(pn, 4);

    const Typeset::Selection sel = parse_tree.getSelection(name);
    auto lookup = map.find(sel);
    if(lookup == map.end()){
        makeEntry(sel, name, true);
        parse_tree.setStackOffset(name, NONE); //This is a kludge to tell the interpreter it's not a prototype
    }else{
        size_t index = lookup->second;
        Id& id = ids[index];
        Symbol& sym = lastSymbolOfId(id);
        if(sym.declaration_lexical_depth == lexical_depth){
            if(sym.is_prototype){
                resolveReference(name, sel, id.back());
            }else{
                errors.push_back(Error(sel, TYPE_ERROR));
            }
        }else if(sym.declaration_lexical_depth == lexical_depth){
            appendEntry(index, sel, name, true);
        }

        sym.is_prototype = false;
    }

    if(captured != ParseTree::EMPTY){
        for(size_t i = 0; i < parse_tree.getNumArgs(captured); i++){
            ParseNode capture = parse_tree.arg(captured, i);
            resolveReference(capture);
        }
    }

    increaseClosureDepth(pn);

    if(captured != ParseTree::EMPTY){
        size_t N = parse_tree.getNumArgs(captured);

        for(size_t i = 0; i < N; i++){
            ParseNode capture = parse_tree.arg(captured, i);
            defineLocalScope(capture, false);
            symbol_table.symbols.back().is_captured = true;
            symbol_table.symbols.back().is_closure_nested = true;
        }
    }

    size_t params_start = symbol_table.symbols.size();
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL){
            resolveExpr(parse_tree.rhs(param));
            param = parse_tree.lhs(param);
        }
        bool success = defineLocalScope( param, false );
        assert(success);
    }
    resolveStmt(body);

    bool expect_default = false;
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode n = parse_tree.arg(params, i);
        if(parse_tree.getOp(n) == OP_EQUAL){
            expect_default = true;
        }else if(expect_default){
            errors.push_back(Error(parse_tree.getSelection(n), EXPECT_DEFAULT_ARG));
        }

        Symbol& sym = symbol_table.symbols[params_start + i];
        sym.is_const = !sym.is_reassigned;
    }

    decreaseClosureDepth();
}

void SymbolTableBuilder::resolvePrototype(ParseNode pn){
    if(defineLocalScope(parse_tree.child(pn)))
        lastDefinedSymbol().is_prototype = true;
}

void SymbolTableBuilder::resolveSubscript(ParseNode pn){
    if(parse_tree.getNumArgs(pn) != 2){ resolveDefault(pn); return; }

    ParseNode id = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    if(parse_tree.getOp(id) != OP_IDENTIFIER || parse_tree.getOp(rhs) != OP_IDENTIFIER){
        resolveDefault(pn);
        return;
    }

    const Typeset::Selection& id_sel = parse_tree.getSelection(id);
    const Typeset::Selection& rhs_sel = parse_tree.getSelection(rhs);

    if(map.find(id_sel) == map.end() || map.find(rhs_sel) == map.end()){
        parse_tree.setOp(pn, OP_IDENTIFIER);
        resolveReference(pn);
    }else{
        resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveBig(ParseNode pn){
    increaseLexicalDepth();
    ParseNode assign = parse_tree.arg(pn, 0);
    if( parse_tree.getOp(assign) != OP_ASSIGN ) return;
    ParseNode id = parse_tree.lhs(assign);
    ParseNode stop = parse_tree.arg(pn, 1);
    ParseNode body = parse_tree.arg(pn, 2);

    defineLocalScope(id, false);
    lastDefinedSymbol().is_used = true;
    resolveExpr( parse_tree.rhs(assign) );
    resolveExpr( stop );
    resolveExpr( body );

    decreaseLexicalDepth();
}

bool SymbolTableBuilder::defineLocalScope(ParseNode pn, bool immutable){
    Typeset::Selection c = parse_tree.getSelection(pn);

    if(parse_tree.getOp(pn) == OP_SUBSCRIPT_ACCESS && !resolvePotentialIdSub(pn)){
        errors.push_back(Error(c, ASSIGN_EXPRESSION));
        return false;
    }

    if(parse_tree.getOp(pn) != OP_IDENTIFIER){
        errors.push_back(Error(c, ASSIGN_EXPRESSION));
        return false;
    }

    auto lookup = map.find(c);
    if(lookup == map.end()){
        makeEntry(c, pn, immutable);
    }else{
        size_t index = lookup->second;
        Symbol& sym = lastSymbolOfId(index);
        if(sym.declaration_lexical_depth == lexical_depth){
            bool CONST = sym.is_const;
            errors.push_back(Error(c, CONST ? REASSIGN_CONSTANT : MUTABLE_CONST_ASSIGN));
            return false;
        }else{
            appendEntry(index, c, pn, immutable);
        }
    }

    return true;
}

void SymbolTableBuilder::increaseLexicalDepth(){
    lexical_depth++;
    addScope();
}

void SymbolTableBuilder::decreaseLexicalDepth(){
    size_t stop = active_scope_id;
    size_t start = stop;
    while(symbol_table.scopes[start].prev != NONE) start = symbol_table.scopes[start].prev;

    for(size_t curr = start; curr <= stop; curr = symbol_table.scopes[curr].next){
        Scope& scope = symbol_table.scopes[curr];
        for(size_t sym_id = scope.sym_begin; sym_id < scope.sym_end; sym_id++){
            Id& id = ids[symbol_id_index[sym_id]];
            Symbol& sym = symbol_table.symbols[sym_id];
            assert(sym.declaration_lexical_depth == lexical_depth);

            finalize(sym_id);

            if(id.size() == 1){
                map.erase(sym.document_occurences.front()); //Much better to erase empty entries than check for them
                Id().swap(id); //Frees memory allocated to id_info
                //The entry in id_infos is left stranded, but that's
                //preferable to re-indexing.
            }else{
                id.pop_back();
            }
        }
    }

    closeScope();
    lexical_depth--;
}

void SymbolTableBuilder::increaseClosureDepth(ParseNode pn){
    closure_depth++;
    lexical_depth++;
    addScope();
}

void SymbolTableBuilder::decreaseClosureDepth(){
    closure_depth--;
    decreaseLexicalDepth();
}

void SymbolTableBuilder::finalize(size_t sym_id){
    const Symbol& sym = symbol_table.symbols[sym_id];
    assert(sym.declaration_lexical_depth == lexical_depth);

    SemanticType fmt = SEM_ID;
    if(sym.is_ewise_index){
        fmt = SEM_ID_EWISE_INDEX;
    }else if(sym.is_closure_nested | sym.is_captured){
        fmt = SEM_LINK;
    }else if(!sym.is_const){
        fmt = SEM_ID_FUN_IMPURE;
    }

    for(const Typeset::Selection& c : sym.document_occurences){
        symbol_table.occurence_to_symbol_map[c.left] = sym_id;
        c.format(fmt);
    }

    //DO THIS - you need warnings, some errors are pedantic
    if(!sym.is_used)
        errors.push_back(Error(sym.document_occurences.back(), UNUSED_VAR));
}

void SymbolTableBuilder::makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable){
    assert(map.find(c) == map.end());
    size_t index = ids.size();
    symbol_id_index.push_back(index);
    ids.push_back(Id({symbol_table.symbols.size()}));
    symbol_table.symbols.push_back(Symbol(pn, c, lexical_depth, closure_depth, immutable));
    map[c] = index;
}

void SymbolTableBuilder::appendEntry(size_t index, const Typeset::Selection& c, ParseNode pn, bool immutable){
    symbol_id_index.push_back(index);
    Id& id_info = ids[index];
    id_info.push_back(symbol_table.symbols.size());
    symbol_table.symbols.push_back(Symbol(pn, c, lexical_depth, closure_depth, immutable));
}

}

}
