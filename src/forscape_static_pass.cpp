#include "forscape_static_pass.h"

#include "forscape_error.h"
#include "forscape_parse_tree.h"
#include "forscape_program.h"
#include "forscape_symbol_table.h"
#include "typeset_model.h"

namespace Forscape {

namespace Code {

StaticPass::StaticPass(
        ParseTree& parse_tree,
        ErrorStream& error_stream) noexcept
    : parse_tree(parse_tree), error_stream(error_stream) {}

void StaticPass::resolve(Typeset::Model* entry_point){
    active_model = entry_point;

    reset();
    parse_tree = entry_point->parser.parse_tree;

    if(!entry_point->errors.empty()) return;

    parse_tree.root = resolveStmt(parse_tree.root);

    for(const auto& entry : called_func_map)
        if(entry.second.type == RECURSIVE_CYCLE)
            error(getFuncFromCallSig(entry.first), RECURSIVE_TYPE);
    if(!error_stream.noErrors()) return;

    for(const auto& entry : all_calls){
        ParseNode call = entry.first;
        const CallSignature& fn = entry.second;
        const DeclareSignature& dec = declared(fn[0]);
        ParseNode abstract_fn = dec[0];
        instantiation_lookup[std::make_pair(abstract_fn, call)] = called_func_map[fn].instantiated;
    }

    //Not sure about the soundness of the recursion handling strategy, so let's double check
    //EVENTUALLY: remove this check
    //const auto call_map_backup = called_func_map;
    //for(const auto& entry : call_map_backup){
    //    called_func_map.erase(entry.first);
    //    assert(instantiate(ParseTree::EMPTY, entry.first) == entry.second.type);
    //}

    //EVENTUALLY: replace this with something less janky
    parse_tree.patchClonedTypes();

    finaliseSymbolTable(active_model);
    for(Typeset::Model* m : imported_models) finaliseSymbolTable(m);
}

void StaticPass::reset() noexcept {
    clear();
    memoized_abstract_function_groups.clear();
    declared_funcs.clear();
    declared_func_map.clear();
    called_func_map.clear();
    instantiation_lookup.clear();
    all_calls.clear();
    number_switch.clear();
    string_switch.clear();
    imported_models.clear();
    assert(return_types.empty());
    assert(retry_at_recursion == false);
    assert(first_attempt == true);
    assert(recursion_fallback == nullptr);

    Program::instance()->reset();

    #ifndef NDEBUG
    parse_tree.aliases.clear();
    #endif
}

ParseNode StaticPass::resolveStmt(ParseNode pn) noexcept {
    if(!error_stream.noErrors()) return pn;

    assert(pn != NONE);

    switch (parse_tree.getOp(pn)) {
        case OP_SETTINGS_UPDATE:{
            //EVENTUALLY: settings should only be applied to the lexical scope
            // not a concern now since the static pass and interpreter will undergo a total rewrite
            settings().enact( parse_tree.getFlag(pn) );
            Typeset::Selection sel(parse_tree.getSelection(pn));
            return parse_tree.addTerminal(OP_DO_NOTHING, sel);
        }

        case OP_DO_NOTHING:
            return pn;

        case OP_ASSIGN:
        case OP_EQUAL:{
            ParseNode rhs = resolveExprTop(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            Symbol& sym = *parse_tree.getSymbol(parse_tree.lhs(pn));
            sym.type = parse_tree.getType(rhs);
            sym.rows = parse_tree.getRows(rhs);
            sym.cols = parse_tree.getCols(rhs);
            return pn;
        }
        case OP_REASSIGN:{
            ParseNode lhs = resolveLValue(parse_tree.lhs(pn), true);
            if(!error_stream.noErrors()) return lhs;

            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getOp(lhs) == OP_SUBSCRIPT_ACCESS){
                ParseNode rhs = resolveExprTop(parse_tree.rhs(pn));
                parse_tree.setArg<1>(pn, rhs);

                for(size_t i = 0; i < parse_tree.getNumArgs(lhs); i++){
                    ParseNode arg = resolveExpr(parse_tree.arg(lhs, i));
                    parse_tree.setArg(lhs, i, arg);
                    if(parse_tree.getType(arg) != NUMERIC) return error(pn, arg);
                }
                if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);

                parse_tree.setType(pn, OP_REASSIGN_SUBSCRIPT);
                return pn;
            }else{
                Symbol& sym = *parse_tree.getSymbol(lhs);

                ParseNode rhs = resolveExprTop(parse_tree.rhs(pn), sym.rows, sym.cols);
                parse_tree.setArg<1>(pn, rhs);

                if(isAbstractFunctionGroup(sym.type)){
                    Type t = parse_tree.getType(rhs);
                    if(!isAbstractFunctionGroup(t)){
                        return error(pn, parse_tree.rhs(pn));
                    }else{
                        sym.type = functionSetUnion(sym.type, t);
                        return pn;
                    }
                }else{
                    if(parse_tree.getType(rhs) != sym.type) return error(pn, rhs);
                    return pn;
                }
            }
        }

        case OP_RETURN:{
            assert(!return_types.empty());
            ParseNode child = resolveExprTop(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            Type child_type = parse_tree.getType(child);
            ReturnType& rt = return_types.top();
            Type expected = rt.type;

            if(expected == UNINITIALISED || expected == RECURSIVE_CYCLE){
                rt.type = child_type;
            }else if(isAbstractFunctionGroup(expected)){
                if(!isAbstractFunctionGroup(child_type)){
                    error(pn, child);
                }else{
                    rt.type = functionSetUnion(expected, child_type);
                }
            }else{
                if(child_type != expected) error(pn, pn);
            }

            size_t child_rows = parse_tree.getRows(child);
            if(rt.rows == UNKNOWN_SIZE) rt.rows = child_rows;
            else if(rt.rows != child_rows && child_rows != UNKNOWN_SIZE) return error(pn, child, DIMENSION_MISMATCH);

            return pn;
        }

        case OP_RETURN_EMPTY:{
            assert(!return_types.empty());
            if(return_types.top().type == UNINITIALISED){
                return_types.top().type = VOID_TYPE;
            }else if(return_types.top().type != VOID_TYPE){
                error(pn, pn);
            }
            return pn;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:{
            ParseNode lhs = parse_tree.lhs(pn);
            ParseNode var = resolveExprTop(parse_tree.arg<0>(lhs));
            parse_tree.setArg<0>(lhs, var);
            if(parse_tree.getType(var) != NUMERIC) return error(pn, var);

            for(size_t i = 1; i < parse_tree.getNumArgs(lhs); i++){
                ParseNode sub = parse_tree.arg(lhs, i);
                if(parse_tree.getOp(sub) == OP_IDENTIFIER){
                    Symbol& sym = *parse_tree.getSymbol(sub);
                    sym.type = NUMERIC;
                }else{
                    sub = resolveExprTop(sub);
                    parse_tree.setArg(lhs, i, sub);
                    if(parse_tree.getType(sub) != NUMERIC) return error(pn, sub);
                }
            }

            ParseNode rhs = resolveExprTop(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);

            return pn;
        }

        case OP_IF:
        case OP_WHILE:{
            ParseNode lhs = resolveExprTop(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != BOOLEAN) return error(pn, lhs);

            parse_tree.setArg<1>(pn, resolveStmt(parse_tree.arg<1>(pn)));
            return pn;
        }

        case OP_IF_ELSE:{
            ParseNode cond = resolveExprTop(parse_tree.arg<0>(pn));
            parse_tree.setArg<0>(pn, cond);
            if(parse_tree.getType(cond) != BOOLEAN) return error(pn, cond);

            parse_tree.setArg<1>(pn, resolveStmt(parse_tree.arg<1>(pn)));
            parse_tree.setArg<2>(pn, resolveStmt(parse_tree.arg<2>(pn)));
            return pn;
        }

        case OP_FOR:{
            parse_tree.setArg<0>(pn, resolveStmt(parse_tree.arg<0>(pn)));
            ParseNode cond = resolveExprTop(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, cond);
            if(parse_tree.getType(cond) != BOOLEAN) return error(pn, cond);
            parse_tree.setArg<2>(pn, resolveStmt(parse_tree.arg<2>(pn)));
            parse_tree.setArg<3>(pn, resolveStmt(parse_tree.arg<3>(pn)));
            return pn;
        }

        case OP_RANGED_FOR:{
            ParseNode iterable = resolveExprTop(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, iterable);

            //EVENTUALLY: extract a function for isIterable(type)
            switch (parse_tree.getType(iterable)) {
                case NUMERIC: break;
                default: return error(pn, iterable, TYPE_NOT_ITERABLE);
            }

            Symbol& sym = *parse_tree.getSymbol(parse_tree.arg<0>(pn));
            sym.type = parse_tree.getType(iterable);
            sym.rows = 1;
            sym.cols = 1;

            parse_tree.setArg<2>(pn, resolveStmt(parse_tree.arg<2>(pn)));
            return pn;
        }

        case OP_EXPR_STMT:{
            ParseNode expr = resolveExprTop(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, expr);
            if(parse_tree.getOp(expr) != OP_CALL || !isAbstractFunctionGroup(parse_tree.getType(parse_tree.arg<0>(expr)))){
                error_stream.warn(WARN_UNUSED_VARIABLE, parse_tree.getSelection(expr), UNUSED_EXPRESSION);
                parse_tree.setOp(pn, OP_DO_NOTHING);
            }

            //EVENTUALLY: check the call stmt has side effects

            return pn;
        }

        case OP_ALGORITHM: return resolveAlg(pn);

        case OP_BLOCK: return resolveBlock(pn);
        case OP_LEXICAL_SCOPE:
            parse_tree.setOp(pn, OP_BLOCK);
            return resolveBlock(pn);

        case OP_PRINT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode expr = resolveExprTop(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, expr);
                //EVENTUALLY: Specialise printing for type
                //            Make sure all types accepted here are actually printable (e.g. modules crash)
            }
            return pn;

        case OP_ASSERT:{
            ParseNode child = resolveExprTop(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != BOOLEAN) return error(pn, child);
            else if(parse_tree.getOp(child) == OP_TRUE) parse_tree.setOp(pn, OP_DO_NOTHING);
            //EVENTUALLY: assertion fires if OP_FALSE and codepath is definitely encountered
            //            probably still want to be able to run the code and fail at this point
            return pn;
        }

        case OP_PLOT:{
            ParseNode title = resolveExprTop(parse_tree.arg<0>(pn));
            if(parse_tree.getType(title) != STRING) return error(pn, title);
            parse_tree.setArg<0>(pn, title);

            ParseNode x_label = resolveExprTop(parse_tree.arg<1>(pn));
            if(parse_tree.getType(x_label) != STRING) return error(pn, x_label);
            parse_tree.setArg<1>(pn, x_label);

            ParseNode x = resolveExprTop(parse_tree.arg<2>(pn));
            if(parse_tree.getType(x) != NUMERIC) return error(pn, x);
            parse_tree.setArg<2>(pn, x);

            ParseNode y_label = resolveExprTop(parse_tree.arg<3>(pn));
            if(parse_tree.getType(y_label) != STRING) return error(pn, y_label);
            parse_tree.setArg<3>(pn, y_label);

            ParseNode y = resolveExprTop(parse_tree.arg<4>(pn));
            if(parse_tree.getType(y) != NUMERIC) return error(pn, y);
            parse_tree.setArg<4>(pn, y);

            return pn;
        }

        case OP_IMPORT:{
            //EVENTUALLY: what is the circular import execution method? How are cyclical errors reported?

            ParseNode var = parse_tree.getFlag(pn);
            Symbol& sym = *parse_tree.getSymbol(var);
            ParseNode file = parse_tree.child(pn);
            Typeset::Model* model = parse_tree.getModel(file);
            sym.type = MODULE;
            sym.flag = reinterpret_cast<size_t>(model);

            if(!model->is_imported){
                model->is_imported = true;
                model->parse_node_offset = parse_tree.offset();

                //EVENTUALLY: Reparsing the model shouldn't be necessary
                model->performSemanticFormatting();

                if(model->errors.empty()){
                    const ParseTree& imported = model->parser.parse_tree;
                    const ParseNode root = parse_tree.append(imported);
                    Typeset::Model* current = active_model;
                    active_model = model;
                    resolveStmt(root);
                    imported_models.push_back(model);
                    active_model = current;
                    parse_tree.setFlag(pn, root);
                }else{
                    parse_tree.setFlag(pn, NONE);
                    return error(pn, file, ERROR_IN_LOADED_FILE);
                }
            }else{
                parse_tree.setFlag(pn, NONE);
            }

            return pn;
        }

        case OP_FROM_IMPORT:{
            ParseNode file = parse_tree.arg<0>(pn);
            Typeset::Model* model = parse_tree.getModel(file);

            if(!model->is_imported){
                model->is_imported = true;
                model->parse_node_offset = parse_tree.offset();
                model->performSemanticFormatting();
                if(model->errors.empty()){
                    const ParseTree& imported = model->parser.parse_tree;
                    const ParseNode root = parse_tree.append(imported);
                    Typeset::Model* current = active_model;
                    active_model = model;
                    resolveStmt(root);
                    imported_models.push_back(model);
                    active_model = current;
                    parse_tree.setFlag(pn, root);
                }else{
                    parse_tree.setFlag(pn, NONE);
                    return error(pn, file, ERROR_IN_LOADED_FILE);
                }
            }else{
                parse_tree.setFlag(pn, NONE);
            }

            const auto& lexical_map = model->symbol_builder.symbol_table.lexical_map;

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2){
                ParseNode imported_var = parse_tree.arg(pn, i);
                ParseNode local_var = parse_tree.arg(pn, i+1);

                //EVENTUALLY: this mechanism for auto completion is terrible
                auto lookup = lexical_map.find(parse_tree.getSelection(imported_var));
                if(lookup == lexical_map.end()){
                    parse_tree.setOp(imported_var, OP_ERROR);

                    //DESIGN QUAGMIRE (AST)
                    active_model->parser.parse_tree.setOp(imported_var - active_model->parse_node_offset, OP_ERROR);

                    ParseNode err = error(pn, imported_var, MODULE_FIELD_NOT_FOUND);
                    active_model->errors.back().flag = reinterpret_cast<size_t>(&lexical_map);
                    return err;
                }

                Symbol* sym = reinterpret_cast<Symbol*>(lookup->second);

                //Getting the original var could be accomplished by updating the active model's lexical map,
                //but we choose a space cost over a dictionary op
                if(sym->aliased_var) sym = sym->aliased_var;

                if(local_var == NONE){
                    Symbol& carry_over = *parse_tree.getSymbol(imported_var);
                    carry_over.aliased_var = sym;

                    //EVENTUALLY: think about rules for warning symbol is not used in a module
                    // edge cases: in Python, imports can be chained accross multiple modules
                    sym->is_used = carry_over.is_used;

                    for(SymbolUsage* usage = carry_over.last_external_usage; usage != nullptr; usage = usage->prevUsage()){
                        usage->symbol_index = reinterpret_cast<size_t>(&sym);
                        //DESIGN QUAGMIRE (AST) - parse_tree strategy please
                        parse_tree.setSymbol(usage->pn + active_model->parse_node_offset, sym);
                        active_model->parser.parse_tree.setSymbol(usage->pn, sym);

                        if(usage->prevUsage() == nullptr){
                            usage->prev_usage_index = reinterpret_cast<size_t>(sym->last_external_usage);
                            break;
                        }
                    }
                    sym->last_external_usage = carry_over.last_external_usage;
                }else{
                    sym->is_used = true;
                    SymbolUsage* carry_over_usage = reinterpret_cast<SymbolUsage*>(parse_tree.getFlag(imported_var));
                    carry_over_usage->symbol_index = reinterpret_cast<size_t>(sym);
                    //DESIGN QUAGMIRE (AST) - parse_tree strategy please
                    parse_tree.setSymbol(carry_over_usage->pn + active_model->parse_node_offset, sym);
                    active_model->parser.parse_tree.setSymbol(carry_over_usage->pn, sym);
                    carry_over_usage->prev_usage_index = reinterpret_cast<size_t>(sym->last_external_usage);
                    sym->last_external_usage = carry_over_usage;

                    Symbol& alias = *parse_tree.getSymbol(local_var);
                    alias.type = ALIAS;
                    alias.setShadowedVar(sym);
                    alias.aliased_var = sym;

                    #ifndef NDEBUG
                    auto result = parse_tree.aliases.insert({sym->str(), {alias.str()}});
                    if(!result.second) result.first->second.insert(alias.str());
                    #endif
                }
            }

            return pn;
        }

        case OP_NAMESPACE:
            parse_tree.getSymbol(parse_tree.arg<0>(pn))->type = NAMESPACE;
            parse_tree.setArg<1>(pn, resolveBlock(parse_tree.arg<1>(pn)));
            return pn;

        case OP_CLASS:{
            ParseNode members = parse_tree.arg<2>(pn);
            for(size_t i = 0; i < parse_tree.getNumArgs(members); i++){
                ParseNode member = parse_tree.arg(members, i);
                parse_tree.setArg(members, i, resolveStmt(member));
            }

            return pn;
        }

        case OP_SWITCH: return resolveSwitch(pn);

        default:
            assert(false);
            return pn;
    }
}

ParseNode StaticPass::resolveLValue(ParseNode pn, bool write) noexcept {
    assert(pn != NONE);

    switch (parse_tree.getOp(pn)) {
        case OP_SUBSCRIPT_ACCESS:
        case OP_IDENTIFIER:
            return pn;
        case OP_SCOPE_ACCESS: return resolveScopeAccess(pn, write);
        default: assert(false); return NONE;
    }
}

Type StaticPass::fillDefaultsAndInstantiate(ParseNode call_node, StaticPass::CallSignature sig){
    const StaticPass::DeclareSignature& dec = declared(sig.front());
    ParseNode fn = dec.front();

    ParseNode params = parse_tree.paramList(fn);

    size_t n_params = parse_tree.getNumArgs(params);
    size_t n_args = 0;
    for(size_t i = 1; i < sig.size();){
        n_args++;
        if(sig[i] == NUMERIC){
            i += 3;
        }else{
            i++;
        }
    }

    if(n_args > n_params) return errorType(call_node, TOO_MANY_ARGS);

    //Resolve default args
    for(size_t i = n_args; i < n_params; i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) != OP_EQUAL) return errorType(call_node, TOO_FEW_ARGS);
        ParseNode default_var = parse_tree.lhs(param);
        const Symbol& sym = *parse_tree.getSymbol(default_var);
        Type t = sym.type;
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(sym.rows);
            sig.push_back(sym.cols);
        }
    }

    return instantiate(call_node, sig);
}

