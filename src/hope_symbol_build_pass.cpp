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
};

SymbolTableBuilder::SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model)
    : errors(model->errors), warnings(model->warnings), parse_tree(parse_tree), symbol_table(parse_tree) {}

void SymbolTableBuilder::resolveSymbols(){
    reset();
    if(!parse_tree.empty()){ // Note: symbol resolution runs DESPITE parse errors, because the editor needs it
        ParseNode n = parse_tree.root;
        for(size_t i = 0; i < parse_tree.getNumArgs(n); i++)
            resolveStmt(parse_tree.arg(n, i));
    }

    symbol_table.finalize();

    for(size_t i = 0; i < symbol_table.symbols.size(); i++)
        if(!symbol_table.symbols[i].is_used)
            warnings.push_back(Error(symbol_table.getSel(i), UNUSED_VAR));

    for(const Usage& usage : symbol_table.usages){
        const Symbol& sym = symbol_table.symbols[usage.var_id];

        SemanticType fmt = SEM_ID;
        if(sym.is_ewise_index){
            fmt = SEM_ID_EWISE_INDEX;
        }else if(sym.is_closure_nested | sym.is_captured_by_value){
            fmt = SEM_LINK;
        }else if(!sym.is_const){
            fmt = SEM_ID_FUN_IMPURE;
        }

        parse_tree.getSelection(usage.pn).format(fmt);
    }
}

void SymbolTableBuilder::reset() noexcept{
    symbol_table.reset(parse_tree.getLeft(parse_tree.root));
    map.clear();
    assert(ref_list_sets.empty());
    lexical_depth = GLOBAL_DEPTH;
    closure_depth = 0;
    active_scope_id = 0;
}

ScopeSegment& SymbolTableBuilder::activeScope() noexcept{
    return symbol_table.scopes[active_scope_id];
}

void SymbolTableBuilder::addScope(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode closure){
    symbol_table.addScope(name, begin);

    size_t sze = symbol_table.symbols.size();
    size_t n_usages = symbol_table.usages.size();
    symbol_table.scopes[active_scope_id].sym_end = sze;
    symbol_table.scopes[active_scope_id].usage_end = n_usages;
    active_scope_id++;
    symbol_table.scopes[active_scope_id].sym_begin = sze;
    symbol_table.scopes[active_scope_id].usage_begin = n_usages;
    symbol_table.scopes[active_scope_id].fn = closure;
}

void SymbolTableBuilder::closeScope(const Typeset::Marker& end) noexcept{
    symbol_table.closeScope(end);

    size_t sze = symbol_table.symbols.size();
    size_t n_usages = symbol_table.usages.size();
    symbol_table.scopes[active_scope_id].sym_end = sze;
    symbol_table.scopes[active_scope_id].usage_end = n_usages;
    active_scope_id++;
    if(active_scope_id < symbol_table.scopes.size()){
        symbol_table.scopes[active_scope_id].sym_begin = sze;
        symbol_table.scopes[active_scope_id].usage_begin = n_usages;
    }
}

Symbol& SymbolTableBuilder::lastDefinedSymbol() noexcept{
    return symbol_table.symbols.back();
}

size_t SymbolTableBuilder::symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept{
    auto lookup = map.find(sel);
    return lookup == map.end() ? NONE : lookup->second;
}

void SymbolTableBuilder::resolveStmt(ParseNode pn){
    switch (parse_tree.getOp(pn)) {
        case OP_EQUAL: resolveEquality(pn); break;
        case OP_ASSIGN: resolveAssignment(pn); break;
        case OP_BLOCK: resolveBlock(pn); break;
        case OP_ALGORITHM: resolveAlgorithm(pn); break;
        case OP_PROTOTYPE_ALG: resolvePrototype(pn); break;
        case OP_WHILE: resolveConditional1(symbol_table.whileSel(), pn); break;
        case OP_IF: resolveConditional1(symbol_table.ifSel(), pn); break;
        case OP_IF_ELSE: resolveConditional2(pn); break;
        case OP_FOR: resolveFor(pn); break;
        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveExpr(ParseNode pn){
    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER: resolveReference<true>(pn); break;
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
        size_t sym_index = lookup->second;
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
        if(type == OP_IDENTIFIER && !declared(sub)){
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
        increaseLexicalDepth(symbol_table.ewise(), parse_tree.getLeft(pn));
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

        decreaseLexicalDepth(parse_tree.getRight(pn));
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

    if(parse_tree.getOp(id) != OP_IDENTIFIER ||
       (parse_tree.getOp(sub) != OP_IDENTIFIER && parse_tree.getOp(sub) != OP_INTEGER_LITERAL) ||
       (declared(id) && declared(sub)))
        return false;

    parse_tree.setOp(pn, OP_IDENTIFIER);

    return true;
}

template <bool allow_imp_mult>
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
        }else if(allow_imp_mult){
            if(parse_tree.getOp(pn) == OP_IDENTIFIER){
                const Typeset::Marker left = parse_tree.getLeft(pn);
                const Typeset::Marker right = parse_tree.getRight(pn);
                if(parse_tree.getNumArgs(pn) == 0) resolveIdMult(pn, left, right);
                else resolveScriptMult(pn, left, right);
            }
        }else{
            errors.push_back(Error(c, BAD_READ));
        }
    }else{
        resolveReference(pn, c, lookup->second);
    }
}

