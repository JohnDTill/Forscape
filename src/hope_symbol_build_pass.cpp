#include "hope_symbol_build_pass.h"

#include "hope_parsenodetype.h"
#include "typeset_model.h"
#include <algorithm>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

const std::unordered_map<std::string_view, ParseNodeType> SymbolTableBuilder::predef {
    {"π", PN_PI},
    {"e", PN_EULERS_NUMBER},
    {"φ", PN_GOLDEN_RATIO},
    {"c", PN_SPEED_OF_LIGHT},
    {"ℎ", PN_PLANCK_CONSTANT},
    {"ℏ", PN_REDUCED_PLANCK_CONSTANT},
    {"σ", PN_STEFAN_BOLTZMANN_CONSTANT},
    {"I", PN_IDENTITY_AUTOSIZE},
    //{"Γ", _}, //DO THIS - should have the error function
};

SymbolTableBuilder::SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model)
    : errors(model->errors), parse_tree(parse_tree) {}

void SymbolTableBuilder::resolveSymbols(){
    reset();

    ParseNode n = parse_tree.back();
    for(size_t i = 0; i < parse_tree.numArgs(n); i++)
        resolveStmt(parse_tree.arg(n, i));

    decreaseLexicalDepth();

    stack_frame.clear();
    stack_size = 0;
    link(); //This should be in a separate pass after optimisation
}

void SymbolTableBuilder::link(size_t scope_index){
    if(scope_index == NONE) return;

    const Scope& scope = scopes[scope_index];

    if(scope.closing_function != NONE){
        closures.push_back(std::unordered_map<size_t, size_t>());
        closure_size.push_back(0);
    }

    stack_frame.push_back(stack_size);

    for(const Scope::Subscope& subscope : scope.subscopes){
        for(const Scope::Usage& usage : subscope.usages){
            Symbol& sym = symbols[usage.var_id];
            ParseNode pn = usage.pn;

            if(usage.type == Scope::DECLARE){
                if(sym.is_closure_nested && !sym.is_captured){
                    assert(sym.declaration_closure_depth < closures.size());

                    parse_tree.setUpvalue(pn, closure_size.back());
                    closures.back()[usage.var_id] = closure_size.back()++;
                    parse_tree.setEnum(pn, PN_READ_UPVALUE);
                }else{
                    sym.stack_index = stack_size++;
                }
            }else{
                if(sym.is_closure_nested){
                    assert(sym.declaration_closure_depth < closures.size());

                    for(size_t i = sym.declaration_closure_depth; i < closures.size(); i++){
                        std::unordered_map<size_t, size_t>& closure = closures[i];
                        if(closure.find(usage.var_id) == closure.end())
                            closure[usage.var_id] = closure_size[i]++;
                    }
                    parse_tree.setUpvalue(pn, closures.back()[usage.var_id]);
                    parse_tree.setEnum(pn, PN_READ_UPVALUE);
                }else if(sym.declaration_closure_depth == 0){
                    parse_tree.setStackOffset(pn, sym.stack_index);
                    parse_tree.setEnum(pn, PN_READ_GLOBAL);
                }else{
                    parse_tree.setStackOffset(pn, stack_size - 1 - sym.stack_index);
                }
            }
        }

        link(subscope.subscope_id);
    }

    if(scope.closing_function != NONE){
        ParseNode fn = scope.closing_function;

        ParseTree::NaryBuilder upvalue_builder = parse_tree.naryBuilder(PN_LIST);
        for(const auto& entry : closures.back()){
            size_t symbol_index = entry.first;
            const Symbol& sym = symbols[symbol_index];
            ParseNode n = parse_tree.addTerminal(PN_IDENTIFIER, sym.document_occurences->front());

            parse_tree.setUpvalue(n, entry.second);

            if(sym.declaration_closure_depth != closures.size()-1)
                parse_tree.setEnum(n, PN_READ_UPVALUE);

            upvalue_builder.addNaryChild(n);
        }

        ParseNode upvalue_list = upvalue_builder.finalize(parse_tree.getSelection(fn));

        switch (parse_tree.getEnum(fn)) {
            case PN_ALGORITHM:
                parse_tree.setArg(fn, 2, upvalue_list);
                break;
            case PN_LAMBDA:
                parse_tree.setArg(fn, 0, upvalue_list);
                break;
        }

        closures.pop_back();
        closure_size.pop_back();
    }

    stack_size = stack_frame.back();
    stack_frame.pop_back();
}