ParseNode StaticPass::resolveExprTop(size_t pn, size_t rows_expected, size_t cols_expected){
    pn = resolveExpr(pn, rows_expected, cols_expected);
    if(!encountered_autosize) return pn;
    encountered_autosize = false;
    second_dim_attempt = true;
    pn = resolveExpr(pn, rows_expected, cols_expected);
    if(encountered_autosize) return error(pn, pn, AUTOSIZE);
    second_dim_attempt = encountered_autosize = false;
    return pn;
}

ParseNode StaticPass::resolveExpr(size_t pn, size_t rows_expected, size_t cols_expected) noexcept {
    assert(pn != NONE);

    if(rows_expected == UNKNOWN_SIZE) rows_expected = parse_tree.getRows(pn);
    if(cols_expected == UNKNOWN_SIZE) cols_expected = parse_tree.getCols(pn);

    if(!error_stream.noErrors()) return pn;

    switch (parse_tree.getOp(pn)) {
        case OP_LIMIT: return resolveLimit(pn);
        case OP_DEFINITE_INTEGRAL: return resolveDefiniteIntegral(pn);

        case OP_GRAVITY:{
            static constexpr double GRAVITY = -9.8067;

            parse_tree.setType(pn, NUMERIC);
            if(rows_expected == 1 && cols_expected == 1){
                parse_tree.setScalar(pn);
                parse_tree.setOp(pn, OP_DECIMAL_LITERAL);
                parse_tree.setDouble(pn, GRAVITY);
                return pn;
            }else if(rows_expected == 3 && cols_expected == 1){
                parse_tree.setRows(pn, 3);
                parse_tree.setCols(pn, 1);
                parse_tree.setOp(pn, OP_MATRIX_LITERAL);
                parse_tree.setValue(pn, Eigen::Vector3d(0, 0, GRAVITY));
                parse_tree.getSelection(pn).format(SEM_PREDEFINEDMAT);
                return pn;
            }else if(rows_expected == UNKNOWN_SIZE || cols_expected == UNKNOWN_SIZE){
                encountered_autosize = true;
                if(second_dim_attempt) return error(pn, pn, AUTOSIZE);
                return pn;
            }else{
                return error(pn, pn, DIMENSION_MISMATCH);
            }
        }
        //EVENTUALLY: remove autosize
        case OP_IDENTITY_AUTOSIZE:
            parse_tree.setType(pn, NUMERIC);
            parse_tree.setRows(pn, rows_expected);
            parse_tree.setCols(pn, cols_expected);
            if(rows_expected == UNKNOWN_SIZE || cols_expected == UNKNOWN_SIZE){
                encountered_autosize = true;
                if(second_dim_attempt) return error(pn, pn, AUTOSIZE);
            }else if(parse_tree.definitelyScalar(pn)){
                return parse_tree.getOne(Typeset::Selection(parse_tree.getSelection(pn)));
            }else{
                parse_tree.getSelection(pn).format(SEM_PREDEFINEDMAT);
                parse_tree.setValue(pn, Eigen::MatrixXd::Identity(rows_expected, cols_expected));
                parse_tree.setOp(pn, OP_MATRIX_LITERAL);
            }
            return pn;
        case OP_UNIT_VECTOR_AUTOSIZE:{
            parse_tree.setType(pn, NUMERIC);
            ParseNode child = enforceSemiPositiveInt(parse_tree.child(pn));
            parse_tree.setArg<0>( pn, child );
            if(rows_expected > 1 && cols_expected > 1) return error(pn, pn, DIMENSION_MISMATCH);
            else if(rows_expected > 1 && cols_expected == UNKNOWN_SIZE) cols_expected = 1;
            else if(cols_expected > 1 && rows_expected == UNKNOWN_SIZE) rows_expected = 1;
            parse_tree.setRows(pn, rows_expected);
            parse_tree.setCols(pn, cols_expected);
            size_t entries = rows_expected*cols_expected;
            if(entries == UNKNOWN_SIZE){
                if(second_dim_attempt) return error(pn, pn, AUTOSIZE);
                encountered_autosize = true;
                return pn;
            }else{
                //EVENTUALLY: writing dynamic data in the parse tree vector is UB
                const Value& v = parse_tree.getValue(child);
                if(v.index() != Unitialized_index && std::get<double>(v) >= entries)
                    return error(pn, child, INDEX_OUT_OF_RANGE);

                Typeset::Selection sel = parse_tree.getSelection(pn);
                ParseNode rows = parse_tree.addTerminal(OP_INTEGER_LITERAL, sel);
                parse_tree.setScalar(rows);
                parse_tree.setFlag(rows, (double)rows_expected);
                parse_tree.setValue(rows, (double)rows_expected);
                ParseNode cols = parse_tree.addTerminal(OP_INTEGER_LITERAL, sel);
                parse_tree.setScalar(cols);
                parse_tree.setFlag(cols, (double)cols_expected);
                parse_tree.setValue(cols, (double)cols_expected);

                return resolveUnitVector(parse_tree.addNode<3>(OP_UNIT_VECTOR, sel, {child, rows, cols}));
            }
        }
        case OP_LAMBDA: return resolveLambda(pn);
        case OP_IDENTIFIER:
        case OP_READ_GLOBAL:
        case OP_READ_UPVALUE:{
            Symbol* sym = parse_tree.getSymbol(pn);
            while(sym->type == ALIAS) sym = sym->shadowedVar();
            parse_tree.setRows(pn, sym->rows);
            parse_tree.setCols(pn, sym->cols);
            parse_tree.setType(pn, sym->type);

            return pn;
        }
        case OP_CALL:
            return callSite(pn);
        case OP_IMPLICIT_MULTIPLY:
            return implicitMult(pn);
        case OP_ADDITION:{
            ParseNode arg0 = resolveExpr(parse_tree.arg(pn, 0), rows_expected, cols_expected);
            parse_tree.setArg<0>(pn, arg0);
            Type expected = parse_tree.getType(arg0);
            if(expected != NUMERIC && expected != STRING) return error(pn, arg0, TYPE_NOT_ADDABLE);
            parse_tree.setType(pn, expected);
            size_t arg0_rows = parse_tree.getRows(arg0);
            if(rows_expected == UNKNOWN_SIZE){
                rows_expected = arg0_rows;
                parse_tree.setRows(pn, arg0_rows);
            }else if(rows_expected != arg0_rows && arg0_rows != UNKNOWN_SIZE) return error(pn, arg0, DIMENSION_MISMATCH);
            size_t arg0_cols = parse_tree.getCols(arg0);
            if(cols_expected == UNKNOWN_SIZE){
                cols_expected = arg0_cols;
                parse_tree.setCols(pn, arg0_cols);
            }else if(cols_expected != arg0_cols && arg0_cols != UNKNOWN_SIZE) return error(pn, arg0, DIMENSION_MISMATCH);

            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i), rows_expected, cols_expected);
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != expected) return error(pn, arg, TYPE_NOT_ADDABLE);
                size_t arg_rows = parse_tree.getRows(arg);
                if(rows_expected == UNKNOWN_SIZE){
                    rows_expected = arg_rows;
                    parse_tree.setRows(pn, arg_rows);
                }else if(rows_expected != arg_rows && arg_rows != UNKNOWN_SIZE) return error(pn, arg, DIMENSION_MISMATCH);
                size_t arg_cols = parse_tree.getCols(arg);
                if(cols_expected == UNKNOWN_SIZE){
                    cols_expected = arg_cols;
                    parse_tree.setCols(pn, arg_cols);
                }else if(cols_expected != arg_cols && arg_cols != UNKNOWN_SIZE) return error(pn, arg, DIMENSION_MISMATCH);
            }

            return pn;
        }
        case OP_DERIVATIVE:
        case OP_PARTIAL:
            return resolveDeriv(pn);
        case OP_SLICE:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != NUMERIC) return error(pn, arg, BAD_READ_OR_SUBSCRIPT);
                if(dimsDisagree(parse_tree.getRows(arg), 1)) return error(pn, arg, DIMENSION_MISMATCH);
                if(dimsDisagree(parse_tree.getCols(arg), 1)) return error(pn, arg, DIMENSION_MISMATCH);
            }
            parse_tree.setScalar(pn);
            return pn;
        case OP_SLICE_ALL:
            parse_tree.setScalar(pn);
            return pn;
        case OP_MATRIX: return resolveMatrix(pn);
        case OP_MULTIPLICATION: return resolveMult(pn, rows_expected, cols_expected);
        case OP_SUBSCRIPT_ACCESS:{
            ParseNode lhs = parse_tree.arg<0>(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != NUMERIC) return error(pn, arg);
            }
            parse_tree.setType(pn, NUMERIC);
            return pn;
        }

        case OP_ELEMENTWISE_ASSIGNMENT:
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, arg);
                if(parse_tree.getType(arg) != NUMERIC) return error(pn, arg);
            }
            parse_tree.setType(pn, NUMERIC);
            return pn;
        case OP_IDENTITY_MATRIX: return resolveIdentity(pn);
        case OP_ONES_MATRIX: return resolveOnesMatrix(pn);
        case OP_UNIT_VECTOR: return resolveUnitVector(pn);
        case OP_ZERO_MATRIX: return resolveZeroMatrix(pn);
        case OP_GROUP_BRACKET:
        case OP_GROUP_PAREN:{
            ParseNode expr = resolveExpr(parse_tree.child(pn));
            parse_tree.copyDims(pn, expr);
            return expr;
        }
        case OP_SINGLE_CHAR_MULT_PROXY:
            return resolveExpr(parse_tree.getFlag(pn));
        case OP_POWER: return resolvePower(pn);
        case OP_ARCTANGENT2:
        case OP_ROOT:
        {
            parse_tree.setScalar(pn);
            ParseNode lhs = enforceScalar(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            ParseNode rhs = enforceScalar(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            return pn;
        }
        case OP_BINOMIAL:{
            parse_tree.setScalar(pn);
            ParseNode n = enforceSemiPositiveInt(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, n);
            ParseNode k = enforceSemiPositiveInt(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, k);
            return pn;
        }
        case OP_BACKSLASH:
        case OP_DIVIDE:
        case OP_FRACTION:{
            parse_tree.setType(pn, NUMERIC);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
            parse_tree.copyDims(pn, lhs);
            ParseNode rhs = enforceScalar(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            return pn;
        }
        case OP_DOT:{
            parse_tree.setScalar(pn);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);
            return pn;
        }

        case OP_CROSS:
        case OP_FORWARDSLASH:
        case OP_LOGARITHM_BASE:
        case OP_MODULUS:{
            parse_tree.setType(pn, NUMERIC);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);
            return pn;
        }
        case OP_NORM_p:{
            parse_tree.setScalar(pn);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);
            return pn;
        }
        case OP_SUBTRACTION:{
            parse_tree.setType(pn, NUMERIC);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);

            size_t r_lhs = parse_tree.getRows(lhs);
            size_t r_rhs = parse_tree.getRows(rhs);
            if(r_lhs == UNKNOWN_SIZE) parse_tree.setRows(pn, r_rhs);
            else if(r_rhs == UNKNOWN_SIZE || r_rhs == r_lhs) parse_tree.setRows(pn, r_lhs);
            else return error(pn, pn, DIMENSION_MISMATCH);

            size_t c_lhs = parse_tree.getCols(lhs);
            size_t c_rhs = parse_tree.getCols(rhs);
            if(c_lhs == UNKNOWN_SIZE) parse_tree.setCols(pn, c_rhs);
            else if(c_rhs == UNKNOWN_SIZE || c_lhs == c_rhs) parse_tree.setCols(pn, c_lhs);
            else return error(pn, pn, DIMENSION_MISMATCH);

            return pn;
        }
        case OP_LENGTH:{
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            size_t rows = parse_tree.getRows(child);
            size_t cols = parse_tree.getCols(child);
            if(rows != UNKNOWN_SIZE && cols != UNKNOWN_SIZE){
                if(rows > 1 && cols > 1) return error(pn, child, DIMENSION_MISMATCH);
                parse_tree.setOp(pn, OP_INTEGER_LITERAL);
                parse_tree.setValue(pn, static_cast<double>(rows*cols));
                return pn;
            }else{
                parse_tree.setArg<0>(pn, child);
                if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
                return pn;
            }
        }
        case OP_ROWS:{
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            size_t rows = parse_tree.getRows(child);
            if(rows != UNKNOWN_SIZE){
                parse_tree.setOp(pn, OP_INTEGER_LITERAL);
                parse_tree.setValue(pn, static_cast<double>(rows));
                return pn;
            }else{
                parse_tree.setArg<0>(pn, child);
                if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
                return pn;
            }
        }
        case OP_COLS:{
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            size_t cols = parse_tree.getCols(child);
            if(cols != UNKNOWN_SIZE){
                parse_tree.setOp(pn, OP_INTEGER_LITERAL);
                parse_tree.setValue(pn, static_cast<double>(cols));
                return pn;
            }else{
                parse_tree.setArg<0>(pn, child);
                if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
                return pn;
            }
        }
        case OP_ARCCOSINE:
        case OP_ARCCOTANGENT:
        case OP_ARCSINE:
        case OP_ARCTANGENT:
        case OP_COMP_ERR_FUNC:
        case OP_COSINE:
        case OP_ERROR_FUNCTION:
        case OP_HYPERBOLIC_ARCCOSECANT:
        case OP_HYPERBOLIC_ARCCOSINE:
        case OP_HYPERBOLIC_ARCCOTANGENT:
        case OP_HYPERBOLIC_ARCSECANT:
        case OP_HYPERBOLIC_ARCSINE:
        case OP_HYPERBOLIC_ARCTANGENT:
        case OP_HYPERBOLIC_COSECANT:
        case OP_SINE:
        case OP_SQRT:
        case OP_TANGENT:
        {
            parse_tree.setScalar(pn);
            ParseNode child = enforceScalar(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            return pn;
        }
        case OP_FACTORIAL:{
            ParseNode lhs = parse_tree.child(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

            parse_tree.setScalar(pn);
            ParseNode child = enforceSemiPositiveInt(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            return pn;
        }
        case OP_EXP:
        case OP_LOGARITHM:
        case OP_NATURAL_LOG:
        case OP_NORM:
        case OP_NORM_1:
        case OP_NORM_INFTY:
        case OP_ABS:
        {
            parse_tree.setScalar(pn);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
            return pn;
        }
        case OP_BIJECTIVE_MAPPING:{
            ParseNode lhs = parse_tree.child(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

            parse_tree.setType(pn, NUMERIC);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
            return pn;
        }
        case OP_DAGGER:{
            ParseNode lhs = parse_tree.child(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

            parse_tree.setType(pn, NUMERIC);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
            parse_tree.transposeDims(pn, child);
            return pn;
        }
        case OP_PSEUDO_INVERSE:{
            ParseNode lhs = parse_tree.child(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

            parse_tree.setType(pn, NUMERIC);
            ParseNode child = resolveExpr(parse_tree.child(pn), cols_expected, rows_expected);
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
            parse_tree.transposeDims(pn, child);
            return pn;
        }
        case OP_TRANSPOSE: return resolveTranspose(pn, rows_expected, cols_expected);
        case OP_MAYBE_TRANSPOSE: parse_tree.setType(pn, NUMERIC); return pn;
        case OP_UNARY_MINUS: return resolveUnaryMinus(pn);
        case OP_SUMMATION:
        case OP_PRODUCT:{
            ParseNode asgn = parse_tree.arg<0>(pn);
            assert(parse_tree.getOp(asgn) == OP_ASSIGN);
            Symbol& sym = *parse_tree.getSymbol(parse_tree.lhs(asgn));
            sym.type = NUMERIC;
            ParseNode initialiser = resolveExpr(parse_tree.rhs(asgn));
            parse_tree.setArg<1>(asgn, initialiser);
            if(parse_tree.getType(initialiser) != NUMERIC) return error(pn, initialiser);
            parse_tree.setScalar(parse_tree.lhs(asgn));

            ParseNode final = resolveExpr(parse_tree.arg<1>(pn));
            parse_tree.setArg<1>(pn, final);
            if(parse_tree.getType(final) != NUMERIC) return error(pn, initialiser);

            ParseNode body = resolveExpr(parse_tree.arg<2>(pn));
            parse_tree.setArg<2>(pn, body);
            if(parse_tree.getType(body) != NUMERIC) return error(pn, body);
            if(parse_tree.getOp(pn) == OP_PRODUCT && parse_tree.nonSquare(body)) return error(pn, body, DIMENSION_MISMATCH);

            parse_tree.setType(pn, NUMERIC);
            parse_tree.copyDims(pn, body);
            return pn;
        }
        case OP_LOGICAL_NOT:{
            parse_tree.setType(pn, BOOLEAN);
            ParseNode child = resolveExpr(parse_tree.child(pn));
            parse_tree.setArg<0>(pn, child);
            if(parse_tree.getType(child) != BOOLEAN) return error(pn, child);
            return pn;
        }
        case OP_LOGICAL_AND:
        case OP_LOGICAL_OR:{
            parse_tree.setType(pn, BOOLEAN);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            if(parse_tree.getType(lhs) != BOOLEAN) return error(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) != BOOLEAN) return error(pn, rhs);
            return pn;
        }
        case OP_LESS:
        case OP_GREATER:{
            parse_tree.setType(pn, BOOLEAN);
            for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
                ParseNode child = enforceScalar(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, child);
            }
            return pn;
        }
        case OP_CASES:{
            for(size_t i = 1; i < parse_tree.getNumArgs(pn); i+=2){
                ParseNode cond = resolveExpr(parse_tree.arg(pn, i));
                parse_tree.setArg(pn, i, cond);
                if(parse_tree.getType(cond) != BOOLEAN) return error(pn, cond, EXPECT_BOOLEAN);
            }

            Type expected = UNINITIALISED;
            size_t expected_index;
            for(expected_index = 0; expected_index < parse_tree.getNumArgs(pn); expected_index+=2){
                ParseNode arg = resolveExpr(parse_tree.arg(pn, expected_index));
                parse_tree.setArg(pn, expected_index , arg);
                expected = parse_tree.getType(arg);
                if(expected != RECURSIVE_CYCLE) break;
            }

            if(isAbstractFunctionGroup(expected))
                for(size_t i = expected_index+2; i < parse_tree.getNumArgs(pn); i+=2){
                    ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                    parse_tree.setArg(pn, i , arg);
                    Type t = parse_tree.getType(arg);
                    if(!isAbstractFunctionGroup(t)) return error(pn, arg);
                    expected = functionSetUnion(expected, t);
                }
            else
                for(size_t i = expected_index+2; i < parse_tree.getNumArgs(pn); i+=2){
                    ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
                    parse_tree.setArg(pn, i , arg);
                    Type t = parse_tree.getType(arg);
                    if(t != expected) return error(pn, arg);
                }
            parse_tree.setType(pn, expected);
            return pn;
        }
        case OP_EQUAL:
        case OP_NOT_EQUAL:{
            parse_tree.setType(pn, BOOLEAN);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(lhs) != parse_tree.getType(rhs)) return error(pn, pn);
            return pn;
        }
        case OP_APPROX:
        case OP_NOT_APPROX:{
            parse_tree.setType(pn, BOOLEAN);
            ParseNode lhs = resolveExpr(parse_tree.lhs(pn));
            parse_tree.setArg<0>(pn, lhs);
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(lhs) != parse_tree.getType(rhs)) return error(pn, pn);
            return pn;
        }
        case OP_DECIMAL_LITERAL:
        case OP_INTEGER_LITERAL:{
            double val = parse_tree.getDouble(pn);
            parse_tree.setScalar(pn);
            parse_tree.setValue(pn, val);
            return pn;
        }
        case OP_PI:
        case OP_EULERS_NUMBER:
        case OP_GOLDEN_RATIO:
        case OP_SPEED_OF_LIGHT:
        case OP_PLANCK_CONSTANT:
        case OP_REDUCED_PLANCK_CONSTANT:
        case OP_STEFAN_BOLTZMANN_CONSTANT:
            parse_tree.setScalar(pn);
            return pn;
        case OP_FALSE:
        case OP_TRUE:
            parse_tree.setType(pn, BOOLEAN);
            return pn;
        case OP_STRING:
            parse_tree.setType(pn, STRING);
            return pn;
        case OP_LINEAR_SOLVE:
            parse_tree.setArg<0>(pn, resolveExpr(parse_tree.lhs(pn)));
            parse_tree.setArg<1>(pn, resolveExpr(parse_tree.rhs(pn)));
            return pn;
        case OP_NORM_SQUARED:
            parse_tree.setScalar(pn);
            parse_tree.setArg<0>(pn, resolveExpr(parse_tree.child(pn)));
            return pn;
        case OP_INVERT:
        case OP_CHECK_SCALAR:
        case OP_CHECK_NAT:
        case OP_CHECK_POSITIVE_INT:
        case OP_CHECK_ZERO:
            parse_tree.setArg<0>(pn, resolveExpr(parse_tree.child(pn)));
            return pn;
        case OP_MATRIX_LITERAL:
            return pn;

        case OP_AMBIGUOUS_PARENTHETICAL: {
            //Parsed something with unclear precedence, like h(x)^y
            //This node is binary, with h as LHS and (x)^y as RHS
            //The first arg of RHS would be the arg, if this is a call
            ParseNode lhs = parse_tree.lhs(pn);
            if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

            lhs = resolveExpr(parse_tree.arg<0>(pn));
            if(isAbstractFunctionGroup(parse_tree.getType(lhs))){
                ParseNode high_prec = parse_tree.arg<1>(pn);
                parse_tree.setArg<1>(pn, parse_tree.arg<0>(high_prec));
                parse_tree.setOp(pn, OP_CALL);
                parse_tree.setArg<0>(high_prec, pn);
                return resolveExpr(high_prec);
            }else{
                parse_tree.setOp(pn, OP_MULTIPLICATION);
                return resolveMult(pn);
            }
        }

        case OP_SCOPE_ACCESS: return resolveScopeAccess(pn);

        default:
            //assert(false);
            return error(pn, pn, NOT_IMPLEMENTED);
    }
}

