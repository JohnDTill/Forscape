#include "hope_symbol_build_pass.h"

#include <code_parsenode_ops.h>
#include <hope_common.h>
#include "typeset_model.h"
#include "typeset_construct.h"
#include <algorithm>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

HOPE_STATIC_MAP<std::string_view, Op> SymbolTableBuilder::predef {
    {"π", OP_PI},
    {"e", OP_EULERS_NUMBER},
    {"φ", OP_GOLDEN_RATIO},
    {"c", OP_SPEED_OF_LIGHT},
    {"ℎ", OP_PLANCK_CONSTANT},
    {"ℏ", OP_REDUCED_PLANCK_CONSTANT},
    {"σ", OP_STEFAN_BOLTZMANN_CONSTANT},
    {"I", OP_IDENTITY_AUTOSIZE},
};

SymbolTableBuilder::SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model) noexcept
    : errors(model->errors), warnings(model->warnings), parse_tree(parse_tree), model(model), symbol_table(parse_tree) {}

void SymbolTableBuilder::resolveSymbols() alloc_except {
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

        assert(parse_tree.getOp(usage.pn) != OP_IDENTIFIER ||
            symbol_table.getSel(parse_tree.getSymId(usage.pn)) == sym.sel(parse_tree));

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

    #ifndef NDEBUG
    if(!errors.empty()) return;
    assert(parse_tree.inFinalState());

    #ifndef HOPE_TYPESET_HEADLESS
    static std::unordered_set<ParseNode> doc_map_nodes;
    doc_map_nodes.clear();

    struct hash {
        size_t operator() (const Typeset::Selection& a) const noexcept {
            return reinterpret_cast<size_t>(a.left.text) ^ (a.left.index);
        }
    };

    struct cmp {
        bool operator() (const Typeset::Selection& a, const Typeset::Selection& b) const noexcept {
            return a.left == b.left && a.right == b.right;
        }
    };

    static std::unordered_set<Typeset::Selection, hash, cmp> selection_in_map;
    selection_in_map.clear();

    model->populateDocMapParseNodes(doc_map_nodes);

    //Every identifier in the doc map goes to a valid symbol
    for(ParseNode pn : doc_map_nodes)
        if(parse_tree.getOp(pn) == OP_IDENTIFIER){
            symbol_table.verifyIdentifier(pn);
            selection_in_map.insert(parse_tree.getSelection(pn));
        }

    //Every usage in the symbol table is in the doc map
    for(const Usage& usage : symbol_table.usages)
        assert(selection_in_map.find(parse_tree.getSelection(usage.pn)) != selection_in_map.end());
    #endif
    #endif
}

void SymbolTableBuilder::reset() noexcept {
    symbol_table.reset(parse_tree.getLeft(parse_tree.root));
    //assert(map.empty()); //Not necessarily empty due to errors
    map.clear();
    assert(refs.empty());
    assert(ref_frames.empty());
    lexical_depth = GLOBAL_DEPTH;
    closure_depth = 0;
    active_scope_id = 0;
}

ScopeSegment& SymbolTableBuilder::activeScope() noexcept{
    return symbol_table.scopes[active_scope_id];
}

