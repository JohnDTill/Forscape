#include "forscape_symbol_lexical_pass.h"

#include <code_parsenode_ops.h>
#include <forscape_common.h>
#include <forscape_dynamic_settings.h>
#include <forscape_program.h>
#include "typeset_model.h"
#include "typeset_construct.h"
#include <algorithm>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

FORSCAPE_STATIC_MAP<std::string_view, Op> SymbolLexicalPass::predef {
    {"π", OP_PI},
    {"e", OP_EULERS_NUMBER},
    {"φ", OP_GOLDEN_RATIO},
    {"c", OP_SPEED_OF_LIGHT},
    {"ℎ", OP_PLANCK_CONSTANT},
    {"ℏ", OP_REDUCED_PLANCK_CONSTANT},
    {"σ", OP_STEFAN_BOLTZMANN_CONSTANT},
    {"I", OP_IDENTITY_AUTOSIZE},
    //{"i", OP_IMAGINARY}, //EVENTUALLY - support complex numbers
    {"g", OP_GRAVITY},
    {"Γ", OP_GAMMA_FUNCTION},

    //EVENTUALLY: how do units work? conversions?
    //{"m", OP_METERS},
    //{"s", OP_SECONDS},
    //{"kg", OP_KILOGRAMS},
    //{"N", OP_NEWTONS},
    //{"Pa", OP_PASCALS},
    //{"Hz", OP_HERTZ},
    //{"l", OP_LITERS},
    //{"rad", OP_RADIANS},
    //{"deg", OP_DEGREES},

    //{"ft", OP_FEET},
    //{"in", OP_INCHES},
    //{"lb", OP_POUND_MASS},
    //{"lbf", OP_POUND_FORCE},

    {"T", OP_MAYBE_TRANSPOSE},
};

SymbolLexicalPass::SymbolLexicalPass(ParseTree& parse_tree, Typeset::Model* model) noexcept
    : error_stream(Program::instance()->error_stream),
      parse_tree(parse_tree),
      model(model),
      symbol_table(parse_tree),
      symbols(symbol_table.symbols),
      lexical_map(symbol_table.lexical_map),
      scope_segments(symbol_table.scope_segments),
      symbol_usages(symbol_table.symbol_usages) {}

void SymbolLexicalPass::resolveSymbols() alloc_except {
    reset();
    // Note: symbol resolution runs DESPITE parse errors, because the editor needs it
    if(!parse_tree.empty()) resolveBlock(parse_tree.root);

    symbol_table.finalize();
    for(ParseNode pn : processed_refs) parse_tree.setSymbol(pn, symbols.data() + parse_tree.getSymId(pn));
}

void SymbolLexicalPass::reset() noexcept {
    symbol_table.reset(parse_tree.getLeft(parse_tree.root));
    //assert(map.empty()); //Not necessarily empty due to errors
    lexical_map.clear();
    assert(refs.empty());
    processed_refs.clear();
    assert(ref_frames.empty());
    lexical_depth = GLOBAL_DEPTH;
    closure_depth = 0;
    active_scope_id = 0;
}

ScopeSegment& SymbolLexicalPass::activeScope() noexcept {
    return scope_segments[active_scope_id];
}

void SymbolLexicalPass::addScope(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode closure) alloc_except {
    symbol_table.addScope(SCOPE_NAME(name) begin);

    size_t sze = symbols.size();
    size_t n_usages = symbol_usages.size();
    scope_segments[active_scope_id].usage_end = n_usages;
    active_scope_id++;
    ScopeSegment& added_segment = scope_segments.back();
    added_segment.first_sym_index = sze;
    added_segment.usage_begin = n_usages;
    added_segment.fn = closure;
}

void SymbolLexicalPass::closeScope(const Typeset::Marker& end) noexcept {
    symbol_table.closeScope(end);

    size_t sze = symbols.size();
    size_t n_usages = symbol_usages.size();
    scope_segments[active_scope_id].usage_end = n_usages;
    active_scope_id++;
    assert(active_scope_id < scope_segments.size());
    ScopeSegment& added_segment = scope_segments.back();
    added_segment.first_sym_index = sze;
    added_segment.usage_begin = n_usages;
}

Symbol& SymbolLexicalPass::lastDefinedSymbol() noexcept {
    return symbols.back();
}

size_t SymbolLexicalPass::symbolIndexFromSelection(const Typeset::Selection& sel) const noexcept {
    auto lookup = lexical_map.find(sel);
    return lookup == lexical_map.end() ? NONE : lookup->second;
}

void SymbolLexicalPass::resolveStmt(ParseNode pn) alloc_except {
    switch (parse_tree.getOp(pn)) {
        case OP_ALGORITHM: resolveAlgorithm(pn); break;
        case OP_ASSIGN: resolveAssignment(pn); break;
        case OP_BLOCK: resolveBlock(pn); break;
        case OP_CLASS: resolveClass(pn); break;
        case OP_EQUAL: resolveEquality(pn); break;
        case OP_FOR: resolveFor(pn); break;
        case OP_FROM_IMPORT: resolveFromImport(pn); break;
        case OP_IF: resolveConditional1(SCOPE_NAME("-if-")  pn); break;
        case OP_IF_ELSE: resolveConditional2(pn); break;
        case OP_IMPORT: resolveImport(pn); break;
        case OP_LEXICAL_SCOPE: resolveLexicalScope(pn); break;
        case OP_NAMESPACE: resolveNamespace(pn); break;
        case OP_RANGED_FOR: resolveRangedFor(pn); break;
        case OP_RETURN:
        case OP_RETURN_EMPTY:
            if(closure_depth == 0) error(pn, RETURN_OUTSIDE_FUNCTION);
            resolveDefault(pn);
            break;
        case OP_SETTINGS_UPDATE: resolveSettingsUpdate(pn); break;
        case OP_SWITCH: resolveSwitch(pn); break;
        case OP_UNKNOWN_LIST: resolveUnknownDeclaration(pn); break;
        case OP_WHILE: resolveConditional1(SCOPE_NAME("-while-")  pn); break;

        default: resolveDefault(pn);
    }
}