ParseNode StaticPass::patchSingleCharMult(ParseNode parent, ParseNode mult) noexcept {
    assert(parse_tree.getOp(mult) == OP_SINGLE_CHAR_MULT_PROXY);

    mult = parse_tree.getFlag(mult);
    size_t index = parse_tree.getNumArgs(mult)-1;
    parse_tree.setArg<0>(parent, parse_tree.arg(mult, index));
    parse_tree.setArg(mult, index, parent);

    return resolveMult(mult);
}

size_t StaticPass::callSite(size_t pn) noexcept {
    ParseNode lhs = parse_tree.arg<0>(pn);
    if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

    //pn = parse_tree.clone(pn);
    ParseNode call_expr = resolveExpr(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, call_expr);
    size_t node_size = parse_tree.getNumArgs(pn);
    Type callable_type = parse_tree.getType(call_expr);

    if(callable_type == NUMERIC){
        if(node_size == 2){
            ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
            parse_tree.setArg<1>(pn, rhs);
            if(parse_tree.getType(rhs) == NUMERIC){
                //parse_tree.setOp(pn, OP_MULTIPLICATION);
                //return resolveMult(pn);
                //EVENTUALLY: need to create a new node since the call isn't cloned
                return resolveMult(parse_tree.addNode<2>(OP_MULTIPLICATION, {call_expr, rhs}));
            }else{
                return error(pn, rhs);
            }
        }else{
            return error(pn, pn, NOT_CALLABLE);
        }
    }else if(callable_type == UNINITIALISED){
        return error(pn, call_expr, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(callable_type)){
        return error(pn, call_expr, NOT_CALLABLE);
    }

    CallSignature sig;
    assert(numElements(callable_type));
    sig.push_back(arg(callable_type, 0));

    for(size_t i = 1; i < node_size; i++){
        ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
        parse_tree.setArg(pn, i, arg);
        Type t = parse_tree.getType(arg);
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(parse_tree.getRows(arg));
            sig.push_back(parse_tree.getCols(arg));
        }
    }

    parse_tree.setType(pn, instantiateSetOfFuncs(pn, callable_type, sig));
    return pn;
}

