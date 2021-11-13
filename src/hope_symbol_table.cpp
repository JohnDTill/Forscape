#include "hope_symbol_table.h"

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
    ids.clear();
    stack.clear();
    map.clear();
    stack_sizes.clear();
    closures.clear();
    doc_map.clear();
    lexical_depth = GLOBAL_DEPTH;

    ParseNode n = parse_tree.back();
    for(size_t i = 0; i < parse_tree.numArgs(n); i++)
        resolveStmt(parse_tree.arg(n, i));

    decreaseLexicalDepth();
}

void SymbolTableBuilder::resolveStmt(ParseNode pn){
    switch (parse_tree.getEnum(pn)) {
        case PN_EQUAL: resolveEquality(pn); break;
        case PN_ASSIGN: resolveAssignment(pn); break;
        case PN_IDENTIFIER: resolveReference(pn); break;
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
        case PN_CALL: resolveCall(pn); break;
        case PN_SUBSCRIPT_ACCESS: resolveSubscript(pn); break;

        case PN_SUMMATION:
        case PN_PRODUCT:
            resolveBig(pn);
            break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveEquality(ParseNode pn){
    if(parse_tree.numArgs(pn) > 2) return;

    resolveExpr(parse_tree.rhs(pn));
    defineLocalScope(parse_tree.lhs(pn));
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
        makeEntry(c, false);
    }else{
        size_t index = lookup->second;
        Symbol& sym_info = ids[index].back();
        sym_info.is_reassigned = true;
        parse_tree.setEnum(pn, PN_REASSIGN);
        resolveReference(id, c, index);
    }
}

void SymbolTableBuilder::resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs){
    ParseNode id = parse_tree.arg(lhs, 0);

    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getEnum(id) != PN_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    auto lookup = map.find(c);
    if(lookup == map.end()){
        errors.push_back(Error(c, BAD_READ));
    }else{
        size_t index = lookup->second;
        Symbol& sym_info = ids[index].back();
        sym_info.is_reassigned = true;
        resolveReference(id, c, index);
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
        for(ParseNode pn : undefined_vars){
            bool success = defineLocalScope(pn, false);
            assert(success);
        }
        resolveExpr(rhs);

        for(size_t i = 0; i < undefined_vars.size(); i++){
            IdIndex index = stack[stack.size()-1-i];
            ParseNode var = undefined_vars[undefined_vars.size()-1];
            ids[index].back().is_ewise_index = true;
            if(!ids[index].back().is_used){
                errors.push_back(Error(parse_tree.getSelection(var), UNUSED_ELEM_AGN_INDEX));
                ids[index].back().is_used = true;
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
    parse_tree.setStackOffset(pn, 0);

    return true;
}

void SymbolTableBuilder::resolveReference(ParseNode pn){
    Typeset::Selection c = parse_tree.getSelection(pn);

    auto lookup = map.find(c);
    if(lookup == map.end()){
        auto lookup = predef.find(c.isTextSelection() ? c.strView() : c.str());
        if(lookup != predef.end()){
            parse_tree.setEnum(pn, lookup->second);
            c.format(SEM_PREDEF);
            return;
        }
        errors.push_back(Error(c, BAD_READ));
    }else{
        size_t index = lookup->second;
        Symbol& sym_info = ids[index].back();
        sym_info.is_used = true;
        resolveReference(pn, c, index);
    }
}

void SymbolTableBuilder::resolveReference(ParseNode pn, const Typeset::Selection& c, IdIndex id_index){
    assert(parse_tree.getEnum(pn) == PN_IDENTIFIER);

    Symbol& sym = ids[id_index].back();
    sym.occurences->push_back(c);

    if(sym.closure_depth == GLOBAL_DEPTH){
        parse_tree.setStackOffset(pn, sym.stack_index);
        parse_tree.setEnum(pn, PN_READ_GLOBAL);
    }else if(sym.closure_depth == closureDepth()){
        if(sym.is_enclosed){
            auto& closure = closures.back();
            assert(closure.upvalue_indices.find(id_index) != closure.upvalue_indices.end());
            size_t upvalue_index = closure.upvalue_indices[id_index];
            parse_tree.setUpvalue(pn, upvalue_index);
            parse_tree.setEnum(pn, PN_READ_UPVALUE);
        }else{
            parse_tree.setStackOffset(pn, stack.size()-1-sym.stack_index);
        }
    }else if(!sym.is_enclosed){
        Closure& closure = closures[sym.closureIndex()];
        closure.upvalue_indices[id_index] = closure.num_upvalues++;
        for(size_t i = sym.closureIndex()+1; i <= closureIndex(); i++){
            Closure& closure = closures[i];
            closure.upvalue_indices[id_index] = closure.num_upvalues++;
            closure.upvalues.push_back({id_index, i == sym.closureIndex()+1});
        }

        size_t upvalue_index = closures.back().num_upvalues - 1;
        parse_tree.setUpvalue(pn, upvalue_index);
        parse_tree.setEnum(pn, PN_READ_UPVALUE);
        sym.is_enclosed = true;
    }else{
        parse_tree.setUpvalue(pn, getUpvalueIndex(id_index, closureIndex()));
        parse_tree.setEnum(pn, PN_READ_UPVALUE);
    }
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
        makeEntry(sel, true);
        parse_tree.setStackOffset(name, std::numeric_limits<size_t>::max());
    }else{
        size_t index = lookup->second;
        Id& id_info = ids[index];
        Symbol& sym_info = id_info.back();
        if(sym_info.lexical_depth == lexical_depth){
            if(sym_info.is_prototype){
                parse_tree.setStackOffset(name, stack.size()-1-sym_info.stack_index);
                sym_info.occurences->push_back(sel);
            }else{
                errors.push_back(Error(sel, TYPE_ERROR));
            }
        }else if(sym_info.lexical_depth == lexical_depth){
            appendEntry(index, sel, true);
        }

        sym_info.is_prototype = false;
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

        Closure& closure = closures.back();
        std::vector<IdIndex>& id_capped = closure.captured;

        for(size_t i = 0; i < N; i++){
            ParseNode capture = parse_tree.arg(captured, i);
            Typeset::Selection c = parse_tree.getSelection(capture);

            Symbol* sym;
            auto lookup = map.find(c);
            if(lookup == map.end()){
                size_t index = ids.size();
                ids.push_back(Id({Symbol(c, lexical_depth, closureDepth(), stack.size(), false)}));
                map[c] = index;
                sym = &ids.back().back();
                id_capped.push_back(index);
            }else{
                size_t index = lookup->second;
                Symbol& sym_info = ids[index].back();
                if(sym_info.lexical_depth == lexical_depth){
                    //Capture listed twice
                    errors.push_back(Error(c, MUTABLE_CONST_ASSIGN));
                    break;
                }else{
                    Id& id_info = ids[index];
                    id_info.push_back(Symbol(c, lexical_depth, closureDepth(), stack.size(), false));
                    sym = &id_info.back();
                    id_capped.push_back(index);
                }
            }
            sym->is_enclosed = true;
            sym->is_captured = true;
            closure.upvalue_indices[id_capped.back()] = closure.num_upvalues++;
        }
    }

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

        size_t index = stack[stack.size()-1-i];
        Symbol& sym_info = ids[index].back();
        sym_info.is_const = !sym_info.is_reassigned;
    }

    decreaseClosureDepth();
}

void SymbolTableBuilder::resolvePrototype(ParseNode pn){
    if(defineLocalScope(parse_tree.child(pn)))
        ids[stack.back()].back().is_prototype = true;
}

void SymbolTableBuilder::resolveCall(ParseNode pn){
    resolveExpr( parse_tree.arg(pn, 0) );

    size_t nargs = parse_tree.numArgs(pn)-1;
    for(size_t i = 0; i < nargs; i++){
        resolveExpr(parse_tree.arg(pn, i+1));
        static constexpr size_t DUMMY_ARG = 0;
        stack.push_back(DUMMY_ARG);
    }

    stack.resize(stack.size() - nargs);
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
        parse_tree.setStackOffset(pn, 0);
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
    ids[stack.back()].back().is_used = true;
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
        makeEntry(c, immutable);
    }else{
        size_t index = lookup->second;
        Symbol& sym_info = ids[index].back();
        if(sym_info.lexical_depth == lexical_depth){
            bool CONST = sym_info.is_const;
            errors.push_back(Error(c, CONST ? REASSIGN_CONSTANT : MUTABLE_CONST_ASSIGN));
            return false;
        }else{
            appendEntry(index, c, immutable);
        }
    }

    return true;
}