void SymbolTableBuilder::addScope(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode closure) alloc_except {
    symbol_table.addScope(SCOPE_NAME(name) begin);

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

void SymbolTableBuilder::resolveStmt(ParseNode pn) alloc_except {
    switch (parse_tree.getOp(pn)) {
        case OP_EQUAL: resolveEquality(pn); break;
        case OP_ASSIGN: resolveAssignment(pn); break;
        case OP_BLOCK: resolveBlock(pn); break;
        case OP_ALGORITHM: resolveAlgorithm(pn); break;
        case OP_PROTOTYPE_ALG: resolvePrototype(pn); break;
        case OP_WHILE: resolveConditional1(SCOPE_NAME("-while-")  pn); break;
        case OP_IF: resolveConditional1(SCOPE_NAME("-if-")  pn); break;
        case OP_IF_ELSE: resolveConditional2(pn); break;
        case OP_FOR: resolveFor(pn); break;
        case OP_RETURN:
        case OP_RETURN_EMPTY:
            if(closure_depth == 0) errors.push_back(Error(parse_tree.getSelection(pn), RETURN_OUTSIDE_FUNCTION));
            resolveDefault(pn);
            break;
        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveExpr(ParseNode pn) alloc_except {
    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER: resolveReference<true>(pn); break;
        case OP_LAMBDA: resolveLambda(pn); break;
        case OP_SUBSCRIPT_ACCESS: resolveSubscript(pn); break;

        case OP_SUMMATION:
        case OP_PRODUCT:
            resolveBig(pn);
            break;

        case OP_DERIVATIVE:
        case OP_PARTIAL:
            resolveDerivative(pn);
            break;

        default: resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveEquality(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) > 2){
        errors.push_back(Error(parse_tree.getSelection(pn), ErrorCode::TYPE_ERROR));
    }else{
        resolveExpr(parse_tree.rhs(pn));
        defineLocalScope(parse_tree.lhs(pn));
    }
}

void SymbolTableBuilder::resolveAssignment(ParseNode pn) alloc_except {
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

void SymbolTableBuilder::resolveAssignmentId(ParseNode pn) alloc_except {
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
        if(sym.is_const){
            errors.push_back(Error(c, REASSIGN_CONSTANT));
        }else{
            sym.is_reassigned = true;
            parse_tree.setOp(pn, OP_REASSIGN);
            resolveReference(id, sym_index);
        }
    }
}

void SymbolTableBuilder::resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs) alloc_except {
    ParseNode id = parse_tree.arg<0>(lhs);

    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getOp(id) != OP_IDENTIFIER){
        errors.push_back(Error(c, NON_LVALUE));
        return;
    }

    size_t symbol_index = symbolIndexFromSelection(c);
    if(symbol_index != NONE){
        symbol_table.symbols[symbol_index].is_reassigned = true;
        resolveReference(id, symbol_index);
    }else{
        errors.push_back(Error(c, BAD_READ));
    }

    bool only_trivial_slice = true;
    assert(potential_loop_vars.empty()); //Should be non-recursive, should be cleared after use
    size_t num_subscripts = parse_tree.getNumArgs(lhs) - 1;
    for(size_t i = 0; i < num_subscripts; i++){
        ParseNode sub = parse_tree.arg(lhs, i+1);
        Op type = parse_tree.getOp(sub);
        if(type == OP_IDENTIFIER && !declared(sub)){
            potential_loop_vars.push_back(sub);
        }else{
            only_trivial_slice &= (type == OP_SLICE) && parse_tree.getNumArgs(sub) == 1;
            resolveExpr(sub);
        }
    }

    if(!potential_loop_vars.empty() & only_trivial_slice){
        if(num_subscripts > 2){
            const Typeset::Selection& sel = parse_tree.getSelection(parse_tree.arg<3>(lhs));
            errors.push_back(Error(sel, INDEX_OUT_OF_RANGE));
            potential_loop_vars.clear();
            return;
        }

        parse_tree.setOp(pn, OP_ELEMENTWISE_ASSIGNMENT);
        increaseLexicalDepth(SCOPE_NAME("-ewise assign-")  parse_tree.getLeft(pn));
        size_t vars_start = symbol_table.symbols.size();
        for(ParseNode pn : potential_loop_vars){
            bool success = defineLocalScope(pn, false);
            (void)success;
            assert(success);
        }
        resolveExpr(rhs);

        for(size_t i = 0; i < potential_loop_vars.size(); i++){
            Symbol& sym = symbol_table.symbols[vars_start + i];
            ParseNode var = potential_loop_vars[potential_loop_vars.size()-1];
            sym.is_ewise_index = true;
            if(!sym.is_used){
                errors.push_back(Error(parse_tree.getSelection(var), UNUSED_ELEM_AGN_INDEX));
                sym.is_used = true;
            }
        }

        decreaseLexicalDepth(parse_tree.getRight(pn));
        potential_loop_vars.clear();
    }else if(!potential_loop_vars.empty()){
        for(ParseNode pn : potential_loop_vars)
            errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ_OR_SUBSCRIPT));
        potential_loop_vars.clear();
    }else{
        resolveExpr(rhs);
    }
}