void SymbolTableBuilder::reset() noexcept{
    ids.clear();
    map.clear();
    doc_map.clear();
    scopes.clear();
    symbols.clear();
    symbol_id_index.clear();
    lexical_depth = GLOBAL_DEPTH;
    closure_depth = 0;
    scopes.push_back(Scope(false, NONE));
    active_scope_id = 0;
    stack_size = 0;
    stack_frame.clear();
}

void SymbolTableBuilder::resolveStmt(ParseNode pn){
    switch (parse_tree.getEnum(pn)) {
        case PN_EQUAL: resolveEquality(pn); break;
        case PN_ASSIGN: resolveAssignment(pn); break;
        case PN_BLOCK: resolveBlock(pn); break;
        case PN_ALGORITHM: resolveAlgorithm(pn); break;
        case PN_PROTOTYPE_ALG: resolvePrototype(pn); break;

        case PN_WHILE:
        case PN_IF:
            resolveConditional1(pn); break;

        case PN_IF_ELSE: resolveConditional2(pn); break;
        case PN_FOR: resolveFor(pn); break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveExpr(ParseNode pn){
    switch (parse_tree.getEnum(pn)) {
        case PN_IDENTIFIER: resolveReference(pn); break;
        case PN_LAMBDA: resolveLambda(pn); break;
        case PN_SUBSCRIPT_ACCESS: resolveSubscript(pn); break;

        case PN_SUMMATION:
        case PN_PRODUCT:
            resolveBig(pn);
            break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveEquality(ParseNode pn){
    if(parse_tree.numArgs(pn) > 2){
        errors.push_back(Error(parse_tree.getSelection(pn), ErrorCode::TYPE_ERROR));
    }else{
        resolveExpr(parse_tree.rhs(pn));
        defineLocalScope(parse_tree.lhs(pn));
    }
}

void SymbolTableBuilder::resolveAssignment(ParseNode pn){
    assert(parse_tree.numArgs(pn) == 2);

    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    switch(parse_tree.getEnum( lhs )){
        case PN_IDENTIFIER:
            resolveExpr(rhs);
            resolveAssignmentId(pn);
            break;

        case PN_SUBSCRIPT_ACCESS:
            if(map.find(parse_tree.getSelection(lhs)) != map.end()){
                resolvePotentialIdSub(lhs);
                resolveExpr(rhs);
                resolveAssignmentId(pn);
            }else{
                parse_tree.setEnum(pn, PN_REASSIGN);
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
    if(parse_tree.getEnum(id) != PN_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    auto lookup = map.find(c);
    if(lookup == map.end()){
        makeEntry(c, id, false);
    }else{
        size_t sym_index = lastSymbolIndexOfId(lookup->second);
        Symbol& sym = symbols[sym_index];
        sym.is_reassigned = true;
        parse_tree.setEnum(pn, PN_REASSIGN);
        resolveReference(id, c, sym_index);
    }
}

void SymbolTableBuilder::resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs){
    ParseNode id = parse_tree.arg(lhs, 0);

    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getEnum(id) != PN_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    size_t symbol_index = symbolIndexFromSelection(c);
    if(symbol_index != NONE){
        symbols[symbol_index].is_reassigned = true;
        resolveReference(id, c, symbol_index);
    }else{
        errors.push_back(Error(c, BAD_READ));
    }

    bool only_trivial_slice = true;
    std::vector<ParseNode> undefined_vars;
    size_t num_subscripts = parse_tree.numArgs(lhs) - 1;
    for(size_t i = 0; i < num_subscripts; i++){
        ParseNode sub = parse_tree.arg(lhs, i+1);
        ParseNodeType type = parse_tree.getEnum(sub);
        const Typeset::Selection& sel = parse_tree.getSelection(sub);
        if(type == PN_IDENTIFIER && map.find(sel) == map.end()){
            undefined_vars.push_back(sub);
        }else{
            only_trivial_slice &= (type == PN_SLICE) && parse_tree.numArgs(sub) == 1;
            resolveExpr(sub);
        }
    }

    if(!undefined_vars.empty() & only_trivial_slice){
        if(num_subscripts > 2){
            const Typeset::Selection& sel = parse_tree.getSelection(parse_tree.arg(lhs, 3));
            errors.push_back(Error(sel, INDEX_OUT_OF_RANGE));
            return;
        }

        parse_tree.setEnum(pn, PN_ELEMENTWISE_ASSIGNMENT);
        increaseLexicalDepth();
        size_t vars_start = symbols.size();
        for(ParseNode pn : undefined_vars){
            bool success = defineLocalScope(pn, false);
            assert(success);
        }
        resolveExpr(rhs);

        for(size_t i = 0; i < undefined_vars.size(); i++){
            Symbol& sym = symbols[vars_start + i];
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
    if(parse_tree.numArgs(pn) != 2) return false;

    ParseNode id = parse_tree.lhs(pn);
    ParseNode sub = parse_tree.rhs(pn);

    if(parse_tree.getEnum(id) != PN_IDENTIFIER || parse_tree.getEnum(sub) != PN_IDENTIFIER)
        return false;

    const Typeset::Selection& id_sel = parse_tree.getSelection(id);
    const Typeset::Selection& id_sub = parse_tree.getSelection(sub);

    auto lookup_id = map.find(id_sel);
    auto lookup_sub = map.find(id_sub);

    if(lookup_id != map.end() && lookup_sub != map.end()) return false;

    parse_tree.setEnum(pn, PN_IDENTIFIER);

    return true;
}

void SymbolTableBuilder::resolveReference(ParseNode pn){
    Typeset::Selection c = parse_tree.getSelection(pn);

    auto lookup = map.find(c);
    if(lookup == map.end()){
        auto lookup = predef.find(c.isTextSelection() ? c.strView() : c.str());
        if(lookup != predef.end()){
            ParseNodeType read_type = lookup->second;
            parse_tree.setEnum(pn, read_type);
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
    assert(parse_tree.getEnum(pn) == PN_IDENTIFIER);
    Symbol& sym = symbols[sym_id];
    sym.is_used = true;

    sym.document_occurences->push_back(c);
    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    activeScope().subscopes.back().usages.push_back(Scope::Usage(sym_id, pn, Scope::READ));
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

    switch (parse_tree.getEnum(pn)) {
        case PN_EQUAL:
        case PN_ASSIGN:{
            Typeset::Selection c = parse_tree.getSelection(pn);
            errors.push_back(Error(c, UNUSED_VAR));
        }
    }
}

void SymbolTableBuilder::resolveBlock(ParseNode pn){
    for(size_t i = 0; i < parse_tree.numArgs(pn); i++)
        resolveStmt( parse_tree.arg(pn, i) );
}

void SymbolTableBuilder::resolveDefault(ParseNode pn){
    for(size_t i = 0; i < parse_tree.numArgs(pn); i++)
        resolveExpr(parse_tree.arg(pn, i));
}

void SymbolTableBuilder::resolveLambda(ParseNode pn){
    increaseClosureDepth(pn);

    ParseNode params = parse_tree.arg(pn, 1);
    assert(parse_tree.getEnum(params) == PN_LIST);

    for(size_t i = 0; i < parse_tree.numArgs(params); i++)
        defineLocalScope( parse_tree.arg(params, i) );

    ParseNode rhs = parse_tree.arg(pn, 2);
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
        for(size_t i = 0; i < parse_tree.numArgs(captured); i++){
            ParseNode capture = parse_tree.arg(captured, i);
            resolveReference(capture);
        }
    }

    increaseClosureDepth(pn);

    if(captured != ParseTree::EMPTY){
        size_t N = parse_tree.numArgs(captured);

        for(size_t i = 0; i < N; i++){
            ParseNode capture = parse_tree.arg(captured, i);
            defineLocalScope(capture, false);
            symbols.back().is_captured = true;
            symbols.back().is_closure_nested = true;
        }
    }

    size_t params_start = symbols.size();
    for(size_t i = 0; i < parse_tree.numArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getEnum(param) == PN_EQUAL){
            resolveExpr(parse_tree.rhs(param));
            param = parse_tree.lhs(param);
        }
        bool success = defineLocalScope( param, false );
        assert(success);
    }
    resolveStmt(body);

    bool expect_default = false;
    for(size_t i = 0; i < parse_tree.numArgs(params); i++){
        ParseNode n = parse_tree.arg(params, i);
        if(parse_tree.getEnum(n) == PN_EQUAL){
            expect_default = true;
        }else if(expect_default){
            errors.push_back(Error(parse_tree.getSelection(n), EXPECT_DEFAULT_ARG));
        }

        Symbol& sym = symbols[params_start + i];
        sym.is_const = !sym.is_reassigned;
    }

    decreaseClosureDepth();
}

void SymbolTableBuilder::resolvePrototype(ParseNode pn){
    if(defineLocalScope(parse_tree.child(pn)))
        lastDefinedSymbol().is_prototype = true;
}

void SymbolTableBuilder::resolveSubscript(ParseNode pn){
    if(parse_tree.numArgs(pn) != 2){ resolveDefault(pn); return; }

    ParseNode id = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    if(parse_tree.getEnum(id) != PN_IDENTIFIER || parse_tree.getEnum(rhs) != PN_IDENTIFIER){
        resolveDefault(pn);
        return;
    }

    const Typeset::Selection& id_sel = parse_tree.getSelection(id);
    const Typeset::Selection& rhs_sel = parse_tree.getSelection(rhs);

    if(map.find(id_sel) == map.end() || map.find(rhs_sel) == map.end()){
        parse_tree.setEnum(pn, PN_IDENTIFIER);
        resolveReference(pn);
    }else{
        resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveBig(ParseNode pn){
    increaseLexicalDepth();
    ParseNode assign = parse_tree.arg(pn, 0);
    if( parse_tree.getEnum(assign) != PN_ASSIGN ) return;
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

    if(parse_tree.getEnum(pn) == PN_SUBSCRIPT_ACCESS && !resolvePotentialIdSub(pn)){
        errors.push_back(Error(c, ASSIGN_EXPRESSION));
        return false;
    }

    if(parse_tree.getEnum(pn) != PN_IDENTIFIER){
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
    for(const Scope::Subscope& subscope : activeScope().subscopes){
        for(const Scope::Usage& usage : subscope.usages){
            if(usage.type != Scope::DECLARE) continue;
            size_t sym_id = usage.var_id;

            Id& id = ids[symbol_id_index[sym_id]];
            Symbol& sym = symbols[sym_id];
            assert(sym.declaration_lexical_depth == lexical_depth);

            finalize(sym);

            if(id.size() == 1){
                map.erase(sym.document_occurences->front()); //Much better to erase empty entries than check for them
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
    addScope(pn);
}

void SymbolTableBuilder::decreaseClosureDepth(){
    closure_depth--;
    decreaseLexicalDepth();
}

void SymbolTableBuilder::finalize(const Symbol& sym){
    assert(sym.declaration_lexical_depth == lexical_depth);

    SemanticType fmt = SEM_ID;
    if(sym.is_ewise_index){
        fmt = SEM_ID_EWISE_INDEX;
    }else if(sym.is_closure_nested | sym.is_captured){
        fmt = SEM_LINK;
    }else if(!sym.is_const){
        fmt = SEM_ID_FUN_IMPURE;
    }

    for(const Typeset::Selection& c : *sym.document_occurences){
        doc_map[c.left] = sym.document_occurences;
        c.format(fmt);
    }

    //DO THIS - you need warnings...
    //if(!sym_info.is_used)
    //    errors.push_back(Error(sym_info.occurences->back(), UNUSED_VAR));
}

void SymbolTableBuilder::makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable){
    assert(map.find(c) == map.end());
    size_t index = ids.size();
    symbol_id_index.push_back(ids.size());
    ids.push_back(Id({symbols.size()}));
    activeScope().subscopes.back().usages.push_back(Scope::Usage(symbols.size(), pn, Scope::DECLARE));
    symbols.push_back(Symbol(c, lexical_depth, closure_depth, immutable));
    map[c] = index;
}

void SymbolTableBuilder::appendEntry(size_t index, const Typeset::Selection& c, ParseNode pn, bool immutable){
    symbol_id_index.push_back(index);
    Id& id_info = ids[index];
    id_info.push_back(symbols.size());
    activeScope().subscopes.back().usages.push_back(Scope::Usage(symbols.size(), pn, Scope::DECLARE));
    symbols.push_back(Symbol(c, lexical_depth, closure_depth, immutable));
}

}

}