void SymbolTableBuilder::increaseLexicalDepth(){
    lexical_depth++;
}

void SymbolTableBuilder::decreaseLexicalDepth(){
    size_t i = stack.size();
    while(i-->0){
        size_t index = stack[i];
        Id& id_info = ids[index];
        Symbol& sym_info = id_info.back();
        if(sym_info.lexical_depth < lexical_depth) break;

        finalize(sym_info);

        if(id_info.size() == 1){
            map.erase(sym_info.occurences->front());
            Id().swap(id_info); //Frees memory allocated to id_info
            //The entry in id_infos is left stranded, but that's
            //preferable to re-indexing.
        }else{
            id_info.pop_back();
        }
    }

    stack.resize(i+1);

    lexical_depth--;
}

void SymbolTableBuilder::increaseClosureDepth(ParseNode pn){
    closures.push_back(Closure(pn));
    stack_sizes.push_back(stack.size());
    increaseLexicalDepth();
}

void SymbolTableBuilder::decreaseClosureDepth(){
    stack_sizes.pop_back();

    for(size_t index : closures.back().captured){
        Id& id = ids[index];
        Symbol& sym = id.back();
        finalize(sym);

        if(id.size() == 1){
            map.erase(id.front().occurences->front());
            Id().swap(id); //Frees memory allocated to id_info
            //The entry in id_infos is left stranded, but that's
            //preferable to re-indexing.
        }else{
            id.pop_back();
        }
    }

    decreaseLexicalDepth();

    ParseNode fn = closures.back().fn;

    ParseTree::NaryBuilder upvalue_builder = parse_tree.naryBuilder(PN_LIST);
    for(const auto& entry : closures.back().upvalues){
        IdIndex id_index = entry.first;
        const Symbol& sym = ids[id_index].back();
        ParseNode n = parse_tree.addTerminal(PN_IDENTIFIER, sym.occurences->front());

        size_t prev_closure_index = closureIndex()-1;
        if(entry.second){
            parse_tree.setStackOffset(n, stack.size()-1-sym.stack_index);
        }else{
            auto& map = closures[prev_closure_index].upvalue_indices;
            assert(map.find(id_index) != map.end());
            parse_tree.setUpvalue(n, map[id_index]);
            parse_tree.setEnum(n, PN_READ_UPVALUE);
        }

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
}

size_t SymbolTableBuilder::getUpvalueIndex(IdIndex id_index, size_t closure_index){
    Closure& closure = closures[closure_index];
    Symbol& sym = ids[id_index].back();
    std::unordered_map<IdIndex, size_t>& upvalue_indices = closure.upvalue_indices;
    auto lookup = upvalue_indices.find(id_index);
    if(lookup != upvalue_indices.end()){
        return lookup->second;
    }else if(sym.closureIndex() == closureIndex()){
        assert(sym.is_captured);
        assert(upvalue_indices.find(id_index) != upvalue_indices.end());
        return upvalue_indices[id_index];
    }else{
        upvalue_indices[id_index] = closure.num_upvalues;
        closure.upvalues.push_back({id_index, false});

        for(size_t i = closure_index-1; i > sym.closureIndex(); i--){
            Closure& closure = closures[closure_index];
            std::unordered_map<IdIndex, size_t>& upvalue_indices = closure.upvalue_indices;
            if(upvalue_indices.find(id_index) == upvalue_indices.end()){
                upvalue_indices[id_index] = closure.num_upvalues++;
                closures[i+1].upvalues.push_back({id_index, false});
            }else{
                break;
            }
        }

        return closure.num_upvalues++;
    }
}

void SymbolTableBuilder::finalize(const Symbol& sym_info){
    assert(sym_info.lexical_depth == lexical_depth);

    SemanticType fmt = SEM_ID;
    if(sym_info.is_ewise_index){
        fmt = SEM_ID_EWISE_INDEX;
    }else if(sym_info.is_enclosed){
        fmt = SEM_LINK;
    }else if(!sym_info.is_const){
        fmt = SEM_ID_FUN_IMPURE;
    }

    for(const Typeset::Selection& c : *sym_info.occurences){
        doc_map[c.left] = sym_info.occurences;
        c.format(fmt);
    }

    //DO THIS - you need warnings...
    //if(!sym_info.is_used)
    //    errors.push_back(Error(sym_info.occurences->back(), UNUSED_VAR));
}

void SymbolTableBuilder::makeEntry(const Typeset::Selection& c, bool immutable){
    assert(map.find(c) == map.end());
    size_t index = ids.size();
    ids.push_back(Id({Symbol(c, lexical_depth, closureDepth(), stack.size(), immutable)}));
    stack.push_back(index);
    map[c] = index;
}

void SymbolTableBuilder::appendEntry(size_t index, const Typeset::Selection& c, bool immutable){
    Id& id_info = ids[index];
    id_info.push_back(Symbol(c, lexical_depth, closureDepth(), stack.size(), immutable));
    stack.push_back(index);
}

}

}