size_t StaticPass::implicitMult(size_t pn, size_t start) noexcept {
    ParseNode lhs = resolveExpr(parse_tree.arg(pn, start));
    parse_tree.setArg(pn, start, lhs);
    Type tl = parse_tree.getType(lhs);

    if(start == parse_tree.getNumArgs(pn)-1) return lhs;
    else if(tl == NUMERIC){
        ParseNode rhs = implicitMult(pn, start+1);
        Type tl = parse_tree.getType(rhs);
        if(tl != NUMERIC) return error(pn, rhs);
        ParseNode mult = parse_tree.addNode<2>(OP_MULTIPLICATION, {lhs, rhs});
        return resolveMult(mult);
    }else if(tl == UNINITIALISED){
        return error(pn, lhs, CALL_BEFORE_DEFINE);
    }else if(!isAbstractFunctionGroup(tl)){
        return error(pn, lhs, NOT_CALLABLE);
    }

    std::vector<size_t> sig;
    assert(numElements(tl));
    sig.push_back(arg(tl, 0));

    ParseNode rhs = implicitMult(pn, start+1);
    Type tr = parse_tree.getType(rhs);
    sig.push_back(tr);
    if(tr == NUMERIC){
        sig.push_back(parse_tree.getRows(rhs));
        sig.push_back(parse_tree.getCols(rhs));
    }

    ParseNode call = parse_tree.addNode<2>(OP_CALL, {lhs, rhs});
    parse_tree.setType(call, instantiateSetOfFuncs(call, tl, sig));

    return call;
}

Type StaticPass::instantiateSetOfFuncs(ParseNode call_node, Type fun_group, CallSignature& sig){
    Type expected = RECURSIVE_CYCLE;
    size_t expected_index;

    for(expected_index = 0; expected_index < numElements(fun_group); expected_index++){
        sig[0] = arg(fun_group, expected_index);
        expected = fillDefaultsAndInstantiate(call_node, sig);

        if(expected != RECURSIVE_CYCLE) break;
    }

    if(expected == RECURSIVE_CYCLE){
        for(size_t i = 0; i < numElements(fun_group); i++){
            size_t decl_index = arg(fun_group, i);
            const DeclareSignature& dec = declared(decl_index);
            ParseNode fn = getFuncFromDeclSig(dec);
            if(parse_tree.getOp(fn) != OP_LAMBDA) fn = parse_tree.algName(fn);
            errorType(fn, RECURSIVE_TYPE);
        }
        return errorType(call_node, RECURSIVE_TYPE);
    }

    if(isAbstractFunctionGroup(expected)){
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return errorType(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(!isAbstractFunctionGroup(evaluated)) return errorType(call_node);
            expected = functionSetUnion(expected, evaluated);
        }
    }else{
        for(size_t i = expected_index+1; i < numElements(fun_group); i++){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return errorType(call_node);
        }
        for(size_t i = expected_index; i-->0;){
            sig[0] = arg(fun_group, i);
            Type evaluated = fillDefaultsAndInstantiate(call_node, sig);
            if(evaluated == RECURSIVE_CYCLE) continue;
            if(evaluated != expected) return errorType(call_node);
        }
    }

    return expected;
}

size_t StaticPass::error(ParseNode pn, ParseNode sel, ErrorCode code) noexcept {
    return error(pn, sel, std::string(getMessage(code)), code);
}

size_t StaticPass::error(ParseNode pn, ParseNode sel, const std::string& msg, ErrorCode code) noexcept {
    if(retry_at_recursion)
        parse_tree.setType(pn, RECURSIVE_CYCLE);
    else if(error_stream.noErrors())
        error_stream.fail(parse_tree.getSelection(sel), msg, code);
    return pn;
}

size_t StaticPass::errorType(ParseNode pn, ErrorCode code) noexcept {
    if(retry_at_recursion) return RECURSIVE_CYCLE;

    if(error_stream.noErrors())
        error_stream.fail(parse_tree.getSelection(pn), code);

    return FAILURE;
}

ParseNode StaticPass::getFuncFromCallSig(const CallSignature& sig) const noexcept {
    size_t decl_index = sig[0];
    const DeclareSignature& dec = declared(decl_index);

    return getFuncFromDeclSig(dec);
}

ParseNode StaticPass::getFuncFromDeclSig(const DeclareSignature& sig) const noexcept {
    return sig[0];
}

ParseNode StaticPass::resolveAlg(ParseNode pn){
    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            parse_tree.setArg(params, i, resolveStmt(param));
    }

    DeclareSignature sig;
    sig.push_back(pn);

    ParseNode cap_list = parse_tree.valCapList(pn);
    size_t cap_list_size = parse_tree.valListSize(cap_list);
    for(size_t i = 0; i < cap_list_size; i++){
        ParseNode cap = parse_tree.arg(cap_list, i);
        const Symbol& inner = *parse_tree.getSymbol(cap);
        const Symbol& outer = *inner.shadowedVar();
        Type t = outer.type;
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(outer.rows);
            sig.push_back(outer.cols);
        }
    }

    ParseNode ref_list = parse_tree.refCapList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        const Symbol& sym = *parse_tree.getSymbol(ref);
        Type t = sym.type;
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(sym.rows);
            sig.push_back(sym.cols);
        }
    }

    Type t = makeFunctionSet(declare(sig));

    parse_tree.getSymbol(parse_tree.algName(pn))->type = t;
    parse_tree.setType(pn, t);

    return pn;
}

ParseNode StaticPass::resolveSwitch(ParseNode pn) {
    ParseNode switch_key = resolveExprTop(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, switch_key);

    switch (parse_tree.getType(switch_key)) {
        case NUMERIC: return resolveSwitchNumeric(pn, switch_key);
        case STRING: return resolveSwitchString(pn, switch_key); //EVENTUALLY: generalise to templated function
        default: return error(pn, switch_key, UNSUPPORTED_SWITCH_TYPE);
    }
}