void SymbolTableBuilder::resolveReference(ParseNode pn, const Typeset::Selection& c, size_t sym_id){
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    Symbol& sym = symbol_table.symbols[sym_id];
    parse_tree.setFlag(pn, sym.flag);
    sym.is_used = true;

    symbol_table.addOccurence(parse_tree.getLeft(pn), sym_id);
    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    symbol_table.usages.push_back(Usage(sym_id, pn, READ));
}

void SymbolTableBuilder::resolveIdMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right){
    Typeset::Marker m = left;
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->size()) m = right;

        auto lookup = map.find(Typeset::Selection(left, m));
        if(lookup == map.end()){
            errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ));
            return;
        }
        left = m;
    }

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_IMPLICIT_MULTIPLY);
    left = parse_tree.getLeft(pn);
    m = left;
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->size()) m = right;

        Typeset::Selection sel(left, m);
        auto lookup = map.find(sel);
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        resolveReference(pn, sel, lookup->second);
        builder.addNaryChild(pn);
        left = m;
    }
    ParseNode mult = builder.finalize();

    parse_tree.setFlag(pn, mult);
    parse_tree.setOp(pn, OP_PROXY);
}

void SymbolTableBuilder::resolveScriptMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right){
    assert(left.text != right.text);

    Typeset::Marker m = left;
    Typeset::Marker end(m.text, m.text->size());
    Typeset::Marker new_right = end;
    end.decrementGrapheme();
    if(left == end){
        errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ));
        return;
    }

    while(m != end){
        m.incrementGrapheme();
        auto lookup = map.find(Typeset::Selection(left, m));
        if(lookup == map.end()){
            errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ));
            return;
        }
        left = m;
    }

    ParseNode new_id = parse_tree.addTerminal(OP_IDENTIFIER, Typeset::Selection(end, new_right));
    ParseTree::NaryBuilder script_builder = parse_tree.naryBuilder(parse_tree.getOp(pn));
    script_builder.addNaryChild(new_id);
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
        script_builder.addNaryChild(parse_tree.arg(pn, i));
    ParseNode script = script_builder.finalize(Typeset::Selection(end, right));
    resolveExpr(script);

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_IMPLICIT_MULTIPLY);
    left = parse_tree.getLeft(pn);
    m = left;
    while(m != end){
        m.incrementGrapheme();
        Typeset::Selection sel(left, m);
        auto lookup = map.find(sel);
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        resolveReference(pn, sel, lookup->second);
        builder.addNaryChild(pn);
        left = m;
    }
    builder.addNaryChild(script);
    ParseNode mult = builder.finalize();

    parse_tree.setFlag(pn, mult);
    parse_tree.setOp(pn, OP_PROXY);
}

void SymbolTableBuilder::resolveConditional1(const Typeset::Selection& name, ParseNode pn){
    resolveExpr( parse_tree.arg(pn, 0) );
    resolveBody( name, parse_tree.arg(pn, 1) );
}

void SymbolTableBuilder::resolveConditional2(ParseNode pn){
    resolveExpr( parse_tree.arg(pn, 0) );
    resolveBody( symbol_table.ifSel(), parse_tree.arg(pn, 1) );
    resolveBody( symbol_table.elseSel(), parse_tree.arg(pn, 2) );
}

void SymbolTableBuilder::resolveFor(ParseNode pn){
    increaseLexicalDepth(symbol_table.forSel(), parse_tree.getLeft(parse_tree.arg(pn, 1)));
    resolveStmt(parse_tree.arg(pn, 0));
    resolveExpr(parse_tree.arg(pn, 1));
    resolveStmt(parse_tree.arg(pn, 2));
    resolveStmt(parse_tree.arg(pn, 3));
    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolTableBuilder::resolveBody(const Typeset::Selection& name, ParseNode pn){
    increaseLexicalDepth(name, parse_tree.getLeft(pn));
    resolveStmt(pn);
    decreaseLexicalDepth(parse_tree.getRight(pn));

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
    increaseClosureDepth(symbol_table.lambda(), parse_tree.getLeft(pn), pn);

    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++)
        defineLocalScope( parse_tree.arg(params, i) );

    ParseNode rhs = parse_tree.arg(pn, 3);
    resolveExpr(rhs);

    decreaseClosureDepth(parse_tree.getRight(pn));
}