void SymbolLexicalPass::resolveExpr(ParseNode pn) alloc_except {
    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER: resolveReference<true>(pn); break;
        case OP_LAMBDA: resolveLambda(pn); break;
        case OP_SUBSCRIPT_ACCESS: resolveSubscript(pn); break;
        case OP_LIMIT: resolveLimit(pn); break;
        case OP_INTEGRAL: resolveIndefiniteIntegral(pn); break;
        case OP_DEFINITE_INTEGRAL: resolveDefiniteIntegral(pn); break;

        case OP_SUMMATION:
        case OP_PRODUCT:
            resolveBig(pn);
            break;

        case OP_DERIVATIVE:
        case OP_PARTIAL:
            resolveDerivative(pn);
            break;

        case OP_SCOPE_ACCESS:
            resolveScopeAccess(pn);
            break;

        default: resolveDefault(pn);
    }
}

void SymbolLexicalPass::resolveEquality(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) != 2){
        error(pn, ErrorCode::TYPE_ERROR);
    }else{
        resolveExpr(parse_tree.rhs(pn));
        defineLocalScope(parse_tree.lhs(pn));
    }
}

void SymbolLexicalPass::resolveAssignment(ParseNode pn) alloc_except {
    assert(parse_tree.getNumArgs(pn) == 2);

    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    switch(parse_tree.getOp( lhs )){
        case OP_SCOPE_ACCESS:
            resolveScopeAccess(lhs);
            parse_tree.setOp(pn, OP_REASSIGN);
            break;

        case OP_IDENTIFIER:
            resolveAssignmentId(pn);
            resolveExpr(rhs);
            break;

        case OP_SUBSCRIPT_ACCESS:
            if(lexical_map.find(parse_tree.getSelection(lhs)) != lexical_map.end()){
                resolvePotentialIdSub(lhs);
                resolveAssignmentId(pn);
                resolveExpr(rhs);
            }else{
                parse_tree.setOp(pn, OP_REASSIGN);
                resolveAssignmentSubscript(pn, lhs, rhs);
            }
            break;

        default:
            error(lhs, ASSIGN_EXPRESSION);
    }
}

void SymbolLexicalPass::resolveAssignmentId(ParseNode pn) alloc_except {
    ParseNode id = parse_tree.lhs(pn);
    Typeset::Selection c = parse_tree.getSelection(id);
    if(parse_tree.getOp(id) != OP_IDENTIFIER){
        error(c, NON_LVALUE);
        return;
    }

    auto result = lexical_map.insert({c, symbols.size()});
    if(result.second){
        symbol_table.addSymbol(id, lexical_depth, closure_depth, NONE, false);
    }else{
        SymbolIndex index = result.first->second;
        Symbol& sym = symbols[index];
        if(sym.is_const){
            error(c, REASSIGN_CONSTANT);
        }else{
            sym.is_reassigned = true;
            parse_tree.setOp(pn, OP_REASSIGN);
            resolveReference(id, index);
        }
    }
}

void SymbolLexicalPass::resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs) alloc_except {
    ParseNode id = parse_tree.arg<0>(lhs);
    Typeset::Selection c = parse_tree.getSelection(id);

    if(parse_tree.getOp(id) == OP_SCOPE_ACCESS){
        resolveScopeAccess(id);
    }else if(parse_tree.getOp(id) != OP_IDENTIFIER){
        error(c, NON_LVALUE);
        return;
    }else{
        size_t symbol_index = symbolIndexFromSelection(c);
        if(symbol_index != NONE){
            symbols[symbol_index].is_reassigned = true;
            resolveReference(id, symbol_index);
        }else{
            error(c, BAD_READ);
        }
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
            error(parse_tree.arg<3>(lhs), INDEX_OUT_OF_RANGE);
            potential_loop_vars.clear();
            return;
        }

        parse_tree.setOp(pn, OP_ELEMENTWISE_ASSIGNMENT);
        increaseLexicalDepth(SCOPE_NAME("-ewise assign-")  parse_tree.getLeft(pn));
        size_t vars_start = symbols.size();
        for(ParseNode pn : potential_loop_vars){
            bool success = defineLocalScope(pn, false);
            (void)success;
            assert(success);
        }
        resolveExpr(rhs);

        for(size_t i = 0; i < potential_loop_vars.size(); i++){
            Symbol& sym = symbols[vars_start + i];
            ParseNode var = potential_loop_vars[potential_loop_vars.size()-1];
            sym.is_ewise_index = true;
            if(!sym.is_used){
                error(var, UNUSED_ELEM_AGN_INDEX);
                sym.is_used = true;
            }
        }

        decreaseLexicalDepth(parse_tree.getRight(pn));
        potential_loop_vars.clear();
    }else if(!potential_loop_vars.empty()){
        for(ParseNode pn : potential_loop_vars)
            error(pn, BAD_READ_OR_SUBSCRIPT);
        potential_loop_vars.clear();
    }else{
        resolveExpr(rhs);
    }
}