ParseNode StaticPass::resolveSwitchNumeric(ParseNode pn, ParseNode switch_key) {
    parse_tree.setFlag(pn, NONE);

    //Resolve codepaths, supporting fallthrough
    ParseNode last_codepath = NONE;
    for(size_t i = parse_tree.getNumArgs(pn); i-->1;){
        ParseNode case_node = parse_tree.arg(pn, i);
        ParseNode case_codepath = parse_tree.rhs(case_node);
        if(case_codepath != NONE)
            last_codepath = resolveStmt(case_codepath);
        parse_tree.setArg<1>(case_node, last_codepath);
    }

    //Resolve keys
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++){
        ParseNode case_node = parse_tree.arg(pn, i);

        ParseNode codepath = parse_tree.rhs(case_node);

        if(parse_tree.getOp(case_node) == OP_CASE){
            ParseNode case_key = resolveExpr(parse_tree.lhs(case_node));
            auto type = parse_tree.getType(case_key);
            if(type != NUMERIC) return error(pn, case_key, TYPE_ERROR);
            double val = parse_tree.getDouble(case_key);
            auto result = number_switch.insert({{pn, val}, codepath});
            if(!result.second) return error(pn, case_key, REDUNDANT_CASE);
        }else{
            assert(parse_tree.getOp(case_node) == OP_DEFAULT);
            if(parse_tree.getFlag(pn) != NONE){
                return error(pn, parse_tree.lhs(case_node), REDUNDANT_CASE);
            }
            parse_tree.setFlag(pn, case_node);
        }
    }

    parse_tree.setOp(pn, OP_SWITCH_NUMERIC);

    return pn;
}

ParseNode StaticPass::resolveSwitchString(ParseNode pn, ParseNode switch_key){
    parse_tree.setFlag(pn, NONE);

    //Resolve codepaths, supporting fallthrough
    ParseNode last_codepath = NONE;
    for(size_t i = parse_tree.getNumArgs(pn); i-->1;){
        ParseNode case_node = parse_tree.arg(pn, i);
        ParseNode case_codepath = parse_tree.rhs(case_node);
        if(case_codepath != NONE)
            last_codepath = resolveStmt(case_codepath);
        parse_tree.setArg<1>(case_node, last_codepath);
    }

    //Resolve keys
    for(size_t i = 1; i < parse_tree.getNumArgs(pn); i++){
        ParseNode case_node = parse_tree.arg(pn, i);

        ParseNode codepath = parse_tree.rhs(case_node);

        if(parse_tree.getOp(case_node) == OP_CASE){
            ParseNode case_key = resolveExpr(parse_tree.lhs(case_node));
            auto type = parse_tree.getType(case_key);
            if(type != STRING) return error(pn, case_key, TYPE_ERROR);
            //EVENTUALLY: this should use actual string values
            Typeset::Selection sel = parse_tree.getSelection(case_key);
            sel.left.index++;
            sel.right.index--;
            std::string val = sel.str();
            auto result = string_switch.insert({{pn, val}, codepath});
            if(!result.second) return error(pn, case_key, REDUNDANT_CASE);
        }else{
            assert(parse_tree.getOp(case_node) == OP_DEFAULT);
            if(parse_tree.getFlag(pn) != NONE){
                return error(pn, parse_tree.lhs(case_node), REDUNDANT_CASE);
            }
            parse_tree.setFlag(pn, case_node);
        }
    }

    parse_tree.setOp(pn, OP_SWITCH_STRING);

    return pn;
}

ParseNode StaticPass::resolveDeriv(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);

    ParseNode expr = parse_tree.arg<0>(pn);
    ParseNode wrt = parse_tree.arg<1>(pn);
    ParseNode val = parse_tree.arg<2>(pn);

    if(val == NONE) return error(pn, wrt, BAD_READ); //EVENTUALLY: treat deriv as func (known & restricted ℝ ↦ ℝ)
    val = resolveExpr(val);
    parse_tree.setArg<2>(pn, val);
    if(parse_tree.getType(val) != NUMERIC) return error(pn, wrt);

    Symbol& wrt_sym = *parse_tree.getSymbol(wrt);
    wrt_sym.type = NUMERIC;
    wrt_sym.rows = parse_tree.getRows(val);
    wrt_sym.cols = parse_tree.getCols(val);

    expr = resolveExpr(expr); //EVENTUALLY: symbolically take the derivative
    parse_tree.setArg<0>(pn, expr);
    if(parse_tree.getType(expr) != NUMERIC) return error(pn, expr);

    if(parse_tree.definitelyScalar(val)){
        parse_tree.copyDims(pn, expr);
    }else if(parse_tree.getCols(val) > 1 || parse_tree.getCols(expr) > 1){
        return error(pn, pn, DIMENSION_MISMATCH);
    }else if(parse_tree.getRows(val) > 1){
        parse_tree.setRows(pn, parse_tree.getRows(val));
        parse_tree.setCols(pn, parse_tree.getRows(expr));
    }

    return pn;
}

ParseNode StaticPass::resolveIdentity(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);

    ParseNode rows = enforceNaturalNumber(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, rows);
    ParseNode cols = enforceNaturalNumber(parse_tree.arg<1>(pn));
    parse_tree.setArg<1>(pn, cols);

    if(parse_tree.getOp(rows) == OP_INTEGER_LITERAL || parse_tree.getOp(rows) == OP_DECIMAL_LITERAL){
        double r = parse_tree.getDouble(rows);
        if(r < 1) return error(pn, rows, DIMENSION_MISMATCH);
        parse_tree.setRows(pn, static_cast<size_t>(r));
    }

    if(parse_tree.getOp(cols) == OP_INTEGER_LITERAL || parse_tree.getOp(cols) == OP_DECIMAL_LITERAL){
        double c = parse_tree.getDouble(cols);
        if(c < 1) return error(pn, cols, DIMENSION_MISMATCH);
        parse_tree.setCols(pn, c);
    }

    if(parse_tree.definitelyScalar(pn)) return parse_tree.getOne(Typeset::Selection(parse_tree.getSelection(pn)));
    if(parse_tree.getRows(pn) != UNKNOWN_SIZE && parse_tree.getCols(pn) != UNKNOWN_SIZE){
        parse_tree.setOp(pn, OP_MATRIX_LITERAL);
        parse_tree.setValue(pn, Eigen::MatrixXd::Identity(parse_tree.getRows(pn), parse_tree.getCols(pn)));
        parse_tree.reduceNumArgs(pn, 0);
        return pn;
    }

    return pn;
}

ParseNode StaticPass::resolveInverse(ParseNode pn){
    ParseNode child = parse_tree.child(pn);
    const auto rows = parse_tree.getRows(child);
    const auto cols = parse_tree.getCols(child);
    if(dimsDisagree(rows, cols)){
        const std::string msg = "Cannot invert non-square " + std::to_string(rows) + "×" + std::to_string(cols) + " matrix";
        return error(pn, pn, msg, DIMENSION_MISMATCH);
    }
    parse_tree.copyDims(pn, child);
    return pn;
}

ParseNode StaticPass::resolveLambda(ParseNode pn){
    ParseNode params = parse_tree.paramList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL)
            parse_tree.setArg(params, i, resolveStmt(param));
    }

    DeclareSignature sig;
    sig.push_back(pn);

    ParseNode cap_list = parse_tree.valCapList(pn);
    size_t cap_list_size = parse_tree.valListSize(cap_list);
    for(size_t i = 0; i < cap_list_size; i++){
        ParseNode cap = parse_tree.arg(cap_list, i);
        size_t inner_id = parse_tree.getFlag(cap);
        assert(inner_id < symbolTable().symbols.size());
        const Symbol& inner = symbolTable().symbols[inner_id];
        const Symbol& outer = *inner.shadowedVar();
        Type t = outer.type;
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(outer.rows);
            sig.push_back(outer.cols);
        }
    }

    ParseNode ref_list = parse_tree.refCapList(pn);
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        const Symbol& sym = *parse_tree.getSymbol(ref);
        Type t = sym.type;
        sig.push_back(t);
        if(t == NUMERIC){
            sig.push_back(sym.rows);
            sig.push_back(sym.cols);
        }
    }

    Type t = makeFunctionSet(declare(sig));
    parse_tree.setType(pn, t);

    return pn;
}

ParseNode StaticPass::resolveMatrix(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);
    size_t nargs = parse_tree.getNumArgs(pn);
    for(size_t i = 0; i < nargs; i++){
        ParseNode arg = resolveExpr(parse_tree.arg(pn, i));
        parse_tree.setArg(pn, i, arg);
        if(parse_tree.getType(arg) != NUMERIC) return error(pn, arg);
    }

    if(nargs == 1) return copyChildProperties(pn);

    size_t typeset_rows = parse_tree.getFlag(pn);
    size_t typeset_cols = nargs/typeset_rows;
    assert(typeset_cols*typeset_rows == nargs);

    static std::vector<size_t> elem_cols; elem_cols.resize(typeset_cols);
    static std::vector<size_t> elem_rows; elem_rows.resize(typeset_rows);
    static std::vector<ParseNode> elements; elements.resize(nargs);

    size_t curr = 0;
    for(size_t i = 0; i < typeset_rows; i++){
        for(size_t j = 0; j < typeset_cols; j++){
            ParseNode arg = parse_tree.arg(pn, curr++);

            if(i==0) elem_cols[j] = parse_tree.getCols(arg);
            else if(elem_cols[j] == UNKNOWN_SIZE) elem_cols[j] = parse_tree.getCols(arg);
            else if(dimsDisagree(elem_cols[j], parse_tree.getCols(arg))) return error(pn, pn, DIMENSION_MISMATCH);

            if(j==0) elem_rows[i] = parse_tree.getRows(arg);
            else if(elem_rows[i] == UNKNOWN_SIZE) elem_rows[i] = parse_tree.getRows(arg);
            else if(dimsDisagree(elem_rows[i], parse_tree.getRows(arg))) return error(pn, pn, DIMENSION_MISMATCH);
        }
    }

    size_t rows = 0;
    for(auto count : elem_rows)
        if(count != UNKNOWN_SIZE){
            rows += count;
        }else{
            rows = UNKNOWN_SIZE;
            break;
        }

    size_t cols = 0;
    for(auto count : elem_cols)
        if(count != UNKNOWN_SIZE){
            cols += count;
        }else{
            cols = UNKNOWN_SIZE;
            break;
        }

    parse_tree.setRows(pn, rows);
    parse_tree.setCols(pn, cols);

    return pn;
}

ParseNode StaticPass::resolveTranspose(ParseNode pn, size_t rows_expected, size_t cols_expected) {
    ParseNode lhs = parse_tree.child(pn);
    if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

    parse_tree.setType(pn, NUMERIC);
    ParseNode child = resolveExpr(lhs, cols_expected, rows_expected);
    if(parse_tree.getType(child) != NUMERIC) return error(pn, child);
    parse_tree.transposeDims(pn, child);
    if(parse_tree.definitelyScalar(child)) return child;
    return pn;
}