void SymbolTableBuilder::resolveAlgorithm(ParseNode pn){
    ParseNode name = parse_tree.algName(pn);
    ParseNode val_cap = parse_tree.valCapList(pn);
    ParseNode params = parse_tree.paramList(pn);
    ParseNode body = parse_tree.body(pn);

    const Typeset::Selection sel = parse_tree.getSelection(name);
    auto lookup = map.find(sel);
    if(lookup == map.end()){
        makeEntry(sel, name, true);
        parse_tree.setFlag(pn, NONE); //This is a kludge to tell the interpreter it's not a prototype
    }else{
        parse_tree.setFlag(pn, size_t(0));
        size_t index = lookup->second;
        Symbol& sym = symbol_table.symbols[index];
        if(sym.declaration_lexical_depth == lexical_depth){
            if(sym.is_prototype){
                resolveReference(name, sel, index);
            }else{
                errors.push_back(Error(sel, TYPE_ERROR));
            }
        }else if(sym.declaration_lexical_depth == lexical_depth){
            appendEntry(index, name, lookup->second, true);
            lookup->second = symbol_table.symbols.size()-1;
        }

        sym.is_prototype = false;
    }

    size_t val_cap_size = parse_tree.valListSize(val_cap);
    if(val_cap != ParseTree::EMPTY){
        parse_tree.setFlag(val_cap, symbol_table.scopes.size());
        for(size_t i = 0; i < val_cap_size; i++){
            ParseNode capture = parse_tree.arg(val_cap, i);
            resolveReference(capture);
        }
    }

    increaseClosureDepth(parse_tree.getSelection(name), parse_tree.getLeft(body), pn);

    for(size_t i = 0; i < val_cap_size; i++){
        ParseNode capture = parse_tree.arg(val_cap, i);
        defineLocalScope(capture, false);
        symbol_table.symbols.back().is_captured_by_value = true;
        symbol_table.symbols.back().is_closure_nested = true;
    }

    bool expect_default = false;
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL){
            resolveExpr(parse_tree.rhs(param));
            param = parse_tree.lhs(param);
            expect_default = true;
        }else if(expect_default){
            errors.push_back(Error(parse_tree.getSelection(param), EXPECT_DEFAULT_ARG));
        }

        bool success = defineLocalScope( param, false );
        if(!success){
            errors.pop_back();
            errors.push_back(Error(parse_tree.getSelection(param), REDEFINE_PARAMETER));
            parse_tree.setFlag(param, NONE);
        }
    }

    resolveStmt(body);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        size_t sym_id = parse_tree.getFlag(param);
        if(sym_id != NONE){
            Symbol& sym = symbol_table.symbols[sym_id];
            sym.is_const = !sym.is_reassigned;
        }
    }

    decreaseClosureDepth(parse_tree.getRight(body));
}

void SymbolTableBuilder::resolvePrototype(ParseNode pn){
    if(defineLocalScope(parse_tree.child(pn)))
        lastDefinedSymbol().is_prototype = true;
}