bool SymbolLexicalPass::resolvePotentialIdSub(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) != 2) return false;

    ParseNode id = parse_tree.lhs(pn);
    ParseNode sub = parse_tree.rhs(pn);

    if(parse_tree.getOp(id) != OP_IDENTIFIER ||
       (parse_tree.getOp(sub) != OP_IDENTIFIER && parse_tree.getOp(sub) != OP_INTEGER_LITERAL) ||
       (declared(id) && declared(sub)))
        return false;

    parse_tree.setOp(pn, OP_IDENTIFIER);
    #ifndef FORSCAPE_TYPESET_HEADLESS
    fixSubIdDocMap(pn);
    #endif

    return true;
}

template <bool allow_imp_mult>
void SymbolLexicalPass::resolveReference(ParseNode pn) alloc_except {
    Typeset::Selection c = parse_tree.getSelection(pn);

    auto lookup = lexical_map.find(c);
    if(lookup == lexical_map.end()){
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
            error(c, BAD_READ);
            parse_tree.setOp(pn, OP_ERROR);
        }
    }else{
        resolveReference(pn, lookup->second);
    }
}

void SymbolLexicalPass::resolveReference(ParseNode pn, size_t sym_id) alloc_except {
    if(sym_id >= cutoff) error(pn, BAD_DEFAULT_ARG);
    symbol_table.resolveReference(pn, sym_id, closure_depth);
}

static std::vector<FORSCAPE_UNORDERED_MAP<Typeset::Selection, SymbolIndex>::iterator> implicit_mult_hits;

void SymbolLexicalPass::resolveIdMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except {
    Typeset::Marker m = left;
    size_t num_terms = 0;
    implicit_mult_hits.clear();
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->numChars()) m = right;

        Typeset::Selection sel(left, m);
        auto lookup = lexical_map.find(sel);
        if(lookup == lexical_map.end()){
            if(!sel.isTextSelection()){
                error(pn, BAD_READ);
                parse_tree.setOp(pn, OP_ERROR);
                return;
            }

            auto predef_lookup = predef.find(sel.strView());
            if(predef_lookup == predef.end()){
                error(pn, BAD_READ);
                parse_tree.setOp(pn, OP_ERROR);
                return;
            }else{
                lookup = lexical_map.end();
            }
        }
        implicit_mult_hits.push_back(lookup);
        left = m;
        num_terms++;
    }

    parse_tree.prepareNary();

    left = parse_tree.getLeft(pn);
    Typeset::Text* t = left.text;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    size_t index = t->parseNodeTagIndex(left.index);
    t->insertParseNodes(index, num_terms-1);
    #endif

    m = left;
    size_t hit_index = 0;
    while(m != right){
        m.incrementGrapheme();
        if(m.index == m.text->numChars()) m = right;

        Typeset::Selection sel(left, m);
        auto lookup = implicit_mult_hits[hit_index++];
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        #ifndef FORSCAPE_TYPESET_HEADLESS
        if(left.text != m.text){
            t->patchParseNode(index++, pn, left.index, t->numChars());
            sel.mapConstructToParseNode(pn);
            left.text->nextConstructAsserted()->frontTextAsserted()->retagParseNode(pn, 0);
        }else{
            t->patchParseNode(index++, pn, left.index, m.index);
        }
        #endif
        if(lookup != lexical_map.end()){
            resolveReference(pn, lookup->second);
        }else{
            parse_tree.setOp(pn, predef.find(sel.strView())->second);
            sel.format(SEM_PREDEF);
        }
        parse_tree.addNaryChild(pn);
        left = m;
    }
    ParseNode mult = parse_tree.finishNary(OP_IMPLICIT_MULTIPLY);

    parse_tree.setFlag(pn, mult);
    parse_tree.setOp(pn, OP_SINGLE_CHAR_MULT_PROXY);
}