bool SymbolTableBuilder::resolvePotentialIdSub(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) != 2) return false;

    ParseNode id = parse_tree.lhs(pn);
    ParseNode sub = parse_tree.rhs(pn);

    if(parse_tree.getOp(id) != OP_IDENTIFIER ||
       (parse_tree.getOp(sub) != OP_IDENTIFIER && parse_tree.getOp(sub) != OP_INTEGER_LITERAL) ||
       (declared(id) && declared(sub)))
        return false;

    parse_tree.setOp(pn, OP_IDENTIFIER);
    #ifndef HOPE_TYPESET_HEADLESS
    fixSubIdDocMap(pn);
    #endif

    return true;
}

template <bool allow_imp_mult>
void SymbolTableBuilder::resolveReference(ParseNode pn) alloc_except {
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
        resolveReference(pn, lookup->second);
    }
}

void SymbolTableBuilder::resolveReference(ParseNode pn, size_t sym_id) alloc_except {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    if(sym_id >= cutoff) errors.push_back(Error(parse_tree.getSelection(pn), BAD_DEFAULT_ARG));

    Symbol& sym = symbol_table.symbols[sym_id];
    parse_tree.setSymId(pn, sym_id);
    sym.is_used = true;

    sym.is_closure_nested |= sym.declaration_closure_depth && (closure_depth != sym.declaration_closure_depth);

    symbol_table.usages.push_back(Usage(sym_id, pn, READ));
}

void SymbolTableBuilder::resolveIdMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except {
    Typeset::Marker m = left;
    size_t num_terms = 0;
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->numChars()) m = right;

        auto lookup = map.find(Typeset::Selection(left, m));
        if(lookup == map.end()){
            errors.push_back(Error(parse_tree.getSelection(pn), BAD_READ));
            return;
        }
        left = m;
        num_terms++;
    }

    parse_tree.prepareNary();

    left = parse_tree.getLeft(pn);
    Typeset::Text* t = left.text;

    #ifndef HOPE_TYPESET_HEADLESS
    size_t index = t->parseNodeTagIndex(left.index);
    t->insertParseNodes(index, num_terms-1);
    #endif

    m = left;
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->numChars()) m = right;

        Typeset::Selection sel(left, m);
        auto lookup = map.find(sel);
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        #ifndef HOPE_TYPESET_HEADLESS
        if(left.text != m.text){
            t->patchParseNode(index++, pn, left.index, t->numChars());
            sel.mapConstructToParseNode(pn);
            left.text->nextConstructAsserted()->frontTextAsserted()->retagParseNode(pn, 0);
        }else{
            t->patchParseNode(index++, pn, left.index, m.index);
        }
        #endif
        resolveReference(pn, lookup->second);
        parse_tree.addNaryChild(pn);
        left = m;
    }
    ParseNode mult = parse_tree.finishNary(OP_IMPLICIT_MULTIPLY);

    parse_tree.setFlag(pn, mult);
    parse_tree.setOp(pn, OP_SINGLE_CHAR_MULT_PROXY);
}

void SymbolTableBuilder::resolveScriptMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except {
    assert(left.text != right.text);

    Typeset::Marker m = left;
    Typeset::Marker end(m.text, m.text->numChars());
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
    parse_tree.prepareNary();
    parse_tree.addNaryChild(new_id);
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++)
        parse_tree.addNaryChild(parse_tree.arg(pn, i));
    ParseNode script = parse_tree.finishNary(parse_tree.getFlag(pn), Typeset::Selection(end, right));
    resolveExpr(script);

    parse_tree.prepareNary();
    left = parse_tree.getLeft(pn);
    Typeset::Text* t = left.text;
    #ifndef HOPE_TYPESET_HEADLESS
    t->popParseNode();
    #endif
    m = left;
    while(m != end){
        m.incrementGrapheme();
        Typeset::Selection sel(left, m);
        auto lookup = map.find(sel);
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        #ifndef HOPE_TYPESET_HEADLESS
        t->tagParseNode(pn, left.index, m.index);
        #endif
        resolveReference(pn, lookup->second);
        parse_tree.addNaryChild(pn);
        left = m;
    }
    parse_tree.addNaryChild(script);
    #ifndef HOPE_TYPESET_HEADLESS
    if(parse_tree.getOp(script) == OP_IDENTIFIER){
        t->tagParseNode(script, left.index, t->numChars());
        fixSubIdDocMap<false>(script);
    }else{
        t->tagParseNode(new_id, left.index, t->numChars());
    }
    #endif
    ParseNode mult = parse_tree.finishNary(OP_IMPLICIT_MULTIPLY);

    parse_tree.setFlag(pn, mult);
    parse_tree.setOp(pn, OP_SINGLE_CHAR_MULT_PROXY);
}