ParseNode StaticPass::resolveMult(ParseNode pn, size_t rows_expected, size_t cols_expected){
    //EVENTUALLY: the expectations are all wrong. This is dang tricky since matrix-matrix
    //            multiplication uses the same notation as matrix scale-by-scalar

    ParseNode lhs = resolveExpr(parse_tree.lhs(pn), rows_expected);
    parse_tree.setArg<0>(pn, lhs);
    Type lhs_type = parse_tree.getType(lhs);
    if(lhs_type != NUMERIC) return error(pn, lhs);
    size_t rhs_rows_expected = UNKNOWN_SIZE;
    if(parse_tree.definitelyScalar(lhs)) rhs_rows_expected = rows_expected;
    ParseNode rhs = resolveExpr(parse_tree.rhs(pn), rhs_rows_expected, cols_expected);
    parse_tree.setArg<1>(pn, rhs);
    if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);
    parse_tree.setType(pn, NUMERIC);

    if(parse_tree.definitelyScalar(lhs)){
        if(parse_tree.getRows(rhs) == UNKNOWN_SIZE) parse_tree.setRows(rhs, rows_expected);
        parse_tree.copyDims(pn, rhs);
    }else if(parse_tree.definitelyScalar(rhs)){
        if(parse_tree.getCols(lhs) == UNKNOWN_SIZE) parse_tree.setCols(rhs, cols_expected);
        parse_tree.copyDims(pn, lhs);
    }else if(parse_tree.definitelyNotScalar(lhs) && parse_tree.definitelyNotScalar(rhs)){
        parse_tree.setRows(pn, parse_tree.getRows(lhs));
        parse_tree.setCols(pn, parse_tree.getCols(rhs));
        if(dimsDisagree(parse_tree.getCols(lhs), parse_tree.getRows(rhs))) return error(pn, pn, DIMENSION_MISMATCH);
    }

    if(parse_tree.getOp(lhs) == OP_INVERT){
        parse_tree.setArg(pn, 0, parse_tree.child(lhs));
        parse_tree.setOp(pn, OP_LINEAR_SOLVE);
    }

    return pn;
}

ParseNode StaticPass::resolveOnesMatrix(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);

    ParseNode rows = enforceNaturalNumber(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, rows);
    ParseNode cols = enforceNaturalNumber(parse_tree.arg<1>(pn));
    parse_tree.setArg<1>(pn, cols);

    if(parse_tree.getOp(rows) == OP_INTEGER_LITERAL || parse_tree.getOp(rows) == OP_DECIMAL_LITERAL){
        double r = parse_tree.getDouble(rows);
        if(r < 1) return error(pn, rows, DIMENSION_MISMATCH);
        parse_tree.setRows(pn, r);
    }

    if(parse_tree.getOp(cols) == OP_INTEGER_LITERAL || parse_tree.getOp(cols) == OP_DECIMAL_LITERAL){
        double c = parse_tree.getDouble(cols);
        if(c < 1) return error(pn, cols, DIMENSION_MISMATCH);
        parse_tree.setCols(pn, c);
    }

    if(parse_tree.definitelyScalar(pn)) return parse_tree.getOne(Typeset::Selection(parse_tree.getSelection(pn)));

    if(parse_tree.getRows(pn) != UNKNOWN_SIZE && parse_tree.getCols(pn) != UNKNOWN_SIZE){
        parse_tree.setOp(pn, OP_MATRIX_LITERAL);
        parse_tree.setValue(pn, Eigen::MatrixXd::Ones(parse_tree.getRows(pn), parse_tree.getCols(pn)));
        parse_tree.reduceNumArgs(pn, 0);
        return pn;
    }

    return pn;
}

ParseNode StaticPass::resolvePower(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    if(parse_tree.getOp(lhs) == OP_SINGLE_CHAR_MULT_PROXY) return patchSingleCharMult(pn, lhs);

    lhs = resolveExpr(lhs);
    parse_tree.setArg<0>(pn, lhs);
    if(parse_tree.getType(lhs) != NUMERIC) return error(pn, lhs);
    ParseNode rhs = resolveExpr(parse_tree.rhs(pn));
    parse_tree.setArg<1>(pn, rhs);
    if(parse_tree.getType(rhs) != NUMERIC) return error(pn, rhs);
    parse_tree.setType(pn, NUMERIC);

    if(parse_tree.definitelyScalar(rhs)){
        parse_tree.copyDims(pn, lhs);
    }

    switch (parse_tree.getOp(rhs)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:{
            double rhs_val = parse_tree.getDouble(rhs);

            ParseNode lhs = parse_tree.lhs(pn);
            Code::Op lhs_op = parse_tree.getOp(lhs);
            if(lhs_op == OP_INTEGER_LITERAL || lhs_op == OP_DECIMAL_LITERAL){
                double val = pow(parse_tree.getDouble(lhs), rhs_val);
                parse_tree.setFlag(pn, val);
                parse_tree.setValue(pn, val);
                parse_tree.setOp(pn, OP_DECIMAL_LITERAL);
                parse_tree.reduceNumArgs(pn, 0);
                parse_tree.setScalar(pn);
                return pn;
            }else if(rhs_val == -1){
                parse_tree.setOp(pn, OP_INVERT);
                parse_tree.reduceNumArgs(pn, 1);
                return resolveInverse(pn);
            }else if(rhs_val == 1){
                return parse_tree.lhs(pn);
            }else if(rhs_val == 2){
                if(lhs_op == OP_NORM){
                    parse_tree.setOp(lhs, OP_NORM_SQUARED);
                    return lhs;
                }
            }
            //Can only simplify x^0 to 1 assuming no NaN in LHS

            break;
        }

        case OP_SPEED_OF_LIGHT:
            //EVENTUALLY: warn this is COMPLEMENT_C
            //warnings.push_back(Error(parse_tree.getSelection(rhs), COMPLEMENT_C));
            return error(pn, rhs, UNRECOGNIZED_SYMBOL);
            //return ast.setComplement(base);

        case OP_MAYBE_TRANSPOSE:
            error_stream.warn(WARN_TRANSPOSE_T, parse_tree.getSelection(rhs), TRANSPOSE_T);

            parse_tree.setOp(pn, OP_TRANSPOSE);
            parse_tree.reduceNumArgs(pn, 1);
            return resolveTranspose(pn);
    }

    return pn;
}

ParseNode StaticPass::resolveUnaryMinus(ParseNode pn){
    ParseNode child = resolveExpr(parse_tree.child(pn));
    parse_tree.setArg<0>(pn, child);
    parse_tree.copyDims(pn, child);
    if(parse_tree.getType(child) != NUMERIC) return error(pn, pn);
    parse_tree.setType(pn, NUMERIC);

    switch (parse_tree.getOp(child)) {
        case OP_INTEGER_LITERAL:
        case OP_DECIMAL_LITERAL:
            parse_tree.setDouble(child, -parse_tree.getDouble(child));
            return child;
        case OP_UNARY_MINUS:
            return parse_tree.child(child);
        default:
            parse_tree.setRows(pn, parse_tree.getRows(child));
            parse_tree.setCols(pn, parse_tree.getCols(child));
            return pn;
    }
}

ParseNode StaticPass::resolveUnitVector(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);

    ParseNode rows = enforceNaturalNumber(parse_tree.unitVectorRows(pn));
    parse_tree.setUnitVectorRows(pn, rows);
    ParseNode cols = enforceNaturalNumber(parse_tree.unitVectorCols(pn));
    parse_tree.setUnitVectorCols(pn, cols);

    if(parse_tree.getOp(rows) == OP_INTEGER_LITERAL || parse_tree.getOp(rows) == OP_DECIMAL_LITERAL){
        double r = parse_tree.getDouble(rows);
        if(r < 1) return error(pn, rows, DIMENSION_MISMATCH);
        parse_tree.setRows(pn, r);
    }

    if(parse_tree.getOp(cols) == OP_INTEGER_LITERAL || parse_tree.getOp(cols) == OP_DECIMAL_LITERAL){
        double c = parse_tree.getDouble(cols);
        if(c < 1) return error(pn, cols, DIMENSION_MISMATCH);
        parse_tree.setCols(pn, c);
    }

    if(parse_tree.definitelyScalar(pn)){
        ParseNode elem = enforceZero(parse_tree.unitVectorElem(pn));
        parse_tree.setUnitVectorElem(pn, elem);

        if(parse_tree.getOp(elem) == OP_INTEGER_LITERAL || parse_tree.getOp(elem) == OP_DECIMAL_LITERAL)
            return parse_tree.getOne(Typeset::Selection(parse_tree.getSelection(pn)));

        return pn;
    }

    ParseNode elem = enforceSemiPositiveInt(parse_tree.unitVectorElem(pn));
    parse_tree.setUnitVectorElem(pn, elem);

    if((parse_tree.getOp(elem) == OP_INTEGER_LITERAL || parse_tree.getOp(elem) == OP_DECIMAL_LITERAL) &&
        parse_tree.getRows(pn) != UNKNOWN_SIZE && parse_tree.getCols(pn) != UNKNOWN_SIZE){
        Eigen::Index e = parse_tree.getDouble(elem);
        Eigen::Index r = parse_tree.getRows(pn);
        Eigen::Index c = parse_tree.getCols(pn);

        if(r > 1 && c > 1) return error(pn, pn, DIMENSION_MISMATCH);
        if(e >= r*c) return error(pn, pn, DIMENSION_MISMATCH);

        parse_tree.setOp(pn, OP_MATRIX_LITERAL);
        if(r > 1) parse_tree.setValue(pn, Eigen::VectorXd::Unit(r, e));
        else parse_tree.setValue(pn, Eigen::RowVectorXd::Unit(c, e));
        parse_tree.reduceNumArgs(pn, 0);
        return pn;
    }

    return pn;
}

ParseNode StaticPass::resolveZeroMatrix(ParseNode pn){
    parse_tree.setType(pn, NUMERIC);

    ParseNode rows = enforceNaturalNumber(parse_tree.arg<0>(pn));
    parse_tree.setArg<0>(pn, rows);
    ParseNode cols = enforceNaturalNumber(parse_tree.arg<1>(pn));
    parse_tree.setArg<1>(pn, cols);

    if(parse_tree.getOp(rows) == OP_INTEGER_LITERAL || parse_tree.getOp(rows) == OP_DECIMAL_LITERAL){
        double r = parse_tree.getDouble(rows);
        if(r < 1) return error(pn, rows, DIMENSION_MISMATCH);
        parse_tree.setRows(pn, r);
    }

    if(parse_tree.getOp(cols) == OP_INTEGER_LITERAL || parse_tree.getOp(cols) == OP_DECIMAL_LITERAL){
        double c = parse_tree.getDouble(cols);
        if(c < 1) return error(pn, cols, DIMENSION_MISMATCH);
        parse_tree.setCols(pn, c);
    }

    if(parse_tree.definitelyScalar(pn)) return parse_tree.getZero(Typeset::Selection(parse_tree.getSelection(pn)));

    if(parse_tree.getRows(pn) != UNKNOWN_SIZE && parse_tree.getCols(pn) != UNKNOWN_SIZE){
        parse_tree.setOp(pn, OP_MATRIX_LITERAL);
        parse_tree.setValue(pn, Eigen::MatrixXd::Zero(parse_tree.getRows(pn), parse_tree.getCols(pn)));
        parse_tree.reduceNumArgs(pn, 0);
        return pn;
    }

    return pn;
}

ParseNode StaticPass::resolveLimit(ParseNode pn) {
    ParseNode lim = resolveExpr(parse_tree.arg<1>(pn));
    if(parse_tree.getType(lim) != NUMERIC) return error(pn, lim);
    parse_tree.setArg<1>(pn, lim);

    ParseNode id = parse_tree.arg<0>(pn);
    parse_tree.setType(id, NUMERIC);
    parse_tree.copyDims(id, lim);

    Symbol& id_sym = *parse_tree.getSymbol(id);
    id_sym.type = NUMERIC;
    id_sym.rows = parse_tree.getRows(id);
    id_sym.cols = parse_tree.getCols(id);

    ParseNode expr = resolveExpr(parse_tree.arg<2>(pn));
    if(parse_tree.getType(expr) != NUMERIC) return error(pn, expr);
    parse_tree.setArg<2>(pn, expr);

    parse_tree.setType(pn, NUMERIC);
    parse_tree.copyDims(pn, expr);

    //EVENTUALLY: do symbolic computation with limits
    //return pn;
    return error(pn, pn, LIMIT_NOT_SUPPORTED);
}

ParseNode StaticPass::resolveDefiniteIntegral(ParseNode pn) {
    ParseNode tf = enforceScalar(parse_tree.arg<1>(pn));
    parse_tree.setArg<1>(pn, tf);
    ParseNode t0 = enforceScalar(parse_tree.arg<2>(pn));
    parse_tree.setArg<2>(pn, t0);

    ParseNode var = parse_tree.arg<0>(pn);
    parse_tree.setScalar(var);

    Symbol& var_sym = *parse_tree.getSymbol(var);
    var_sym.type = NUMERIC;
    var_sym.rows = 1;
    var_sym.cols = 1;

    ParseNode e = resolveExpr(parse_tree.arg<3>(pn));
    if(parse_tree.getType(e) != NUMERIC) return error(pn, e);
    parse_tree.setArg<3>(pn, e);

    parse_tree.setType(pn, NUMERIC);
    parse_tree.copyDims(pn, e);

    return pn;
}