void SymbolLexicalPass::resolveScriptMult(ParseNode pn, Typeset::Marker left, Typeset::Marker right) alloc_except {
    assert(left.text != right.text);

    Typeset::Marker m = left;
    Typeset::Marker end(m.text, m.text->numChars());
    Typeset::Marker new_right = end;
    end.decrementGrapheme();
    if(left == end || predef.find(Typeset::Selection(end, new_right).strView()) != predef.end()){
        error(pn, BAD_READ);
        parse_tree.setOp(pn, OP_ERROR);
        return;
    }

    implicit_mult_hits.clear();
    while(m != end){
        m.incrementGrapheme();
        Typeset::Selection sel(left, m);
        auto lookup = lexical_map.find(sel);
        if(lookup == lexical_map.end()){
            auto predef_lookup = predef.find(sel.strView());
            if(predef_lookup == predef.end()){
                error(pn, BAD_READ);
                parse_tree.setOp(pn, OP_ERROR);
                return;
            }else{
                lookup = lexical_map.end();
            }
        }
        implicit_mult_hits.push_back(lookup);
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
    #ifndef FORSCAPE_TYPESET_HEADLESS
    t->popParseNode();
    #endif
    m = left;
    size_t hit_index = 0;
    while(m != end){
        m.incrementGrapheme();
        Typeset::Selection sel(left, m);
        auto lookup = implicit_mult_hits[hit_index++];
        ParseNode pn = parse_tree.addTerminal(OP_IDENTIFIER, sel);
        #ifndef FORSCAPE_TYPESET_HEADLESS
        t->tagParseNode(pn, left.index, m.index);
        #endif
        if(lookup != lexical_map.end()){
            resolveReference(pn, lookup->second);
        }else{
            parse_tree.setOp(pn, predef.find(sel.strView())->second);
            sel.format(SEM_PREDEF);
        }
        parse_tree.addNaryChild(pn);
        left = m;
    }
    parse_tree.addNaryChild(script);
    #ifndef FORSCAPE_TYPESET_HEADLESS
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

void SymbolLexicalPass::resolveConditional1(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except {
    resolveExpr( parse_tree.arg<0>(pn) );
    resolveBody( SCOPE_NAME(name)  parse_tree.arg<1>(pn) );
}

void SymbolLexicalPass::resolveConditional2(ParseNode pn) alloc_except {
    resolveExpr( parse_tree.arg<0>(pn) );
    resolveBody( SCOPE_NAME("-if-")  parse_tree.arg<1>(pn) );
    resolveBody( SCOPE_NAME("-else-")  parse_tree.arg<2>(pn) );
}

void SymbolLexicalPass::resolveFor(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-for-")  parse_tree.getLeft(parse_tree.arg<1>(pn)));
    resolveStmt(parse_tree.arg<0>(pn));
    resolveExpr(parse_tree.arg<1>(pn));
    resolveStmt(parse_tree.arg<2>(pn));
    resolveStmt(parse_tree.arg<3>(pn));
    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveRangedFor(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-foreach-")  parse_tree.getLeft(parse_tree.arg<1>(pn)));
    defineLocalScope(parse_tree.arg<0>(pn));
    resolveExpr(parse_tree.arg<1>(pn));
    resolveStmt(parse_tree.arg<2>(pn));
    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveSettingsUpdate(ParseNode pn) alloc_except {
    Settings& s = settings();
    const auto settings_construct = parse_tree.getFlag(pn);
    s.updateInherited(settings_construct);
    s.enact(settings_construct);
}

void SymbolLexicalPass::resolveSwitch(ParseNode pn) alloc_except {
    resolveExpr(parse_tree.arg<0>(pn));
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++){
        ParseNode a = parse_tree.arg(pn, i);
        increaseLexicalDepth(SCOPE_NAME("case " + parse_tree.str(a))  parse_tree.getLeft(parse_tree.arg<1>(pn)));
        if(parse_tree.getOp(a) == OP_CASE) resolveExpr(parse_tree.lhs(a));
        ParseNode stmt = parse_tree.rhs(a);
        if(stmt != NONE) resolveStmt(stmt);
        decreaseLexicalDepth(parse_tree.getRight(pn));
    }
}

void SymbolLexicalPass::resolveBody(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME(name)  parse_tree.getLeft(pn));
    resolveStmt(pn);
    decreaseLexicalDepth(parse_tree.getRight(pn));

    switch (parse_tree.getOp(pn)) {
        case OP_EQUAL:
        case OP_ASSIGN:
            error(pn, UNUSED_VAR);
    }
}

void SymbolLexicalPass::resolveBlock(ParseNode pn) alloc_except {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
        ParseNode child = parse_tree.arg(pn, i);
        if(parse_tree.getOp(child) != OP_ALGORITHM) continue;
        defineLocalScope(parse_tree.algName(child));
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveStmt( parse_tree.arg(pn, i) );

    //Re-order functions
    static std::vector<ParseNode> algs;
    for(size_t i = parse_tree.getNumArgs(pn); i-->0;){
        ParseNode child = parse_tree.arg(pn, i);

        //Non-capturing algorithms are moved to the top
        if(parse_tree.getOp(child) == OP_ALGORITHM
           && parse_tree.valCapList(child) == NONE
           && parse_tree.getNumArgs(parse_tree.refCapList(child)) == 0){
            algs.push_back(child);
        }else{
            parse_tree.setArg(pn, i+algs.size(), child);
        }
    }
    for(size_t i = algs.size(); i-->0;)
        parse_tree.setArg(pn, algs.size()-i-1, algs[i]);
    algs.clear();
}

void SymbolLexicalPass::resolveLexicalScope(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("Anonymous")  parse_tree.getLeft(pn));
    resolveBlock(pn);
    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveDefault(ParseNode pn) alloc_except {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        resolveExpr(parse_tree.arg(pn, i));
}

void SymbolLexicalPass::resolveLambda(ParseNode pn) alloc_except {
    increaseClosureDepth(SCOPE_NAME("-lambda-")  parse_tree.getLeft(pn), pn);

    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++)
        defineLocalScope( parse_tree.arg(params, i) );

    ParseNode rhs = parse_tree.arg<3>(pn);
    resolveExpr(rhs);

    decreaseClosureDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveAlgorithm(ParseNode pn) alloc_except {
    ParseNode name = parse_tree.algName(pn);
    ParseNode val_cap = parse_tree.valCapList(pn);
    ParseNode params = parse_tree.paramList(pn);
    ParseNode body = parse_tree.body(pn);

    const Typeset::Selection sel = parse_tree.getSelection(name);
    auto lookup = lexical_map.find(sel);
    assert(lookup != lexical_map.end());
    size_t index = lookup->second;
    Symbol& sym = symbols[index];
    assert(sym.declaration_lexical_depth == lexical_depth);

    size_t val_cap_size = parse_tree.valListSize(val_cap);
    if(val_cap != NONE){
        parse_tree.setFlag(val_cap, scope_segments.size());
        for(size_t i = 0; i < val_cap_size; i++){
            ParseNode capture = parse_tree.arg(val_cap, i);
            resolveReference(capture);
        }
    }

    increaseClosureDepth(SCOPE_NAME(sel.str())  parse_tree.getLeft(body), pn);

    for(size_t i = 0; i < val_cap_size; i++){
        ParseNode capture = parse_tree.arg(val_cap, i);
        defineLocalScope(capture, false, false);
        symbols.back().is_captured_by_value = true;
        symbols.back().is_closure_nested = true;
        symbols.back().comment = NONE; //EVENTUALLY: maybe take the comment of the referenced var
    }

    bool expect_default = false;
    cutoff = symbols.size();
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL){
            resolveExpr(parse_tree.rhs(param));
            param = parse_tree.lhs(param);
            expect_default = true;
        }else if(expect_default){
            error(param, EXPECT_DEFAULT_ARG);
        }

        bool success = defineLocalScope( param, false );
        if(!success) error(param, REDEFINE_PARAMETER);
    }
    cutoff = std::numeric_limits<size_t>::max();

    resolveStmt(body);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        if(parse_tree.getOp(param) == OP_ERROR) continue;
        size_t sym_id = parse_tree.getSymId(param);
        if(sym_id != NONE){
            Symbol& sym = symbols[sym_id];
            sym.is_const = !sym.is_reassigned;
        }
    }

    decreaseClosureDepth(parse_tree.getRight(body));
}