void SymbolTableBuilder::resolveConditional1(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except {
    resolveExpr( parse_tree.arg<0>(pn) );
    resolveBody( SCOPE_NAME(name)  parse_tree.arg<1>(pn) );
}

void SymbolTableBuilder::resolveConditional2(ParseNode pn) alloc_except {
    resolveExpr( parse_tree.arg<0>(pn) );
    resolveBody( SCOPE_NAME("-if-")  parse_tree.arg<1>(pn) );
    resolveBody( SCOPE_NAME("-else-")  parse_tree.arg<2>(pn) );
}

void SymbolTableBuilder::resolveFor(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-for-")  parse_tree.getLeft(parse_tree.arg<1>(pn)));
    resolveStmt(parse_tree.arg<0>(pn));
    resolveExpr(parse_tree.arg<1>(pn));
    resolveStmt(parse_tree.arg<2>(pn));
    resolveStmt(parse_tree.arg<3>(pn));
    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolTableBuilder::resolveBody(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME(name)  parse_tree.getLeft(pn));
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

void SymbolTableBuilder::resolveBlock(ParseNode pn) alloc_except {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveStmt( parse_tree.arg(pn, i) );
}

void SymbolTableBuilder::resolveDefault(ParseNode pn) alloc_except {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveExpr(parse_tree.arg(pn, i));
}

void SymbolTableBuilder::resolveLambda(ParseNode pn) alloc_except {
    increaseClosureDepth(SCOPE_NAME("-lambda-")  parse_tree.getLeft(pn), pn);

    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++)
        defineLocalScope( parse_tree.arg(params, i) );

    ParseNode rhs = parse_tree.arg<3>(pn);
    resolveExpr(rhs);

    decreaseClosureDepth(parse_tree.getRight(pn));
}

void SymbolTableBuilder::resolveAlgorithm(ParseNode pn) alloc_except {
    ParseNode name = parse_tree.algName(pn);
    ParseNode val_cap = parse_tree.valCapList(pn);
    ParseNode params = parse_tree.paramList(pn);
    ParseNode body = parse_tree.body(pn);

    const Typeset::Selection sel = parse_tree.getSelection(name);
    auto lookup = map.find(sel);
    if(lookup == map.end()){
        makeEntry(sel, name, true);
    }else{
        size_t index = lookup->second;
        Symbol& sym = symbol_table.symbols[index];
        if(sym.declaration_lexical_depth == lexical_depth){
            if(sym.is_prototype){
                resolveReference(name, index);
                parse_tree.setOp(pn, OP_DEFINE_PROTO);
            }else{
                errors.push_back(Error(sel, TYPE_ERROR));
            }
        }else{
            appendEntry(name, lookup->second, true);
        }

        sym.is_prototype = false;
    }

    size_t val_cap_size = parse_tree.valListSize(val_cap);
    if(val_cap != NONE){
        parse_tree.setFlag(val_cap, symbol_table.scopes.size());
        for(size_t i = 0; i < val_cap_size; i++){
            ParseNode capture = parse_tree.arg(val_cap, i);
            resolveReference(capture);
        }
    }

    increaseClosureDepth(SCOPE_NAME(sel.str())  parse_tree.getLeft(body), pn);

    for(size_t i = 0; i < val_cap_size; i++){
        ParseNode capture = parse_tree.arg(val_cap, i);
        defineLocalScope(capture, false, false);
        symbol_table.symbols.back().is_captured_by_value = true;
        symbol_table.symbols.back().is_closure_nested = true;
        symbol_table.symbols.back().comment = NONE; //EVENTUALLY: maybe take the comment of the referenced var
    }

    bool expect_default = false;
    cutoff = symbol_table.symbols.size();
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
    cutoff = std::numeric_limits<size_t>::max();

    resolveStmt(body);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        size_t sym_id = parse_tree.getSymId(param);
        if(sym_id != NONE){
            Symbol& sym = symbol_table.symbols[sym_id];
            sym.is_const = !sym.is_reassigned;
        }
    }

    decreaseClosureDepth(parse_tree.getRight(body));
}