ParseNode StaticPass::resolveScopeAccess(ParseNode pn, bool write) {
    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode lhs_lvalue = resolveLValue(lhs);
    ParseNode field = parse_tree.arg<1>(pn);
    if(!error_stream.noErrors()) //EVENTUALLY: should support prior errors
        return error(pn, pn, NOT_A_SCOPE);
    size_t sym_id = parse_tree.getSymId(lhs_lvalue);
    Symbol& sym = *parse_tree.getSymbol(lhs_lvalue);

    assert(parse_tree.getOp(field) == OP_ERROR);

    if(sym.type == MODULE){
        Typeset::Model* model = reinterpret_cast<Typeset::Model*>(sym.flag);
        const auto& lexical_map = model->symbol_builder.symbol_table.lexical_map;
        auto lookup = lexical_map.find(parse_tree.getSelection(field));
        //EVENTUALLY: this mechanism is terrible!
        if(lookup == lexical_map.end()){
            ParseNode err = error(pn, field, MODULE_FIELD_NOT_FOUND);
            active_model->errors.back().flag = reinterpret_cast<size_t>(&lexical_map);
            return err;
        }
        parse_tree.setOp(field, OP_IDENTIFIER);

        Symbol& sym = *reinterpret_cast<Symbol*>(lookup->second);
        SymbolUsage& usage = *reinterpret_cast<SymbolUsage*>(parse_tree.getFlag(field));

        ParseNode pn = usage.pn;

        //Patch the empty usage inserted earlier
        usage.symbol_index = reinterpret_cast<size_t>(&sym);
        parse_tree.setSymbol(pn + active_model->parse_node_offset, &sym);
        //DESIGN QUAGMIRE (AST): handle parse_tree logic please
        active_model->parser.parse_tree.setOp(field - active_model->parse_node_offset, OP_IDENTIFIER);
        active_model->parser.parse_tree.setSymbol(pn, &sym);
        usage.prev_usage_index = reinterpret_cast<size_t>(sym.last_external_usage);
        sym.last_external_usage = &usage;

        if(write){
            if(sym.is_const){
                return error(pn + active_model->parse_node_offset, field, REASSIGN_CONSTANT);
            }else{
                sym.is_reassigned = true;
            }
        }else{
            sym.is_used = true;
        }

        parse_tree.setType(field, sym.type);
        parse_tree.setRows(field, sym.rows);
        parse_tree.setCols(field, sym.cols);
        return field;
    }else if(sym.type == NAMESPACE){
        auto lookup = symbolTable().scoped_vars.find(SymbolTable::ScopedVarKey(sym_id, parse_tree.getSelection(field)));
        if(lookup != symbolTable().scoped_vars.end()){
            parse_tree.setOp(field, OP_IDENTIFIER);
            Symbol& sym = *reinterpret_cast<Symbol*>(lookup->second);
            SymbolUsage& usage = *reinterpret_cast<SymbolUsage*>(parse_tree.getFlag(field));

            ParseNode pn = usage.pn;

            //Patch the empty usage inserted earlier
            usage.symbol_index = reinterpret_cast<size_t>(&sym);
            parse_tree.setSymbol(pn + active_model->parse_node_offset, &sym);
            //DESIGN QUAGMIRE (AST): handle parse_tree logic please
            active_model->parser.parse_tree.setOp(field - active_model->parse_node_offset, OP_IDENTIFIER);
            active_model->parser.parse_tree.setSymbol(pn, &sym);

            usage.prev_usage_index = reinterpret_cast<size_t>(sym.last_external_usage);
            sym.last_external_usage = &usage;

            if(write){
                if(sym.is_const){
                    return error(pn + active_model->parse_node_offset, field, REASSIGN_CONSTANT);
                }else{
                    sym.is_reassigned = true;
                }
            }else{
                sym.is_used = true;
            }

            parse_tree.setType(field, sym.type);
            parse_tree.setRows(field, sym.rows);
            parse_tree.setCols(field, sym.cols);
            return field;
        }else{
            return error(pn, field, BAD_READ);
        }
    }else{
        return error(pn, lhs, NOT_A_SCOPE);
    }
}

ParseNode StaticPass::resolveBlock(ParseNode pn) {
    for(size_t i = 0; i < parse_tree.getNumArgs(pn); i++){
        ParseNode stmt = resolveStmt(parse_tree.arg(pn, i));
        parse_tree.setArg(pn, i, stmt);
    }
    return pn;
}

ParseNode StaticPass::copyChildProperties(ParseNode pn) noexcept {
    ParseNode child = parse_tree.child(pn);
    parse_tree.setRows(pn, parse_tree.getRows(child));
    parse_tree.setCols(pn, parse_tree.getCols(child));
    parse_tree.setValue(pn, parse_tree.getValue(child));

    return child;
}

ParseNode StaticPass::enforceScalar(ParseNode pn){
    pn = resolveExpr(pn);

    if(parse_tree.getType(pn) != NUMERIC)
        return error(pn, pn, TYPE_ERROR);
    if(parse_tree.getRows(pn) > 1 || parse_tree.getCols(pn) > 1)
        return error(pn, pn, EXPECT_SCALAR);
    if(parse_tree.getRows(pn) == UNKNOWN_SIZE || parse_tree.getCols(pn) == UNKNOWN_SIZE){
        pn = parse_tree.addUnary(OP_CHECK_SCALAR, pn);
        parse_tree.setScalar(pn);
    }

    return pn;
}

ParseNode StaticPass::enforceZero(ParseNode pn){
    pn = resolveExpr(pn);

    if(parse_tree.getType(pn) != NUMERIC)
        return error(pn, pn, TYPE_ERROR);
    if(parse_tree.getRows(pn) > 1 || parse_tree.getCols(pn) > 1)
        return error(pn, pn, EXPECT_SCALAR);
    if(parse_tree.getRows(pn) == UNKNOWN_SIZE || parse_tree.getCols(pn) == UNKNOWN_SIZE ||
       (parse_tree.getOp(pn) != OP_INTEGER_LITERAL && parse_tree.getOp(pn) != OP_DECIMAL_LITERAL)){
        pn = parse_tree.addUnary(OP_CHECK_ZERO, pn);
        parse_tree.setScalar(pn);
        return pn;
    }
    if(parse_tree.getDouble(pn) != 0)
        return error(pn, pn, INDEX_OUT_OF_RANGE);

    return pn;
}

ParseNode StaticPass::enforceNaturalNumber(ParseNode pn){
    pn = resolveExpr(pn);

    if(parse_tree.getType(pn) != NUMERIC)
        return error(pn, pn, TYPE_ERROR);
    if(parse_tree.getRows(pn) > 1 || parse_tree.getCols(pn) > 1)
        return error(pn, pn, EXPECT_SCALAR);
    if(parse_tree.getRows(pn) == UNKNOWN_SIZE || parse_tree.getCols(pn) == UNKNOWN_SIZE ||
       (parse_tree.getOp(pn) != OP_INTEGER_LITERAL && parse_tree.getOp(pn) != OP_DECIMAL_LITERAL)){
        pn = parse_tree.addUnary(OP_CHECK_NAT, pn);
        parse_tree.setScalar(pn);
        return pn;
    }
    if(parse_tree.getDouble(pn) < 1)
        return error(pn, pn, EXPECT_NATURAL_NUMBER);

    return pn;
}

ParseNode StaticPass::enforceSemiPositiveInt(ParseNode pn){
    pn = resolveExpr(pn);

    if(parse_tree.getType(pn) != NUMERIC)
        return error(pn, pn, TYPE_ERROR);
    if(parse_tree.getRows(pn) > 1 || parse_tree.getCols(pn) > 1)
        return error(pn, pn, EXPECT_SCALAR);
    if(parse_tree.getRows(pn) == UNKNOWN_SIZE || parse_tree.getCols(pn) == UNKNOWN_SIZE ||
       (parse_tree.getOp(pn) != OP_INTEGER_LITERAL && parse_tree.getOp(pn) != OP_DECIMAL_LITERAL)){
        pn = parse_tree.addUnary(OP_CHECK_POSITIVE_INT, pn);
        parse_tree.setScalar(pn);
        return pn;
    }
    if(parse_tree.getDouble(pn) < 0)
        return error(pn, pn, EXPECT_POSITIVE_INT);

    return pn;
}

bool StaticPass::dimsDisagree(size_t a, size_t b) noexcept {
    return a != UNKNOWN_SIZE && b != UNKNOWN_SIZE && a != b;
}

Settings& StaticPass::settings() const noexcept {
    return Program::instance()->settings;
}

SymbolTable& StaticPass::symbolTable() const noexcept {
    return active_model->symbol_builder.symbol_table;
}

void StaticPass::finaliseSymbolTable(Typeset::Model* model) const noexcept {
    auto& symbol_table = model->symbol_builder.symbol_table;

    for(Symbol& sym : symbol_table.symbols)
        if(!sym.is_used)
            error_stream.warn(static_cast<WarningLevel>(sym.use_level), sym.firstOccurence(), UNUSED_VAR);

    for(const SymbolUsage& usage : symbol_table.symbol_usages){
        const Symbol& sym = *usage.symbol();

        SemanticType fmt = SEM_ID;
        if(sym.is_ewise_index){
            fmt = SEM_ID_EWISE_INDEX;
        }else if(sym.is_closure_nested | sym.is_captured_by_value){
            fmt = SEM_LINK;
        }else if(!sym.is_const){
            fmt = SEM_ID_FUN_IMPURE;
        }

        usage.sel.format(fmt);
    }

    #ifndef NDEBUG
    if(!error_stream.noErrors()) return;
    assert(parse_tree.inFinalState());

    #ifndef FORSCAPE_TYPESET_HEADLESS
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

    //model->populateDocMapParseNodes(doc_map_nodes);

    //EVENTUALLY: check this later?
    //Every identifier in the doc map goes to a valid symbol
    //for(ParseNode pn : doc_map_nodes)
    //    if(parse_tree.getOp(pn) == OP_IDENTIFIER){
    //        symbol_table.verifyIdentifier(pn);
    //        selection_in_map.insert(parse_tree.getSelection(pn));
    //    }

    //Every usage in the symbol table is in the doc map
    //for(const Usage& usage : symbol_table.usages)
    //    assert(selection_in_map.find(parse_tree.getSelection(usage.pn)) != selection_in_map.end());
    #endif
    #endif

    for(const SymbolUsage& usage : symbol_table.symbol_usages){
        const Symbol& sym = *usage.symbol();

        if(sym.type == StaticPass::NUMERIC && (sym.rows > 1 || sym.cols > 1)){
            const Typeset::Selection& sel = usage.sel;
            switch (sel.getFormat()) {
                case SEM_ID: sel.format(SEM_ID_MAT); break;
                case SEM_PREDEF: sel.format(SEM_PREDEFINEDMAT); break;
                case SEM_ID_FUN_IMPURE: sel.format(SEM_ID_MAT_IMPURE); break;
                default: break;
            }
        }
    }
}

constexpr bool StaticPass::isAbstractFunctionGroup(size_t type) noexcept {
    return type < ALIAS;
}

Type StaticPass::declare(const DeclareSignature& fn){
    auto result = declared_func_map.insert({fn, declared_funcs.size()});
    if(result.second) declared_funcs.push_back(fn);

    return result.first->second;
}