void SymbolLexicalPass::resolveClass(ParseNode pn) alloc_except {
    ParseNode name = parse_tree.arg<0>(pn);
    ParseNode parents = parse_tree.arg<1>(pn);
    if(parents != NONE){
        for(size_t i = 0; i < parse_tree.getNumArgs(parents); i++){
            ParseNode parent = parse_tree.arg(parents, i);
            resolveReference(parent);
        }
    }
    ParseNode members = parse_tree.arg<2>(pn);

    const Typeset::Selection sel = parse_tree.getSelection(name);
    auto lookup = lexical_map.find(sel);
    if(lookup == lexical_map.end()){
        makeEntry(sel, name, true);
    }else{
        size_t index = lookup->second;
        Symbol& sym = symbols[index];
        if(sym.declaration_lexical_depth == lexical_depth){
            error(sel, TYPE_ERROR);
        }else{
            appendEntry(name, lookup->second, true);
        }
    }

    increaseLexicalDepth(SCOPE_NAME(sel.str())  parse_tree.getLeft(members));

    //EVENTUALLY: do symbol resolution with classes and allow instances

    decreaseLexicalDepth(parse_tree.getRight(members));
}

void SymbolLexicalPass::resolveSubscript(ParseNode pn) alloc_except {
    if(parse_tree.getNumArgs(pn) != 2){ resolveDefault(pn); return; }

    ParseNode id = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    bool lhs_type_eligible = (parse_tree.getOp(id) == OP_IDENTIFIER);
    bool rhs_type_eligible = (parse_tree.getOp(rhs) == OP_IDENTIFIER || parse_tree.getOp(rhs) == OP_INTEGER_LITERAL);
    #define UNDECLARED_ELEMS (!declared(id) || (parse_tree.getOp(rhs) == OP_IDENTIFIER && !declared(rhs)))

    if(lhs_type_eligible && rhs_type_eligible && UNDECLARED_ELEMS){
        parse_tree.setFlag(pn, OP_SUBSCRIPT_ACCESS); //EVENTUALLY: this is held together with duct tape and prayers
        parse_tree.setOp(pn, OP_IDENTIFIER);
        #ifndef FORSCAPE_TYPESET_HEADLESS
        fixSubIdDocMap(pn);
        #endif
        resolveReference<true>(pn);
    }else{
        resolveDefault(pn);
    }
}