void SymbolTableBuilder::resolvePrototype(ParseNode pn) alloc_except {
    if(defineLocalScope(parse_tree.child(pn)))
        lastDefinedSymbol().is_prototype = true;
}

void SymbolTableBuilder::resolveSubscript(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) != 2){ resolveDefault(pn); return; }

    ParseNode id = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    bool lhs_type_eligible = (parse_tree.getOp(id) == OP_IDENTIFIER);
    bool rhs_type_eligible = (parse_tree.getOp(rhs) == OP_IDENTIFIER || parse_tree.getOp(rhs) == OP_INTEGER_LITERAL);
    #define UNDECLARED_ELEMS (!declared(id) || (parse_tree.getOp(rhs) == OP_IDENTIFIER && !declared(rhs)))

    if(lhs_type_eligible && rhs_type_eligible && UNDECLARED_ELEMS){
        parse_tree.setFlag(pn, OP_SUBSCRIPT_ACCESS); //EVENTUALLY: this is held together with duct tape and prayers
        parse_tree.setOp(pn, OP_IDENTIFIER);
        #ifndef HOPE_TYPESET_HEADLESS
        fixSubIdDocMap(pn);
        #endif
        resolveReference<true>(pn);
    }else{
        resolveDefault(pn);
    }
}

void SymbolTableBuilder::resolveBig(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-big symbol-")  parse_tree.getLeft(pn));
    ParseNode assign = parse_tree.arg<0>(pn);
    if( parse_tree.getOp(assign) != OP_ASSIGN ) return;
    ParseNode id = parse_tree.lhs(assign);
    ParseNode stop = parse_tree.arg<1>(pn);
    ParseNode body = parse_tree.arg<2>(pn);

    defineLocalScope(id, false);
    lastDefinedSymbol().is_used = true;
    resolveExpr( parse_tree.rhs(assign) );
    resolveExpr( stop );
    resolveExpr( body );

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolTableBuilder::resolveDerivative(ParseNode pn) alloc_except {
    ParseNode id = parse_tree.arg<1>(pn);
    size_t id_index = symIndex(id);
    if(id_index != NONE) resolveReference(parse_tree.arg<2>(pn));
    else parse_tree.setArg<2>(pn, NONE);

    increaseLexicalDepth(SCOPE_NAME("-derivative-")  parse_tree.getLeft(pn));

    defineLocalScope(id, true);
    if(id_index != NONE) warnings.pop_back(); //EVENTUALLY: stupid hack to not warn shadowing
    ParseNode expr = parse_tree.arg<0>(pn);
    resolveExpr(expr);

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

bool SymbolTableBuilder::defineLocalScope(ParseNode pn, bool immutable, bool warn_on_shadow) alloc_except {
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
            appendEntry(pn, lookup->second, immutable, warn_on_shadow);
        }
    }

    return true;
}

bool SymbolTableBuilder::declared(ParseNode pn) const noexcept{
    return map.find(parse_tree.getSelection(pn)) != map.end();
}

size_t SymbolTableBuilder::symIndex(ParseNode pn) const noexcept{
    const auto& lookup = map.find(parse_tree.getSelection(pn));
    return lookup == map.end() ? NONE : lookup->second;
}

#ifndef HOPE_TYPESET_HEADLESS
template<bool first>
void SymbolTableBuilder::fixSubIdDocMap(ParseNode pn) const alloc_except {
    //Update doc map
    parse_tree.getSelection(pn).mapConstructToParseNode(pn);

    parse_tree.getLeft(parse_tree.lhs(pn)).text->retagParseNodeLast(pn);

    ParseNode sub = parse_tree.rhs(pn);
    Typeset::Text* t = parse_tree.getLeft(sub).text;
    if(first && parse_tree.getOp(sub) == OP_INTEGER_LITERAL)
        t->tagParseNode(pn, 0, t->numChars());
    else
        t->retagParseNode(pn, 0);
}
#endif