Type StaticPass::instantiate(ParseNode call_node, const CallSignature& fn){
    all_calls.push_back(std::make_pair(call_node, fn));

    const DeclareSignature& dec = declared(fn[0]);
    ParseNode abstract_fn = dec[0];

    auto lookup = called_func_map.find(fn);
    if(lookup != called_func_map.end()){
        Type return_type = lookup->second.type;
        retry_at_recursion |= (return_type == RECURSIVE_CYCLE && first_attempt); //revisited in process of instantiation

        if(parse_tree.getRows(call_node) == UNKNOWN_SIZE)
            parse_tree.setRows(call_node, lookup->second.rows);
        else if(lookup->second.rows != UNKNOWN_SIZE
                && lookup->second.rows != parse_tree.getRows(call_node))
            errorType(call_node, DIMENSION_MISMATCH);

        if(parse_tree.getCols(call_node) == UNKNOWN_SIZE)
            parse_tree.setCols(call_node, lookup->second.cols);
        else if(lookup->second.cols != UNKNOWN_SIZE
                && lookup->second.cols != parse_tree.getCols(call_node))
            errorType(call_node, DIMENSION_MISMATCH);

        return return_type;
    }

    if(recursion_fallback == nullptr)
        recursion_fallback = &fn;

    //EVENTUALLY: should probably backup and restore entire lexical scope
    size_t old_val_cap_index = old_val_cap.size();
    size_t old_ref_cap_index = old_ref_cap.size();
    size_t old_args_index = old_args.size();

    //Instantiate
    called_func_map[fn] = CallResult(RECURSIVE_CYCLE, UNKNOWN_SIZE, UNKNOWN_SIZE, NONE);

    ParseNode instantiated_fn = parse_tree.clone(abstract_fn);

    ParseNode val_list = parse_tree.valCapList(instantiated_fn);
    ParseNode ref_list = parse_tree.refCapList(instantiated_fn);
    ParseNode params = parse_tree.paramList(instantiated_fn);
    size_t N_vals = parse_tree.valListSize(val_list);

    size_t type_index = 0;
    if(val_list != NONE){
        size_t scope_index = parse_tree.getFlag(val_list);
        const ScopeSegment& scope_segment = symbolTable().scope_segments[scope_index];
        for(size_t i = 0; i < N_vals; i++){
            size_t sym_id = scope_segment.first_sym_index + i;
            Symbol& sym = symbolTable().symbols[sym_id];
            old_val_cap.push_back(sym);
            sym.type = dec[1+type_index++];
            if(sym.type == NUMERIC){
                sym.rows = dec[1+type_index++];
                sym.cols = dec[1+type_index++];
            }
        }
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        Symbol& sym = *parse_tree.getSymbol(ref);
        old_ref_cap.push_back(sym);
        sym.type = dec[1+type_index++];
        if(sym.type == NUMERIC){
            sym.rows = dec[1+type_index++];
            sym.cols = dec[1+type_index++];
        }
    }

    type_index = 0;
    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        Symbol& sym = *parse_tree.getSymbol(param);
        old_args.push_back(sym);
        sym.type = fn[1+type_index++];
        parse_tree.setType(param, sym.type);
        if(sym.type == NUMERIC){
            sym.rows = fn[1+type_index++];
            sym.cols = fn[1+type_index++];
        }
    }

    Type return_type;
    size_t rows;
    size_t cols;

    bool is_alg = parse_tree.getOp(instantiated_fn) != OP_LAMBDA;

    if(is_alg){
        return_types.push(UNINITIALISED);
        ParseNode body = resolveStmt(parse_tree.body(instantiated_fn));
        parse_tree.setBody(instantiated_fn, body);
        return_type = return_types.top().type;
        if(return_type == UNINITIALISED) return_type = VOID_TYPE; //Function with no return statement
        rows = return_types.top().rows;
        cols = return_types.top().cols;
        return_types.pop();
    }else{
        ParseNode body = resolveExpr(parse_tree.body(instantiated_fn));
        parse_tree.setBody(instantiated_fn, body);
        return_type = parse_tree.getType(body);
        rows = parse_tree.getRows(body);
        cols = parse_tree.getCols(body);
    }

    called_func_map[fn] = CallResult(return_type, rows, cols, instantiated_fn);

    if(recursion_fallback == &fn){
        if(retry_at_recursion){
            retry_at_recursion = false;
            first_attempt = false;

            if(is_alg){
                return_types.push(UNINITIALISED);
                ParseNode body = resolveStmt(parse_tree.body(instantiated_fn));
                parse_tree.setBody(instantiated_fn, body);
                return_type = return_types.top().type;
                if(return_type == UNINITIALISED) return_type = VOID_TYPE; //Function with no return statement
                rows = return_types.top().rows;
                cols = return_types.top().cols;
                return_types.pop();
            }else{
                ParseNode body = resolveExpr(parse_tree.body(instantiated_fn));
                parse_tree.setBody(instantiated_fn, body);
                return_type = parse_tree.getType(body);
                rows = parse_tree.getRows(body);
                cols = parse_tree.getCols(body);
            }

            first_attempt = true;
            called_func_map[fn] = CallResult(return_type, rows, cols, instantiated_fn);
        }

        recursion_fallback = nullptr;
    }else if(first_attempt && return_type == RECURSIVE_CYCLE){
        //Didn't work, be sure to try instantiation again
        called_func_map.erase(fn);
    }

    for(size_t i = 0; i < N_vals; i++){
        ParseNode val = parse_tree.arg(val_list, i);
        Symbol& sym = *parse_tree.getSymbol(val);
        old_val_cap[i+old_val_cap_index].restore(sym);
    }
    old_val_cap.resize(old_val_cap_index);

    size_t j = 0;
    for(size_t i = 0; i < parse_tree.getNumArgs(ref_list); i++){
        ParseNode ref = parse_tree.arg(ref_list, i);
        if(parse_tree.getOp(ref) != OP_READ_UPVALUE) continue;
        Symbol& sym = *parse_tree.getSymbol(ref);
        old_ref_cap[j+old_ref_cap_index].restore(sym);
        j++;
    }
    old_ref_cap.resize(old_ref_cap_index);

    for(size_t i = 0; i < parse_tree.getNumArgs(params); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        Symbol& sym = *parse_tree.getSymbol(param);
        old_args[i+old_args_index].restore(sym);
    }
    old_args.resize(old_args_index);

    assert(parse_tree.getOp(call_node) == OP_CALL);

    if(parse_tree.getRows(call_node) == UNKNOWN_SIZE)
        parse_tree.setRows(call_node, rows);
    else if(rows != UNKNOWN_SIZE && rows != parse_tree.getRows(call_node))
        errorType(call_node, DIMENSION_MISMATCH);

    if(parse_tree.getCols(call_node) == UNKNOWN_SIZE)
        parse_tree.setCols(call_node, cols);
    else if(cols != UNKNOWN_SIZE && cols != parse_tree.getCols(call_node))
        errorType(call_node, DIMENSION_MISMATCH);

    return return_type;
}

static constexpr std::string_view type_strs[] = {
    "Alias",
    "Module",
    "Namespace",
    "Failure",
    "Recursive-Cycle",
    "Void",
    "Bool",
    "Str",
    "Num",
    "Unknown",
};

std::string StaticPass::typeString(Type t) const {
    if(isAbstractFunctionGroup(t)){
        return abstractFunctionSetString(t);
    }else{
        return std::string(type_strs[t - ALIAS]);
    }
}

std::string StaticPass::typeString(const Symbol& sym) const {
    if(sym.type == NUMERIC) return numTypeString(sym.rows, sym.cols);
    else return typeString(sym.type);
}

Type StaticPass::makeFunctionSet(ParseNode pn) noexcept {
    std::vector<ParseNode> possible_fns = {pn};

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(1);
        push_back(pn);
    }

    return lookup.first->second;
}

Type StaticPass::functionSetUnion(Type a, Type b){
    assert(v(a) == TYPE_FUNCTION_SET);
    assert(v(b) == TYPE_FUNCTION_SET);

    if(a == b) return a;

    std::vector<ParseNode> possible_fns;

    size_t last_a = last(a);
    size_t last_b = last(b);
    size_t index_a = first(a);
    size_t index_b = first(b);

    for(;;){
        ParseNode fn_a = v(index_a);
        ParseNode fn_b = v(index_b);

        if(fn_a <= fn_b){
            possible_fns.push_back(fn_a);
            index_a++;
            index_b += (fn_a == fn_b);
        }else{
            possible_fns.push_back(fn_b);
            index_b++;
        }

        if(index_a > last_a){
            while(index_b <= last_b) possible_fns.push_back(v(index_b++));
            break;
        }else if(index_b > last_b){
            while(index_a <= last_a) possible_fns.push_back(v(index_a++));
            break;
        }
    }

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(possible_fns.size());
        insert(end(), possible_fns.begin(), possible_fns.end());
    }

    return lookup.first->second;
}

size_t StaticPass::numElements(size_t index) const noexcept {
    assert(v(index) == TYPE_FUNCTION_SET);

    return v(index+1);
}

ParseNode StaticPass::arg(size_t index, size_t n) const noexcept {
    assert(n < numElements(index));
    return v(index + 2 + n);
}

const StaticPass::DeclareSignature& StaticPass::declared(size_t index) const noexcept {
    assert(index < declared_funcs.size());
    return declared_funcs[index];
}

size_t StaticPass::v(size_t index) const noexcept {
    return operator[](index);
}

size_t StaticPass::first(size_t index) const noexcept {
    return index+2;
}

size_t StaticPass::last(size_t index) const noexcept {
    index++;
    return index + v(index);
}

std::string StaticPass::abstractFunctionSetString(Type t) const {
    assert(v(t) == TYPE_FUNCTION_SET);

    if(numElements(t) == 1) return declFunctionString( v(first(t)) );

    size_t index = first(t);
    std::string str = "{" + declFunctionString( v(index++) );
    while(index <= last(t))
        str += ", " + declFunctionString(v(index++));
    str += "}";

    return str;
}

static void toSuperscript(const std::string& str, std::string& out){
    for(size_t i = 0; i < str.size(); i++){
        switch (str[i]) {
            case '0': out += "⁰"; break;
            case '1': out += "¹"; break;
            case '2': out += "²"; break;
            case '3': out += "³"; break;
            case '4': out += "⁴"; break;
            case '5': out += "⁵"; break;
            case '6': out += "⁶"; break;
            case '7': out += "⁷"; break;
            case '8': out += "⁸"; break;
            case '9': out += "⁹"; break;
            default: assert(false);
        }
    }
}

std::string StaticPass::numTypeString(size_t rows, size_t cols){
    std::string str = "ℝ";
    if(rows == 1 && cols == 1) return str;

    if(rows == UNKNOWN_SIZE){
        str += "ⁿ";
    }else{
        std::string dim = std::to_string(rows);
        toSuperscript(dim, str);
    }

    if(cols == 1) return str;

    str += "˟";
    if(cols == UNKNOWN_SIZE){
        str += "ᵐ";
    }else{
        std::string dim = std::to_string(cols);
        toSuperscript(dim, str);
    }

    return str;
}

std::string StaticPass::declFunctionString(size_t i) const {
    assert(i < declared_funcs.size());
    const DeclareSignature& fn = declared_funcs[i];
    assert(!fn.empty());
    ParseNode pn = fn[0];
    std::string str = parse_tree.getOp(pn) == OP_LAMBDA ?
                      "λ"
                      #ifndef NDEBUG
                      + std::to_string(pn)
                      #endif
                      : parse_tree.str(parse_tree.algName(pn));
    if(fn.size() >= 2){
        str += '[';
        if(fn[1] == NUMERIC) str += numTypeString(fn[2], fn[3]);
        else str += typeString(fn[1]);

        for(size_t i = 2+2*(fn[1] == NUMERIC); i < fn.size(); i++){
            str += ", ";
            if(fn[i] == NUMERIC){
                str += numTypeString(fn[i+1], fn[i+2]);
                i += 2;
            }else{
                str += typeString(fn[i]);
            }
        }
        str += ']';
    }

    return str;
}

std::string StaticPass::instFunctionString(const CallSignature& sig) const {
    std::string str = declFunctionString(sig[0]);
    str += "(";

    if(sig.size() >= 2){
        str += typeString(sig[1]);
        for(size_t i = 2; i < sig.size(); i++){
            str += ", ";
            str += typeString(sig[i]);
        }
    }

    str += ')';

    return str;
}

StaticPass::CachedInfo::CachedInfo(const Symbol& sym) noexcept
    : type(sym.type), rows(sym.rows), cols(sym.cols){}

void StaticPass::CachedInfo::restore(Symbol& sym) const noexcept {
    sym.type = type;
    sym.rows = rows;
    sym.cols = cols;
}

}

}