void SymbolLexicalPass::resolveBig(ParseNode pn) alloc_except {
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

void SymbolLexicalPass::resolveDerivative(ParseNode pn) alloc_except {
    ParseNode id = parse_tree.arg<1>(pn);
    size_t id_index = symIndex(id);
    if(id_index != NONE) resolveReference(parse_tree.arg<2>(pn));
    else parse_tree.setArg<2>(pn, NONE);

    increaseLexicalDepth(SCOPE_NAME("-derivative-")  parse_tree.getLeft(pn));

    defineLocalScope(id, true, false);
    ParseNode expr = parse_tree.arg<0>(pn);
    resolveExpr(expr);

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveLimit(ParseNode pn) alloc_except {
    resolveExpr(parse_tree.arg<1>(pn));
    increaseLexicalDepth(SCOPE_NAME("-limit-")  parse_tree.getLeft(pn));

    defineLocalScope(parse_tree.arg<0>(pn), true, false);
    resolveExpr(parse_tree.arg<2>(pn));

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveIndefiniteIntegral(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-indef_int-")  parse_tree.getLeft(pn));

    ParseNode id = parse_tree.arg<0>(pn);
    if(!declared(id)) defineLocalScope(id, true, false);
    resolveExpr(parse_tree.arg<1>(pn));

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveDefiniteIntegral(ParseNode pn) alloc_except {
    resolveExpr(parse_tree.arg<1>(pn));
    resolveExpr(parse_tree.arg<2>(pn));
    increaseLexicalDepth(SCOPE_NAME("-def_int-")  parse_tree.getLeft(pn));

    defineLocalScope(parse_tree.arg<0>(pn), true, false);
    resolveExpr(parse_tree.arg<3>(pn));

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveSetBuilder(ParseNode pn) alloc_except {
    increaseLexicalDepth(SCOPE_NAME("-set_builder-")  parse_tree.getLeft(pn));

    ParseNode var = parse_tree.arg<0>(pn);
    if(parse_tree.getOp(var) == OP_IN) var = parse_tree.lhs(var);

    if(parse_tree.getOp(var) != OP_IDENTIFIER){
        error(var, ErrorCode::NON_LVALUE);
    }else{
        defineLocalScope(parse_tree.arg<0>(pn), true, false);
        resolveExpr(parse_tree.arg<1>(pn));
    }

    decreaseLexicalDepth(parse_tree.getRight(pn));
}

void SymbolLexicalPass::resolveUnknownDeclaration(ParseNode pn) alloc_except {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++)
        defineLocalScope(parse_tree.arg(pn, i));
}

void SymbolLexicalPass::resolveImport(ParseNode pn) alloc_except {
    ParseNode alias = parse_tree.getFlag(pn);
    if(alias == NONE){
        ParseNode file = parse_tree.child(pn);
        if(parse_tree.getOp(file) != OP_FILE_REF) return;
        Typeset::Selection id_sel = parse_tree.getSelection(file);
        std::string_view file_path = id_sel.strView();

        //Constrict the file path to the identifier, e.g. ../foo/bar.π => bar
        bool found_extension = false;
        for(size_t i = file_path.size(); i-->0;){
            switch (file_path[i]) {
                case '.':
                    if(!found_extension){
                        id_sel.right.index = id_sel.left.index + i;
                        found_extension = true;
                    }
                    break;

                case '/':
                case '\\':
                    id_sel.left.index += i+1;
                    i = 0;
                    break;
            }
        }

        ParseNode id = parse_tree.addTerminal(OP_IDENTIFIER, id_sel);
        defineLocalScope(id);
        symbols.back().tied_to_file = true;

        parse_tree.setFlag(pn, id);
        parse_tree.setCols(file, id);
    }else{
        defineLocalScope(alias);
    }
}

void SymbolLexicalPass::resolveFromImport(ParseNode pn) alloc_except {
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2){
        ParseNode child = parse_tree.arg(pn, i);
        ParseNode alias = parse_tree.arg(pn, i+1);
        if(alias == NONE){
            defineLocalScope(child);
        }else{
            defineLocalScope(alias);
            parse_tree.setFlag(child, symbol_usages.size());
            symbol_usages.push_back(SymbolUsage(NONE, NONE, child, parse_tree.getSelection(child)));
        }
    }
}

void SymbolLexicalPass::resolveNamespace(ParseNode pn) alloc_except {
    ParseNode name = parse_tree.arg<0>(pn);
    ParseNode body = parse_tree.arg<1>(pn);

    if(!error_stream.noErrors()) return; //EVENTUALLY: more resiliency here

    const Typeset::Selection& sel = parse_tree.getSelection(name);
    const auto lookup = lexical_map.find(sel);
    if(lookup != lexical_map.end()){
        const size_t sym_id = lookup->second;
        Symbol& candidate = symbols[sym_id];
        if(candidate.declaration_lexical_depth == lexical_depth){
            //This namespace already exists - load the previous variables
            parse_tree.setSymId(name, sym_id);
            parse_tree.setFlag(pn, 1);
            symbol_usages.push_back(SymbolUsage(candidate.last_usage_index, sym_id, name, sel));
            candidate.last_usage_index = symbol_usages.size()-1;

            loadScope(pn, sym_id);
            resolveBlock(body);
            unloadScope(body, sym_id);
            return;
        }
    }

    //First definition of this namespace
    const size_t sym_id = symbols.size();
    defineLocalScope(name);
    if(!error_stream.noErrors()) return; //EVENTUALLY: more resiliency here
    symbols[sym_id].previous_namespace_index = NONE;
    increaseLexicalDepth(SCOPE_NAME(parse_tree.str(name))  parse_tree.getLeft(body));
    resolveBlock(body);
    unloadScope(body, sym_id);
}

void SymbolLexicalPass::loadScope(ParseNode pn, size_t sym_id) alloc_except {
    const Symbol& scope = symbols[sym_id];

    increaseLexicalDepth(SCOPE_NAME(scope.sel(parse_tree).str())  parse_tree.getLeft(pn));

    //Load the variables from the previous uses of the scope
    ScopeSegmentIndex scope_index = scope.previous_namespace_index;
    while(scope_index != NONE){
        ScopeSegment& scope_segment = scope_segments[scope_index];

        //Put all the segment variables in the map
        for(size_t sym_id = scope_segment.first_sym_index; sym_id < scope_segments[scope_index+1].first_sym_index; sym_id++){
            Symbol& sym = symbols[sym_id];
            const Typeset::Selection& sel = sym.sel(parse_tree);
            auto result = lexical_map.insert({sel, sym_id});
            if(!result.second){
                sym.index_of_shadowed_var = result.first->second;
                result.first->second = sym_id;
            }
        }

        scope_index = scope_segment.prev_namespace_segment;
    }

    //Note: with this solution, re-entering a scope has O(n) cost with a relatively large constant,
    //      where n is the number of symbols in the scope. Resolving a symbol is O(1). Probably best
    //      to prioritise symbol resolution over scope entry.
    //
    //      Might need an optimisation for repeatedly exiting and re-entering the same scope, like with
    //      C++ class implementation files. Otherwise the trade-off seems good.
}

void SymbolLexicalPass::unloadScope(ParseNode body, size_t scope_sym_id) alloc_except {
    Symbol& scope = symbols[scope_sym_id];
    ScopeSegmentIndex scope_index = scope.previous_namespace_index;
    while(scope_index != NONE){
        ScopeSegment& scope_segment = scope_segments[scope_index];

        //Remove all the previously defined scope variables from the lexical map
        const auto& begin = symbols.cbegin() + scope_segment.first_sym_index;
        const auto& end = symbols.cbegin() + scope_segments[scope_index+1].first_sym_index;
        for(auto it = begin; it < end; it++){
            const Symbol& sym = *it;
            const Typeset::Selection& sel = sym.sel(parse_tree);
            assert(lexical_map.contains(sel));
            if(sym.index_of_shadowed_var == NONE) lexical_map.erase(sel);
            else lexical_map[sel] = sym.index_of_shadowed_var;
        }

        scope_index = scope_segment.prev_namespace_segment;
    }

    scope_index = scope_segments.size()-1;
    decreaseLexicalDepth(parse_tree.getRight(body));
    while(scope_index != NONE){
        ScopeSegment& scope_segment = scope_segments[scope_index];

        //Add all new scope variables to scope map
        for(size_t sym_id = scope_segment.first_sym_index; sym_id < scope_segments[scope_index+1].first_sym_index; sym_id++){
            const Symbol& sym = symbols[sym_id];
            if(sym.declaration_lexical_depth != lexical_depth+1) continue;
            auto result = symbol_table.scoped_vars.insert({SymbolTable::ScopedVarKey(scope_sym_id, sym.sel(parse_tree)), sym_id});
            assert(result.second); //Failure to add the symbol is a bug
        }

        //Update the scope linked list
        ScopeSegmentIndex prev_namespace_segment = scope_segment.prev_lexical_segment_index;
        if(prev_namespace_segment == NONE){
            scope_segments[scope_index].prev_namespace_segment = scope.previous_namespace_index;
            scope.previous_namespace_index = scope_segments.size()-2;
            break;
        }else{
            scope_segments[scope_index].prev_namespace_segment = prev_namespace_segment;
            scope_index = prev_namespace_segment;
        }
    }

    //The namespace scope runs together with the lexical scope for purposes of calculating the stack size
    //EVENTUALLY: it's a kludge that we accomodate later stack size calculations here
    scope_segments[scope.previous_namespace_index].is_end_of_scope = false;
    scope_segments[scope_index-1].is_end_of_scope = false;
}

void SymbolLexicalPass::resolveScopeAccess(ParseNode pn) alloc_except {
    //Only resolve the leftmost part here

    ParseNode lhs = parse_tree.arg<0>(pn);
    switch (parse_tree.getOp(lhs)) {
        case OP_IDENTIFIER: resolveReference(lhs); break;
        case OP_SCOPE_ACCESS: resolveScopeAccess(lhs); break;
        default: resolveDefault(lhs); break;
    }

    //Create a usage for the RHS which will be patched later
    ParseNode rhs = parse_tree.arg<1>(pn);
    parse_tree.setFlag(rhs, symbol_usages.size());
    parse_tree.setOp(rhs, OP_ERROR); //Proxy to prevent attempting to read this before it is validated
    symbol_usages.push_back(SymbolUsage(NONE, NONE, rhs, parse_tree.getSelection(rhs)));
}

bool SymbolLexicalPass::defineLocalScope(ParseNode pn, bool immutable, bool warn_on_shadow) alloc_except {
    Typeset::Selection c = parse_tree.getSelection(pn);

    if(parse_tree.getOp(pn) == OP_SUBSCRIPT_ACCESS && !resolvePotentialIdSub(pn)){
        error(c, ASSIGN_EXPRESSION);
        return false;
    }

    if(parse_tree.getOp(pn) != OP_IDENTIFIER){
        error(c, ASSIGN_EXPRESSION);
        return false;
    }

    auto result = lexical_map.insert({c, symbols.size()});
    if(result.second){
        symbol_table.addSymbol(pn, lexical_depth, closure_depth, NONE, immutable);
    }else{
        const SymbolIndex index = result.first->second;
        Symbol& sym = symbols[index];
        if(sym.declaration_lexical_depth == lexical_depth){
            bool CONST = sym.is_const;
            error(c, CONST ? REASSIGN_CONSTANT : MUTABLE_CONST_ASSIGN);
            return false;
        }else{
            appendEntry(pn, result.first->second, immutable, warn_on_shadow);
        }
    }

    return true;
}

bool SymbolLexicalPass::declared(ParseNode pn) const noexcept {
    return lexical_map.find(parse_tree.getSelection(pn)) != lexical_map.end();
}

size_t SymbolLexicalPass::symIndex(ParseNode pn) const noexcept {
    const auto& lookup = lexical_map.find(parse_tree.getSelection(pn));
    return lookup == lexical_map.end() ? NONE : lookup->second;
}

Settings& SymbolLexicalPass::settings() const noexcept {
    return Program::instance()->settings;
}

void SymbolLexicalPass::error(ParseNode pn, ErrorCode code) alloc_except {
    error(parse_tree.getSelection(pn), code);
}

void SymbolLexicalPass::error(const Typeset::Selection& selection, ErrorCode code) alloc_except {
    error_stream.fail(selection, code);
}

#ifndef FORSCAPE_TYPESET_HEADLESS
template<bool first>
void SymbolLexicalPass::fixSubIdDocMap(ParseNode pn) const alloc_except {
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

void SymbolLexicalPass::increaseLexicalDepth(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin) alloc_except {
    lexical_depth++;
    addScope(SCOPE_NAME(name)  begin);

    settings().enterScope();
}

void SymbolLexicalPass::decreaseLexicalDepth(const Typeset::Marker& end) alloc_except {
    closeScope(end);

    for(size_t curr = active_scope_id-1; curr != NONE; curr = scope_segments[curr].prev_lexical_segment_index){
        const auto& begin = symbols.cbegin() + scope_segments[curr].first_sym_index;
        const auto& end = symbols.cbegin() + scope_segments[curr+1].first_sym_index;
        for(auto it = begin; it < end; it++){
            const Symbol& sym = *it;
            assert(sym.declaration_lexical_depth == lexical_depth);

            if(sym.index_of_shadowed_var == NONE)
                lexical_map.erase(sym.sel(parse_tree)); //Much better to erase empty entries than check for them
            else
                lexical_map.find(sym.sel(parse_tree))->second = sym.index_of_shadowed_var;
        }
    }

    lexical_depth--;

    settings().leaveScope();
}

void SymbolLexicalPass::increaseClosureDepth(
        #ifdef FORSCAPE_USE_SCOPE_NAME
        const std::string& name,
        #endif
        const Typeset::Marker& begin, ParseNode pn) alloc_except {
    ref_frames.push_back(refs.size());

    closure_depth++;
    lexical_depth++;
    addScope(SCOPE_NAME(name)  begin, pn);
}

void SymbolLexicalPass::decreaseClosureDepth(const Typeset::Marker& end) alloc_except {
    ParseNode fn = scope_segments.back().fn;

    decreaseLexicalDepth(end);
    closure_depth--;

    //Find variables which are captured by reference in this closure, also modified by inner closures
    for(size_t seg_index = scope_segments.size()-2;
        seg_index != NONE;
        seg_index = scope_segments[seg_index].prev_lexical_segment_index){
        ScopeSegment& closed_seg = scope_segments[seg_index];
        for(size_t i = closed_seg.usage_begin; i < closed_seg.usage_end; i++){
            const SymbolUsage& usage = symbol_usages[i];
            Symbol& sym = symbols[usage.symbol_index];
            bool is_closed = (usage.prev_usage_index != NONE) && sym.is_closure_nested &&
                    (!sym.is_captured_by_value || sym.declaration_closure_depth <= closure_depth);
            if(!is_closed) continue;

            //The variable is closed, so we will add it to our book keeping
            if(sym.type != NONE) refs[sym.type] = NONE; //We have a more recent entry; mark the old one with a tombstone
            sym.type = refs.size();
            refs.push_back(usage.symbol_index);
        }
    }

    size_t cutoff = ref_frames.back();
    ref_frames.pop_back();
    parse_tree.prepareNary();
    for(size_t i = cutoff; i < refs.size(); i++){
        SymbolIndex sym_index = refs[i];
        if(sym_index == NONE) continue; //Tombstone: this node was promoted and we'll see it later
        Symbol& sym = symbols[sym_index];
        Op op = sym.declaration_closure_depth <= closure_depth ? OP_READ_UPVALUE : OP_IDENTIFIER;
        Typeset::Selection sel = symbol_table.getSel(sym_index);
        assert(sel.left != sel.right);
        ParseNode n = parse_tree.addTerminal(op, sel);
        assert(parse_tree.getSelection(n) == sel);
        parse_tree.setFlag(n, sym_index);
        parse_tree.addNaryChild(n);
        processed_refs.push_back(n);

        if(sym.declaration_closure_depth <= (closure_depth - sym.is_captured_by_value)){
            refs[cutoff] = sym_index;
            sym.type = cutoff++;
        }
    }
    Typeset::Selection sel = parse_tree.getSelection(fn);
    ParseNode list = parse_tree.finishNary(OP_LIST, sel);
    parse_tree.setRefList(fn, list);

    refs.resize(cutoff);
}

void SymbolLexicalPass::makeEntry(const Typeset::Selection& c, ParseNode pn, bool immutable) alloc_except {
    assert(lexical_map.find(c) == lexical_map.end());
    lexical_map[c] = symbols.size();
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, NONE, immutable);
}

void SymbolLexicalPass::appendEntry(ParseNode pn, size_t& old_entry, bool immutable, bool warn_on_shadow) alloc_except {
    if(warn_on_shadow){
        error_stream.warn(WARN_SHADOWING, parse_tree.getSelection(pn), SHADOWING_VAR);

        //EVENTUALLY: how do you make an error message with multiple links?
        error_stream.warn(WARN_SHADOWING, symbols[old_entry].sel(parse_tree), "Previous declaration here", SHADOWING_VAR);
    }
    symbol_table.addSymbol(pn, lexical_depth, closure_depth, old_entry, immutable);
    old_entry = symbols.size()-1;
}

}

}