void SymbolTableBuilder::increaseLexicalDepth(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin) alloc_except {
    lexical_depth++;
    addScope(SCOPE_NAME(name)  begin);
}

void SymbolTableBuilder::decreaseLexicalDepth(const Typeset::Marker& end) alloc_except {
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

void SymbolTableBuilder::increaseClosureDepth(
        #ifdef HOPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode pn) alloc_except {
    ref_frames.push_back(refs.size());

    closure_depth++;
    lexical_depth++;
    addScope(SCOPE_NAME(name)  begin, pn);
}

void SymbolTableBuilder::decreaseClosureDepth(const Typeset::Marker& end) alloc_except {
    ParseNode fn = symbol_table.scopes.back().fn;

    decreaseLexicalDepth(end);
    closure_depth--;

    //Find variables which are captured by reference in this closure, also modified by inner closures
    for(size_t seg_index = symbol_table.scopes.size()-2; seg_index != NONE; seg_index = symbol_table.scopes[seg_index].prev){
        ScopeSegment& closed_seg = symbol_table.scopes[seg_index];
        for(size_t i = closed_seg.usage_begin; i < closed_seg.usage_end; i++){
            const Usage& usage = symbol_table.usages[i];
            Symbol& sym = symbol_table.symbols[usage.var_id];
            bool is_closed = (usage.type != UsageType::DECLARE) && sym.is_closure_nested &&
                    (!sym.is_captured_by_value || sym.declaration_closure_depth <= closure_depth);
            if(!is_closed) continue;

            //The variable is closed, so we will add it to our book keeping
            if(sym.type != NONE) refs[sym.type] = NONE; //We have a more recent entry; mark the old one with a tombstone
            sym.type = refs.size();
            refs.push_back(usage.var_id);
        }
    }

    size_t cutoff = ref_frames.back();
    ref_frames.pop_back();
    parse_tree.prepareNary();
    for(size_t i = cutoff; i < refs.size(); i++){
        size_t sym_id = refs[i];
        if(sym_id == NONE) continue; //Tombstone: this node was promoted and we'll see it later
        Symbol& sym = symbol_table.symbols[sym_id];
        Op op = sym.declaration_closure_depth <= closure_depth ? OP_READ_UPVALUE : OP_IDENTIFIER;
        Typeset::Selection sel = symbol_table.getSel(sym_id);
        assert(sel.left != sel.right);
        ParseNode n = parse_tree.addTerminal(op, sel);
        assert(parse_tree.getSelection(n) == sel);
        parse_tree.setSymId(n, sym_id);
        parse_tree.addNaryChild(n);

        if(sym.declaration_closure_depth <= (closure_depth - sym.is_captured_by_value)){
            refs[cutoff] = sym_id;
            sym.type = cutoff++;
        }
    }
    Typeset::Selection sel = parse_tree.getSelection(fn);
    ParseNode list = parse_tree.finishNary(OP_LIST, sel);
    parse_tree.setRefList(fn, list);
    refs.resize(cutoff);
}

void SymbolTableBuilder::makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable) alloc_except {
    assert(map.find(c) == map.end());
    map[c] = symbol_table.symbols.size();
    symbol_table.usages.push_back(Usage(symbol_table.symbols.size(), pn, DECLARE));
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, NONE, immutable);
}

void SymbolTableBuilder::appendEntry(ParseNode pn, size_t& old_entry, bool immutable, bool warn_on_shadow) alloc_except {
    //EVENTUALLY: let the user control warnings, decide defaults
    if(warn_on_shadow) warnings.push_back(Error(parse_tree.getSelection(pn), SHADOWING_VAR));
    symbol_table.usages.push_back(Usage(symbol_table.symbols.size(), pn, DECLARE));
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, old_entry, immutable);
    old_entry = symbol_table.symbols.size()-1;
}

}

}