void SymbolTableBuilder::resolveSubscript(ParseNode pn){
    if(parse_tree.getNumArgs(pn) != 2){ resolveDefault(pn); return; }

    ParseNode id = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    bool lhs_type_eligible = (parse_tree.getOp(id) == OP_IDENTIFIER);
    bool rhs_type_eligible = (parse_tree.getOp(rhs) == OP_IDENTIFIER || parse_tree.getOp(rhs) == OP_INTEGER_LITERAL);
    #define UNDECLARED_ELEMS (!declared(id) || (parse_tree.getOp(rhs) == OP_IDENTIFIER && !declared(rhs)))

    if(lhs_type_eligible && rhs_type_eligible && UNDECLARED_ELEMS){
        parse_tree.setOp(pn, OP_IDENTIFIER);
        resolveReference<true>(pn);
    }else{
        resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveBig(ParseNode pn){
    increaseLexicalDepth(symbol_table.big(), parse_tree.getLeft(pn));
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

    decreaseLexicalDepth(parse_tree.getRight(pn));
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
        Symbol& sym = symbol_table.symbols[index];
        if(sym.declaration_lexical_depth == lexical_depth){
            bool CONST = sym.is_const;
            errors.push_back(Error(c, CONST ? REASSIGN_CONSTANT : MUTABLE_CONST_ASSIGN));
            return false;
        }else{
            appendEntry(index, pn, lookup->second, immutable);
            lookup->second = symbol_table.symbols.size()-1;
        }
    }

    return true;
}

bool SymbolTableBuilder::declared(ParseNode pn) const noexcept{
    return map.find(parse_tree.getSelection(pn)) != map.end();
}

void SymbolTableBuilder::increaseLexicalDepth(const Typeset::Selection& name, const Typeset::Marker& begin){
    lexical_depth++;
    addScope(name, begin);
}

void SymbolTableBuilder::decreaseLexicalDepth(const Typeset::Marker& end){
    closeScope(end);

    for(size_t curr = symbol_table.head(active_scope_id-1); curr < active_scope_id; curr = symbol_table.scopes[curr].next){
        ScopeSegment& scope = symbol_table.scopes[curr];
        for(size_t sym_id = scope.sym_begin; sym_id < scope.sym_end; sym_id++){
            Symbol& sym = symbol_table.symbols[sym_id];
            assert(sym.declaration_lexical_depth == lexical_depth);

            if(sym.shadowed_var == NONE)
                map.erase(sym.sel(parse_tree)); //Much better to erase empty entries than check for them
            else
                map[sym.sel(parse_tree)] = sym.shadowed_var;
        }
    }

    lexical_depth--;
}

void SymbolTableBuilder::increaseClosureDepth(const Typeset::Selection& name, const Typeset::Marker& begin, ParseNode pn){
    ref_list_sets.push_back(std::unordered_set<ParseNode>());

    closure_depth++;
    lexical_depth++;
    addScope(name, begin, pn);
}

void SymbolTableBuilder::decreaseClosureDepth(const Typeset::Marker& end){
    ParseNode fn = symbol_table.scopes.back().fn;

    decreaseLexicalDepth(end);
    closure_depth--;

    std::unordered_set<size_t>& closed_refs = ref_list_sets.back();

    for(size_t seg_index = symbol_table.scopes.size()-2; seg_index != NONE; seg_index = symbol_table.scopes[seg_index].prev){
        ScopeSegment& closed_seg = symbol_table.scopes[seg_index];
        for(size_t i = closed_seg.usage_begin; i < closed_seg.usage_end; i++){
            const Usage& usage = symbol_table.usages[i];
            const Symbol& sym = symbol_table.symbols[usage.var_id];
            if(usage.type != UsageType::DECLARE &&
               sym.is_closure_nested &&
               (!sym.is_captured_by_value || sym.declaration_closure_depth <= closure_depth))
                closed_refs.insert(usage.var_id);
        }
    }

    if(!closed_refs.empty()){
        if(ref_list_sets.size() >= 2){
            std::unordered_set<size_t>& returning_refs = ref_list_sets[ref_list_sets.size()-2];
            for(size_t sym_id : closed_refs){
                const Symbol& sym = symbol_table.symbols[sym_id];
                if(sym.declaration_closure_depth <= (closure_depth - sym.is_captured_by_value))
                    returning_refs.insert(sym_id);
            }
        }

        ParseTree::NaryBuilder ref_builder = parse_tree.naryBuilder(OP_LIST);
        for(size_t sym_id : closed_refs){
            Op op = symbol_table.symbols[sym_id].declaration_closure_depth <= closure_depth ?
                    OP_READ_UPVALUE :
                    OP_IDENTIFIER;
            Typeset::Selection sel = symbol_table.getSel(sym_id);
            assert(sel.left != sel.right);
            ParseNode n = parse_tree.addTerminal(op, sel);
            assert(parse_tree.getSelection(n) == sel);
            parse_tree.setFlag(n, sym_id);
            ref_builder.addNaryChild(n);
        }
        ParseNode list = ref_builder.finalize(parse_tree.getSelection(fn));
        parse_tree.setRefList(fn, list);
    }else{
        parse_tree.setRefList(fn, parse_tree.addTerminal(OP_LIST, parse_tree.getSelection(fn)));
    }

    ref_list_sets.pop_back();
}

void SymbolTableBuilder::makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable){
    assert(map.find(c) == map.end());
    map[c] = symbol_table.symbols.size();
    symbol_table.usages.push_back(Usage(symbol_table.symbols.size(), pn, DECLARE));
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, NONE, immutable);
}

void SymbolTableBuilder::appendEntry(size_t index, ParseNode pn, size_t prev, bool immutable){
    warnings.push_back(Error(parse_tree.getSelection(pn), SHADOWING_VAR)); //EVENTUALLY: make this optional, probably default off
    symbol_table.usages.push_back(Usage(symbol_table.symbols.size(), pn, DECLARE));
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, prev, immutable);
}

}

}
