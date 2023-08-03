#include "forscape_parser.h"

#include <code_parsenode_ops.h>
#include <forscape_common.h>
#include <forscape_program.h>
#include "typeset_construct.h"
#include "typeset_model.h"

#ifdef FORSCAPE_TYPESET_HEADLESS
#define registerParseNodeRegion(a, b)
#define registerParseNodeRegionToPatch(a)
#define startPatch()
#define finishPatch(a)
#endif

#define CONSUME_ASSERT(expected) \
    assert(tokens[index].type == expected); \
    advance();

namespace Forscape {

namespace Code {

Parser::Parser(const Scanner& scanner, Typeset::Model* model) noexcept
    : tokens(scanner.tokens), error_stream(Program::instance()->error_stream), model(model) {}

void Parser::parseAll() alloc_except {
    reset();

    parse_tree.prepareNary();
    skipNewlines();
    while (!peek(ENDOFFILE)) {
        parse_tree.addNaryChild(checkedStatement());
        skipNewlines();
    }

    Typeset::Selection c(tokens.front().sel.left, tokens.back().sel.right);
    parse_tree.root = parse_tree.finishNary(OP_BLOCK, c);

    //Patch lazy calculator
    if(parse_tree.getNumArgs(parse_tree.root) == 1){
        ParseNode stmt = parse_tree.child(parse_tree.root);
        if(parse_tree.getOp(stmt) == OP_EXPR_STMT)
            parse_tree.setOp(stmt, OP_PRINT);
    }

    assert(!error_stream.noErrors() || parse_tree.inFinalState());
}

void Parser::reset() noexcept {
    parse_tree.clear();
    token_map_stack.clear();
    #ifndef FORSCAPE_TYPESET_HEADLESS
    open_symbols.clear();
    close_symbols.clear();
    #endif
    index = 0;
    parsing_dims = false;
    loops = 0;
    error_node = model->errors.empty() ? NONE : PARSE_ERROR;
}

void Parser::registerGrouping(const Typeset::Selection& sel) alloc_except {
    registerGrouping(sel.left, sel.right);
}

void Parser::registerGrouping(const Typeset::Marker& l, const Typeset::Marker& r) alloc_except {
    #ifndef FORSCAPE_TYPESET_HEADLESS
    if(!noErrors()) return;
    open_symbols[l] = r;
    close_symbols[r] = l;
    #endif
}

ParseNode Parser::checkedStatement() alloc_except {
    ParseNode n = statement();
    if(noErrors()){
        return n;
    }else{
        recover();
        return n;
    }
}

ParseNode Parser::statement() alloc_except {
    skipNewlines();
    comment = (index >= 2 && tokens[index-2].type == COMMENT) ? index-2 : NONE;

    switch (currentType()) {
        case ALGORITHM: return algStatement();
        case ASSERT: return assertStatement();
        case BREAK: return loops ? terminalAndAdvance(OP_BREAK) : error(BAD_BREAK);
        case CLASS: return classStatement();
        case CONTINUE: return loops ? terminalAndAdvance(OP_CONTINUE) : error(BAD_CONTINUE);
        case ENUM: return enumStatement();
        case FOR: return forStatement();
        case FROM: return fromStatement();
        case IF: return ifStatement();
        case IMPORT: return importStatement();
        case LEFTBRACKET: return lexicalScopeStatement();
        case NAMESPACE: return namespaceStatement();
        case PLOT: return plotStatement();
        case PRINT: return printStatement();
        case RETURN: return returnStatement();
        case SWITCH: return switchStatement();
        case TOKEN_SETTINGS: return settingsStatement();
        case UNKNOWN: return unknownsStatement();
        case WHILE: return whileStatement();
        default: return mathStatement();
    }
}

ParseNode Parser::ifStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker cond_l = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);
    ParseNode condition = disjunction();
    Typeset::Marker cond_r = rMark();
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(cond_l, cond_r);
    ParseNode body = blockStatement();
    if(match(ELSE)){
        size_t el = blockStatement();
        Typeset::Marker right = rMark();
        Typeset::Selection c(left, right);

        return parse_tree.addNode<3>(OP_IF_ELSE, c, {condition, body, el});
    }else{
        Typeset::Marker right = rMark();
        Typeset::Selection c(left, right);

        return parse_tree.addNode<2>(OP_IF, c, {condition, body});
    }
}

ParseNode Parser::whileStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker l_cond = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);
    ParseNode condition = disjunction();
    Typeset::Marker r_cond = rMark();
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(l_cond, r_cond);
    loops++;
    ParseNode body = blockStatement();
    loops--;
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);

    return parse_tree.addNode<2>(OP_WHILE, c, {condition, body});
}

ParseNode Parser::forStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker l_cond = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);
    ParseNode initializer = peek(SEMICOLON) ?
                            makeTerminalNode(OP_BLOCK) :
                            statement();

    if(match(COLON)) return rangedFor(left, l_cond, initializer);
    else consume(SEMICOLON);
    ParseNode condition = peek(SEMICOLON) ?
                          makeTerminalNode(OP_TRUE) :
                          disjunction();
    consume(SEMICOLON);
    ParseNode update = peek(RIGHTPAREN) ?
                       makeTerminalNode(OP_BLOCK) :
                       statement();
    Typeset::Marker r_cond = rMark();
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(l_cond, r_cond);
    loops++;
    ParseNode body = blockStatement();
    loops--;
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);

    return parse_tree.addNode<4>(OP_FOR, c, {initializer, condition, update, body});
}

ParseNode Parser::rangedFor(Typeset::Marker stmt_left, Typeset::Marker paren_left, ParseNode initialiser) alloc_except {
    if(parse_tree.getOp(initialiser) != OP_EXPR_STMT) return error(BAD_RANGED_FOR_VAR, parse_tree.getSelection(initialiser));
    initialiser = parse_tree.child(initialiser);
    if(parse_tree.getOp(initialiser) != OP_IDENTIFIER) return error(BAD_RANGED_FOR_VAR, parse_tree.getSelection(initialiser));

    ParseNode collection = expression();
    Typeset::Marker paren_right = rMark();
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);

    registerGrouping(paren_left, paren_right);
    loops++;
    ParseNode body = blockStatement();
    loops--;
    Typeset::Marker stmt_right = rMark();
    Typeset::Selection c(stmt_left, stmt_right);

    return parse_tree.addNode<3>(OP_RANGED_FOR, c, {initialiser, collection, body});
}

ParseNode Parser::enumStatement() alloc_except {
    Typeset::Marker start;
    advance();
    ParseNode id = isolatedIdentifier();
    Typeset::Marker lmark = lMark();
    if(!match(LEFTBRACKET)) return error(EXPECT_LBRACKET);

    parse_tree.prepareNary();
    parse_tree.addNaryChild(id);

    while(match(NEWLINE));
    while(!match(RIGHTBRACKET) && noErrors()){
        parse_tree.addNaryChild(isolatedIdentifier());
        match(COMMA); //EVENTUALLY: is this necessary?
        while(match(NEWLINE));
    }

    Typeset::Marker end = rMarkPrev();

    if(noErrors()){
        registerGrouping(lmark, end);
        return parse_tree.finishNary(OP_ENUM, Typeset::Selection(start, end));
    }else{
        parse_tree.cancelNary();
        return error(EXPECT_CASE);
    }

    //EVENTUALLY:
    // it's fairly trivial to support enums downstream as numbers,
    // but you'll get better mileage to support them as enums
    // enums also have a scoped access, e.g. COLOUR::RED
}

ParseNode Parser::settingsStatement() noexcept {
    Typeset::Construct* c = lMark().text->nextConstructAsserted();
    ParseNode pn = terminalAndAdvance(OP_SETTINGS_UPDATE);
    parse_tree.setFlag(pn, reinterpret_cast<size_t>(c));
    #ifndef FORSCAPE_TYPESET_HEADLESS
    c->pn = pn;
    #endif

    return pn;
}

ParseNode Parser::switchStatement() alloc_except {
    Typeset::Marker start;
    advance();
    Typeset::Marker cond_l = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);
    ParseNode switch_key = disjunction();
    Typeset::Marker cond_r = rMark();
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(cond_l, cond_r);
    cond_l = lMark();
    if(!match(LEFTBRACKET)) return error(EXPECT_LBRACKET);
    parse_tree.prepareNary();
    parse_tree.addNaryChild(switch_key);
    while(!match(RIGHTBRACKET) && noErrors()){
        switch (currentType()) {
            case CASE:{
                advance();
                ParseNode case_key = disjunction();
                consume(COLON);
                match(NEWLINE);
                if(peek(CASE) || peek(DEFAULT) || peek(RIGHTBRACKET)){
                    ParseNode case_node = parse_tree.addNode<2>(OP_CASE, parse_tree.getSelection(case_key), {case_key, NONE});
                    parse_tree.addNaryChild(case_node);
                }else{
                    ParseNode case_codepath = blockStatement();
                    ParseNode case_node = parse_tree.addNode<2>(OP_CASE, {case_key, case_codepath});
                    parse_tree.addNaryChild(case_node);
                }
                break;
            }

            case DEFAULT:{
                ParseNode default_label = terminalAndAdvance(OP_DEFAULT);
                consume(COLON);
                match(NEWLINE);
                if(peek(CASE) || peek(DEFAULT) || peek(RIGHTBRACKET)){
                    ParseNode case_node = parse_tree.addNode<2>(OP_DEFAULT, parse_tree.getSelection(default_label), {default_label, NONE});
                    parse_tree.addNaryChild(case_node);
                }else{
                    ParseNode default_codepath = blockStatement();
                    ParseNode default_node = parse_tree.addNode<2>(OP_DEFAULT, {default_label, default_codepath});
                    parse_tree.addNaryChild(default_node);
                }
                break;
            }

            case NEWLINE:
            case COMMENT:
                advance();
                break;

            default:
                parse_tree.cancelNary();
                return error(EXPECT_CASE);
        }
    }

    cond_r = rMarkPrev();

    if(noErrors()){
        registerGrouping(cond_l, cond_r);
        return parse_tree.finishNary(OP_SWITCH, Typeset::Selection(start, cond_r));
    }else{
        parse_tree.cancelNary();
        return error(EXPECT_CASE);
    }
}

ParseNode Parser::printStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker l_group = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);

    parse_tree.prepareNary();

    do{
        parse_tree.addNaryChild(disjunction());
    } while(noErrors() && match(COMMA));

    Typeset::Marker right = rMark();
    Typeset::Selection sel(left, right);
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(l_group, right);

    return parse_tree.finishNary(OP_PRINT, sel);
}

ParseNode Parser::assertStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker l_group = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);
    ParseNode e = disjunction();

    Typeset::Marker right = rMark();
    Typeset::Selection sel(left, right);
    if(!match(RIGHTPAREN)) return error(EXPECT_RPAREN);
    registerGrouping(l_group, right);

    return parse_tree.addUnary(OP_ASSERT, sel, e);
}

ParseNode Parser::blockStatement() alloc_except {
    match(NEWLINE);
    if(!peek(LEFTBRACKET)){
        ParseNode pn = statement();
        match(NEWLINE);
        return pn;
    }

    Typeset::Marker left = lMark();
    CONSUME_ASSERT(LEFTBRACKET);
    parse_tree.prepareNary();

    skipNewlines();
    while(noErrors() && !match(RIGHTBRACKET)){
        parse_tree.addNaryChild(statement());
        skipNewlines();
    }

    Typeset::Selection sel(left, rMarkPrev());
    if(noErrors()) registerGrouping(sel);

    return parse_tree.finishNary(OP_BLOCK, sel);
}

ParseNode Parser::lexicalScopeStatement() alloc_except {
    Typeset::Marker left = lMark();
    CONSUME_ASSERT(LEFTBRACKET);
    parse_tree.prepareNary();

    skipNewlines();
    while(noErrors() && !match(RIGHTBRACKET)){
        parse_tree.addNaryChild(statement());
        skipNewlines();
    }

    Typeset::Selection sel(left, rMarkPrev());
    if(noErrors()) registerGrouping(sel);

    return parse_tree.finishNary(OP_LEXICAL_SCOPE, sel);
}

ParseNode Parser::algStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();

    ParseNode id = isolatedIdentifier();
    if(parse_tree.getOp(id) == OP_ERROR) return id;
    if(comment != NONE) parse_tree.setFlag(id, parse_tree.addTerminal(OP_COMMENT, tokens[comment].sel));

    if(!peek(LEFTPAREN) && !peek(LEFTBRACE))
        return parse_tree.addUnary(OP_PROTOTYPE_ALG, id);

    ParseNode val_captures = match(LEFTBRACE) ? captureList() : NONE;

    ParseNode ref_upvalues = NONE;

    consume(LEFTPAREN);
    ParseNode params = paramList();

    size_t loops_backup = 0;
    std::swap(loops_backup, loops);
    ParseNode body = blockStatement();
    std::swap(loops_backup, loops);
    Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addNode<5>(OP_ALGORITHM, sel, {val_captures, ref_upvalues, params, body, id});
}

ParseNode Parser::returnStatement() alloc_except {
    Typeset::Marker m = lMark();
    advance();
    return parse_tree.addLeftUnary(OP_RETURN, m, disjunction());
}

ParseNode Parser::plotStatement() alloc_except {
    Typeset::Marker m = lMark();
    advance();
    Typeset::Marker l = lMark();
    if(!match(LEFTPAREN)) return error(CONSUME);
    ParseNode title = expression();
    if(!match(COMMA)) return error(CONSUME);
    ParseNode x_label = expression();
    if(!match(COMMA)) return error(CONSUME);
    ParseNode x = expression();
    if(!match(COMMA)) return error(CONSUME);
    ParseNode y_label = expression();
    if(!match(COMMA)) return error(CONSUME);
    ParseNode y = expression();
    Typeset::Marker r = rMark();
    if(!match(RIGHTPAREN)) return error(CONSUME);
    Typeset::Selection sel(m, r);
    registerGrouping(l, r);

    return parse_tree.addNode<5>(OP_PLOT, sel, {title, x_label, x, y_label, y});
}

ParseNode Parser::importStatement() noexcept {
    const Typeset::Marker& left = lMark();
    advance();

    ParseNode file = filename();
    ParseNode alias = match(AS) ? isolatedIdentifier() : NONE;
    ParseNode import_stmt = parse_tree.addUnary(OP_IMPORT, Typeset::Selection(left, rMarkPrev()), file);
    parse_tree.setFlag(import_stmt, alias);

    return import_stmt;
}

ParseNode Parser::fromStatement() noexcept {
    const Typeset::Marker& left = lMark();
    advance();

    ParseNode file = filename();

    if(!match(IMPORT)) return error(UNRECOGNIZED_EXPR);

    parse_tree.prepareNary();
    parse_tree.addNaryChild(file);
    do {
        if(!peek(IDENTIFIER)){
            Typeset::Marker m = rMarkPrev();
            if(!m.atTextEnd()) m.incrementToNextWord();
            parse_tree.addNaryChild(parse_tree.addTerminal(OP_IDENTIFIER, Typeset::Selection(m, m)));
            parse_tree.addNaryChild(NONE);
            return parse_tree.finishNary(OP_FROM_IMPORT, Typeset::Selection(left, rMarkPrev()));
        }

        ParseNode component = isolatedIdentifier();
        ParseNode alias = match(AS) ? isolatedIdentifier() : NONE;
        parse_tree.addNaryChild(component);
        parse_tree.addNaryChild(alias);
    } while(match(COMMA));

    return parse_tree.finishNary(OP_FROM_IMPORT, Typeset::Selection(left, rMarkPrev()));
}

ParseNode Parser::filename() noexcept {
    if(!peek(FILEPATH)) return PARSE_ERROR;
    const Typeset::Selection& sel = selection();
    ParseNode file = terminalAndAdvance(OP_FILE_REF);

    std::string_view path = parse_tree.getSelection(file).strView();
    Program::ptr_or_code ptr_or_code = Program::instance()->openFromRelativePath(path, model->path.parent_path());

    switch (ptr_or_code) {
        case Program::FILE_NOT_FOUND: return error(FILE_NOT_FOUND, sel);
        case Program::FILE_CORRUPTED: return error(FILE_CORRUPTED, sel);
        default:
            if(reinterpret_cast<Typeset::Model*>(ptr_or_code) == model)
                return error(SELF_IMPORT, sel);
            parse_tree.setFlag(file, ptr_or_code);
            registerParseNodeRegion(file, index-1);
    }

    return file;
}

ParseNode Parser::namespaceStatement() noexcept {
    const Typeset::Marker& left = lMark();

    advance();
    ParseNode name = isolatedIdentifier();
    ParseNode body = blockStatement();

    return parse_tree.addNode<2>(OP_NAMESPACE, Typeset::Selection(left, rMarkPrev()), {name, body});
}

ParseNode Parser::unknownsStatement() alloc_except {
    const Typeset::Marker left = lMark();
    advance();
    consume(COLON);
    parse_tree.prepareNary();
    parse_tree.addNaryChild(isolatedIdentifier());
    while(match(COMMA)){
        parse_tree.addNaryChild(isolatedIdentifier());
    }

    if(!noErrors()) return PARSE_ERROR;

    ParseNode unknown_list = parse_tree.finishNary(OP_UNKNOWN_LIST, Typeset::Selection(left, rMarkPrev()));

    if(match(MEMBER)){
        parse_tree.setFlag(unknown_list, expression());
        parse_tree.setRight(unknown_list, rMarkPrev());
    }

    return unknown_list;
}

ParseNode Parser::classStatement() noexcept {
    const Typeset::Marker& left = lMark();

    advance();
    ParseNode name = isolatedIdentifier();
    ParseNode parents = NONE;

    if(match(COLON)){
        parse_tree.prepareNary();

        do {
            bool is_private = false;
            if(match(PRIVATE)) is_private = true;
            else match(PUBLIC);

            ParseNode parent = isolatedIdentifier();
            parse_tree.setFlag(parent, is_private);

            parse_tree.addNaryChild(parent);
        } while(match(COMMA));

        parents = parse_tree.finishNary(OP_LIST);
    }

    skipNewline();
    Typeset::Marker bracket_left = lMark();
    consume(LEFTBRACKET);
    skipNewlines();

    parse_tree.prepareNary();
    while(noErrors() && !match(RIGHTBRACKET)){
        bool is_static = match(STATIC);
        ParseNode member = isolatedIdentifier();
        parse_tree.setFlag(member, is_static);
        match(COMMA);
        skipNewline();
    }
    if(!noErrors()) return PARSE_ERROR;

    Typeset::Selection sel(bracket_left, rMarkPrev());
    registerGrouping(sel);
    ParseNode member_list = parse_tree.finishNary(OP_LIST, sel);

    return parse_tree.addNode<3>(OP_CLASS, Typeset::Selection(left, rMarkPrev()), {name, parents, member_list});
}

ParseNode Parser::mathStatement() alloc_except {
    ParseNode n = expression();

    switch (currentType()) {
        case DEFEQUALS:
        case EQUALS:
        case EQUIVALENT:
            n = (parse_tree.getOp(n) == OP_CALL && parse_tree.getNumArgs(n) >= 2) ?
                        namedLambdaStmt(n) :
                        equality(n);
            break;
        case LEFTARROW: n = assignment(n); break;
        default: n = parse_tree.addUnary(OP_EXPR_STMT, n);
    }

    switch (currentType()) {
        case NEWLINE:
        case COMMENT:
            advance();
            return n;

        case ARGCLOSE:
        case COLON:
        case SEMICOLON:
        case RIGHTBRACE:
        case RIGHTPAREN:
        case ENDOFFILE:
            return n;

        default:
            return error(UNRECOGNIZED_EXPR);
    }
}

ParseNode Parser::namedLambdaStmt(ParseNode call) alloc_except {
    assert(parse_tree.getOp(call) == OP_CALL);
    advance();

    ParseNode expr = expression();
    if(!noErrors()) return PARSE_ERROR;
    ParseNode body = parse_tree.addUnary(OP_RETURN, expr);

    ParseNode id = parse_tree.arg<0>(call);
    if(parse_tree.getOp(id) == OP_SUBSCRIPT_ACCESS){
        parse_tree.setOp(id, OP_IDENTIFIER);

        #ifndef FORSCAPE_TYPESET_HEADLESS
        parse_tree.getLeft(parse_tree.lhs(id)).text->retagParseNodeLast(id);

        ParseNode sub = parse_tree.rhs(id);
        Typeset::Text* t = parse_tree.getLeft(sub).text;
        if(parse_tree.getOp(sub) == OP_INTEGER_LITERAL)
            t->tagParseNode(id, 0, t->numChars());
        else
            t->retagParseNode(id, 0);
        #endif
    }

    ParseNode val_captures = NONE;
    ParseNode ref_upvalues = NONE;

    const size_t nargs = parse_tree.getNumArgs(call)-1;
    assert(nargs >= 1);

    //Repurpose the call
    ParseNode params = call;
    for(size_t i = 0; i < nargs; i++){
        ParseNode param = parse_tree.arg(call, i+1);
        Op op = parse_tree.getOp(param);
        if(op == OP_IDENTIFIER || (op == OP_EQUAL && parse_tree.getOp(parse_tree.lhs(param)) == OP_IDENTIFIER))
            parse_tree.setArg(params, i, parse_tree.arg(call, i+1));
        else
            parse_tree.setArg(params, i, error(INVALID_PARAMETER, parse_tree.getSelection(param)));
    }
    parse_tree.reduceNumArgs(params, nargs);
    parse_tree.setOp(params, OP_LIST);
    parse_tree.setLeft(params, parse_tree.getLeft(parse_tree.arg<0>(params)));
    parse_tree.setRight(params, parse_tree.getRight(parse_tree.arg(params, nargs-1)));

    Typeset::Selection sel(parse_tree.getLeft(id), parse_tree.getRight(body));
    return parse_tree.addNode<5>(OP_ALGORITHM, sel, {val_captures, ref_upvalues, params, body, id});
}

ParseNode Parser::assignment(ParseNode lhs) alloc_except {
    startPatch();
    registerParseNodeRegionToPatch(index);
    advance();
    ParseNode pn = parse_tree.addNode<2>(OP_ASSIGN, {lhs, expression()});
    finishPatch(pn);
    if(match(COMMENT)) parse_tree.setFlag(lhs, parse_tree.addTerminal(OP_COMMENT, selectionPrev()));
    else if(comment != NONE) parse_tree.setFlag(lhs, parse_tree.addTerminal(OP_COMMENT, tokens[comment].sel));
    return pn;
}

ParseNode Parser::expression() noexcept{
    return addition();
}

ParseNode Parser::equality(ParseNode lhs) alloc_except {
    parse_tree.prepareNary();
    parse_tree.addNaryChild(lhs);

    do {
        advance();
        parse_tree.addNaryChild(expression());
    } while(peek(EQUALS));

    ParseNode pn = parse_tree.finishNary(OP_EQUAL);
    if(match(COMMENT)) parse_tree.setFlag(lhs, parse_tree.addTerminal(OP_COMMENT, selectionPrev()));
    else if(comment != NONE) parse_tree.setFlag(lhs, parse_tree.addTerminal(OP_COMMENT, tokens[comment].sel));
    return pn;
}

ParseNode Parser::disjunction() alloc_except {
    ParseNode n = conjunction();

    for(;;){
        switch (currentType()) {
            case DISJUNCTION: advance(); n = parse_tree.addNode<2>(OP_LOGICAL_OR, {n, conjunction()}); break;
            default: return n;
        }
    }
}

ParseNode Parser::conjunction() alloc_except {
    ParseNode n = comparison();

    for(;;){
        switch (currentType()) {
            case CONJUNCTION: advance(); n = parse_tree.addNode<2>(OP_LOGICAL_AND, {n, comparison()}); break;
            default: return n;
        }
    }
}

ParseNode Parser::comparison() alloc_except {
    ParseNode n = addition();

    switch (currentType()) {
        case EQUALS: advance(); return parse_tree.addNode<2>(OP_EQUAL, {n, addition()});
        case NOTEQUAL: advance(); return parse_tree.addNode<2>(OP_NOT_EQUAL, {n, addition()});
        case APPROX: advance(); return parse_tree.addNode<2>(OP_APPROX, {n, addition()});
        case NOTAPPROX: advance(); return parse_tree.addNode<2>(OP_NOT_APPROX, {n, addition()});
        case MEMBER: advance(); return parse_tree.addNode<2>(OP_IN, {n, addition()});
        case NOTIN: advance(); return parse_tree.addNode<2>(OP_NOT_MEMBER, {n, addition()});
        case SUBSET: advance(); return parse_tree.addNode<2>(OP_SUBSET, {n, addition()});
        case SUBSETEQ: advance(); return parse_tree.addNode<2>(OP_SUBSET_EQ, {n, addition()});
        case LESS: return less(n, 0);
        case LESSEQUAL: return less(n, 1);
        case GREATER: return greater(n, 0);
        case GREATEREQUAL: return greater(n, 1);
        default: return n;
    }
}

ParseNode Parser::less(ParseNode first, size_t flag) alloc_except {
    startPatch();
    registerParseNodeRegionToPatch(index);
    advance();

    parse_tree.prepareNary();
    parse_tree.addNaryChild(first);
    parse_tree.addNaryChild(addition());
    size_t comparisons = 1;

    for(;;){
        if(match(LESSEQUAL)){
            flag |= (size_t(1) << comparisons);
        }else if(!match(LESS)){
            ParseNode pn = parse_tree.finishNary(OP_LESS);
            finishPatch(pn);
            parse_tree.setFlag(pn, flag);
            return pn;
        }

        registerParseNodeRegionToPatch(index-1);
        comparisons++;
        assert(comparisons < sizeof(size_t)*8); //EVENTUALLY: report an error to the user

        parse_tree.addNaryChild(addition());
    }
}

ParseNode Parser::greater(ParseNode first, size_t flag) alloc_except {
    advance();
    parse_tree.prepareNary();
    parse_tree.addNaryChild(first);
    parse_tree.addNaryChild(addition());
    size_t comparisons = 1;

    for(;;){
        if(match(GREATEREQUAL)){
            flag |= (size_t(1) << comparisons);
        }else if(!match(GREATER)){
            ParseNode pn = parse_tree.finishNary(OP_GREATER);
            parse_tree.setFlag(pn, flag);
            return pn;
        }

        comparisons++;
        assert(comparisons < sizeof(size_t)*8); //EVENTUALLY: report an error to the user

        parse_tree.addNaryChild(addition());
    }
}

ParseNode Parser::addition() alloc_except {
    ParseNode n = multiplication();

    for(;;){
        switch (currentType()) {
            case PLUS: advance(); n = parse_tree.addNode<2>(OP_ADDITION, {n, multiplication()}); break;
            case MINUS: advance(); n = parse_tree.addNode<2>(OP_SUBTRACTION, {n, multiplication()}); break;
            case CUP: advance(); n = parse_tree.addNode<2>(OP_UNION, {n, multiplication()}); break;
            case CAP: advance(); n = parse_tree.addNode<2>(OP_INTERSECTION, {n, multiplication()}); break;
            default: return n;
        }
    }
}

ParseNode Parser::multiplication() alloc_except {
    ParseNode n = leftUnary();

    for(;;){
        switch (currentType()) {
            case MULTIPLY: advance(); n = parse_tree.addNode<2>(OP_MULTIPLICATION, {n, leftUnary()}); break;
            case DIVIDE: advance(); n = parse_tree.addNode<2>(OP_DIVIDE, {n, leftUnary()}); break;
            case FORWARDSLASH: advance(); n = parse_tree.addNode<2>(OP_FORWARDSLASH, {n, leftUnary()}); break;
            case BACKSLASH: advance(); n = parse_tree.addNode<2>(OP_BACKSLASH, {n, leftUnary()}); break;
            case TIMES: if(parsing_dims) return n; advance(); n = parse_tree.addNode<2>(OP_CROSS, {n, leftUnary()}); break;
            case DOTPRODUCT: advance(); n = parse_tree.addNode<2>(OP_DOT, {n, leftUnary()}); break;
            case PERCENT: advance(); n = parse_tree.addNode<2>(OP_MODULUS, {n, leftUnary()}); break;
            case OUTERPRODUCT: advance(); n = parse_tree.addNode<2>(OP_OUTER_PRODUCT, {n, leftUnary()}); break;
            case ODOT: advance(); n = parse_tree.addNode<2>(OP_ODOT, {n, leftUnary()}); break;
            case OCIRC: advance(); n = parse_tree.addNode<2>(OP_COMPOSITION, {n, leftUnary()}); break;
            default: return n;
        }
    }
}

ParseNode Parser::leftUnary() alloc_except {
    switch (currentType()) {
        case MINUS:{
            Typeset::Marker m = lMark();
            advance();
            return parse_tree.addLeftUnary(OP_UNARY_MINUS, m, implicitMult());
        }
        case NOT:{
            Typeset::Marker m = lMark();
            advance();
            return parse_tree.addLeftUnary(OP_LOGICAL_NOT, m, implicitMult());
        }
        case POUND:{
            Typeset::Marker m = lMark();
            advance();
            return parse_tree.addLeftUnary(OP_CARDINALITY, m, implicitMult());
        }
        case NABLA:{
            Typeset::Marker m = lMark();
            advance();
            switch (currentType()) {
                case DOTPRODUCT: advance(); return parse_tree.addLeftUnary(OP_DIVERGENCE, m, implicitMult());
                case TIMES: advance(); return parse_tree.addLeftUnary(OP_CURL, m, implicitMult());
                default: return parse_tree.addLeftUnary(OP_GRADIENT, m, implicitMult());
            }
        }
        default: return implicitMult();
    }
}

ParseNode Parser::implicitMult() alloc_except {
    ParseNode n = rightUnary();

    switch (currentType()) {
        FORSCAPE_IMPLICIT_MULT_TOKENS
        case LEFTPAREN:{
            ParseNode pn = collectImplicitMult(n);
            return parse_tree.getNumArgs(pn) != 1 ? pn : parse_tree.child(pn);
        }
        case INTEGER: return error(TRAILING_CONSTANT);
        default: return n;
    }
}

ParseNode Parser::collectImplicitMult(ParseNode n) alloc_except {
    parse_tree.prepareNary();
    parse_tree.addNaryChild(n);

    for(;;){
        if(!noErrors()){
            parse_tree.cancelNary();
            return PARSE_ERROR;
        }

        switch (currentType()) {
            FORSCAPE_IMPLICIT_MULT_TOKENS
                parse_tree.addNaryChild(rightUnary());
                break;
            case LEFTPAREN:
                parse_tree.addNaryChild( call_or_mult(parse_tree.popNaryChild()) );
                break;
            case INTEGER:
                return error(TRAILING_CONSTANT);
            default:
                return parse_tree.finishNary(OP_IMPLICIT_MULTIPLY);
        }
    }
}

ParseNode Parser::call_or_mult(ParseNode n) alloc_except {
    const Typeset::Marker& left = lMark();
    advance();

    if(match(RIGHTPAREN)){
        const Typeset::Marker& right = rMarkPrev();
        registerGrouping(left, right);
        return parse_tree.addRightUnary(OP_CALL, right, n);
    }

    ParseNode parenthetical = disjunction();
    if(match(RIGHTPAREN)){
        const Typeset::Marker& right = rMarkPrev();
        registerGrouping(left, right);

        ParseNode post_high_prec = rightUnary(parenthetical);
        Op op = (post_high_prec == parenthetical) ? OP_CALL : OP_AMBIGUOUS_PARENTHETICAL;

        Typeset::Selection sel(parse_tree.getLeft(n), right);
        return parse_tree.addNode<2>(op, sel, {n, post_high_prec});
    }

    parse_tree.prepareNary();
    parse_tree.addNaryChild(n);
    parse_tree.addNaryChild(parenthetical);

    while(!match(RIGHTPAREN) && noErrors()){
        consume(COMMA);
        parse_tree.addNaryChild(disjunction());
    }

    if(!noErrors()){
        //EVENTUALLY:
        //  Check if the user is typing here
        //  If so, display a tooltip with the function parameters
        //  This depends on doing the work to resolve the called function, despite errors

        return PARSE_ERROR;
    }

    const Typeset::Marker& right = rMarkPrev();
    registerGrouping(left, right);

    return parse_tree.finishNary(OP_CALL, Typeset::Selection(parse_tree.getLeft(n), right));
}

ParseNode Parser::rightUnary() alloc_except {
    return rightUnary(primary());
}

ParseNode Parser::rightUnary(ParseNode n) alloc_except {
    for(;;){
        switch (currentType()) {
            case EXCLAM:{
                Typeset::Marker m = rMark();
                advance();
                ParseNode pn = parse_tree.addRightUnary(OP_FACTORIAL, m, n);
                registerParseNodeRegion(pn, index-1);
                return pn;
            }
            case CARET:{
                advance();
                ParseNode pn = parse_tree.addNode<2>(OP_POWER, {n, implicitMult()});
                return pn;
            }
            case TOKEN_SUPERSCRIPT: n = superscript(n); break;
            case TOKEN_SUBSCRIPT: n = subscript(n, rMark()); break;
            case TOKEN_DUALSCRIPT: n = dualscript(n); break;
            case PERIOD: advance();
                if(!peek(IDENTIFIER)){
                    const Typeset::Marker& m = rMarkPrev();
                    ParseNode blank = parse_tree.addTerminal(OP_ERROR, Typeset::Selection(m, m));
                    n = parse_tree.addNode<2>(OP_SCOPE_ACCESS, {n, blank}); break;
                }
                if(!noErrors()) return n;
                n = parse_tree.addNode<2>(OP_SCOPE_ACCESS, {n, primary()}); break;
            default: return n;
        }
    }
}

ParseNode Parser::primary() alloc_except {
    switch (currentType()) {
        case INTEGER: return integer();
        case IDENTIFIER: return identifier();
        case STRING: return string();
        case DOUBLESTRUCK_R: return setWithSigns<OP_REALS, OP_POSITIVE_REALS, OP_NEGATIVE_REALS>();
        case DOUBLESTRUCK_Q: return setWithSigns<OP_RATIONALS, OP_POSITIVE_RATIONALS, OP_NEGATIVE_RATIONALS>();
        case DOUBLESTRUCK_Z: return generalSet(OP_INTEGERS);
        case DOUBLESTRUCK_N: return generalSet(OP_NATURALS);
        case DOUBLESTRUCK_C: return generalSet(OP_COMPLEX_NUMS);
        case DOUBLESTRUCK_B: return generalSet(OP_BOOLEANS);
        case DOUBLESTRUCK_P: return generalSet(OP_PRIMES);
        case DOUBLESTRUCK_H: advance(); return parse_tree.addTerminal(OP_QUATERNIONS, selectionPrev());
        case SPECIALORTHOGONAL: return oneArgRequireParen(OP_SPECIAL_ORTHOGONAL);

        //Value literal
        case INFTY: return terminalAndAdvance(OP_INFTY);
        case EMPTYSET: return terminalAndAdvance(OP_EMPTY_SET);
        case TRUELITERAL: return terminalAndAdvance(OP_TRUE);
        case FALSELITERAL: return terminalAndAdvance(OP_FALSE);
        case GRAVITY: return terminalAndAdvance(OP_GRAVITY);
        case UNDEFINED: return terminalAndAdvance(OP_UNDEFINED);

        //EVENTUALLY: how do units work?
        //case DEGREE: return terminalAndAdvance(OP_DEGREES);
        case POUNDSTERLING: return terminalAndAdvance(OP_CURRENCY_POUNDS);
        case EURO: return terminalAndAdvance(OP_CURRENCY_EUROS);
        case DOLLAR: return terminalAndAdvance(OP_CURRENCY_DOLLARS);

        //Grouping
        case LEFTPAREN: return parenGrouping();
        case LEFTBRACE: return braceGrouping();
        case LEFTCEIL: return grouping(OP_CEIL, RIGHTCEIL);
        case LEFTFLOOR: return grouping(OP_FLOOR, RIGHTFLOOR);
        case BAR: return grouping(OP_ABS, BAR);
        case DOUBLEBAR: return norm();
        case LEFTANGLE: return innerProduct();
        case LEFTBRACKET: return set();
        case LEFTDOUBLEBRACE: return integerRange();

        //Constructs
        case TOKEN_FRACTION: return fraction();
        case TOKEN_BINOMIAL: return binomial();
        case TOKEN_MATRIX: return matrix();
        case TOKEN_CASES: return cases();
        case TOKEN_SQRT: return squareRoot();
        case TOKEN_NRT: return nRoot();
        case TOKEN_LIMIT: return limit();
        case TOKEN_INTEGRAL0: return indefiniteIntegral();
        case TOKEN_INTEGRAL2: return definiteIntegral();

        case TOKEN_BIGSUM0:
        case TOKEN_BIGSUM1:
        case TOKEN_BIGPROD0:
        case TOKEN_BIGPROD1:
        case TOKEN_BIGCOPROD0:
        case TOKEN_BIGCOPROD1:
            return error(EMPTY_BIG_SYMBOL);

        case TOKEN_BIGSUM2: return big(OP_SUMMATION);
        case TOKEN_BIGPROD2: return big(OP_PRODUCT);

        case TOKEN_ACCENTHAT: return oneArgConstruct(OP_ACCENT_HAT);
        case TOKEN_ACCENTBAR: return oneArgConstruct(OP_ACCENT_BAR);

        //Keyword funcs
        case SGN: return oneArg(OP_SIGN_FUNCTION);
        case LENGTH: return oneArg(OP_LENGTH);
        case ROWS: return oneArg(OP_ROWS);
        case COLS: return oneArg(OP_COLS);
        case SIN: return trig(OP_SINE);
        case COS: return trig(OP_COSINE);
        case TAN: return trig(OP_TANGENT);
        case ARCSIN: return trig(OP_ARCSINE);
        case ARCCOS: return trig(OP_ARCCOSINE);
        case ARCTAN: return trig(OP_ARCTANGENT);
        case ARCTAN2: return twoArgs(OP_ARCTANGENT2);
        case CSC: return trig(OP_COSECANT);
        case SEC: return trig(OP_SECANT);
        case COT: return trig(OP_COTANGENT);
        case ARCCSC: return trig(OP_ARCCOSECANT);
        case ARCSEC: return trig(OP_ARCSECANT);
        case ARCCOT: return trig(OP_ARCCOTANGENT);
        case SINH: return trig(OP_HYPERBOLIC_SINE);
        case COSH: return trig(OP_HYPERBOLIC_COSINE);
        case TANH: return trig(OP_HYPERBOLIC_TANGENT);
        case ARCSINH: return trig(OP_HYPERBOLIC_ARCSINE);
        case ARCCOSH: return trig(OP_HYPERBOLIC_ARCCOSINE);
        case ARCTANH: return trig(OP_HYPERBOLIC_ARCTANGENT);
        case CSCH: return trig(OP_HYPERBOLIC_COSECANT);
        case SECH: return trig(OP_HYPERBOLIC_SECANT);
        case COTH: return trig(OP_HYPERBOLIC_COTANGENT);
        case ARCCSCH: return trig(OP_HYPERBOLIC_ARCCOSECANT);
        case ARCSECH: return trig(OP_HYPERBOLIC_ARCSECANT);
        case ARCCOTH: return trig(OP_HYPERBOLIC_ARCCOTANGENT);
        case EXP: return oneArg(OP_EXP);
        case NATURALLOG: return oneArg(OP_NATURAL_LOG);
        case LOG: return log();
        case ERRORFUNCTION: return oneArg(OP_ERROR_FUNCTION);
        case COMPERRFUNC: return oneArg(OP_COMP_ERR_FUNC);

        case ARGCLOSE: return error(EXPECTED_PRIMARY, Typeset::Selection(lMark(), lMark()));

        default:
            return error(EXPECTED_PRIMARY);
    }
}

ParseNode Parser::string() noexcept{
    ParseNode pn = terminalAndAdvance(OP_STRING);
    return pn;
}

ParseNode Parser::braceGrouping() noexcept {
    Typeset::Marker left = lMark();
    advance();
    if(peek(RIGHTBRACE)){
        //It's an empty list
        Typeset::Marker right = rMark();
        Typeset::Selection sel(left, right);
        registerGrouping(sel);
        advance();
        return parse_tree.addTerminal(OP_LIST, sel);
    }

    ParseNode nested = disjunction();
    if(peek(RIGHTBRACE)){
        //Bracket expression
        Typeset::Marker right = rMark();
        Typeset::Selection sel(left, right);
        registerGrouping(sel);
        advance();
        return parse_tree.addUnary(OP_GROUP_BRACKET, sel, nested);
    }

    consume(COMMA);
    if(!noErrors()) return PARSE_ERROR;

    ParseNode end = disjunction();

    //It's a list
    if(match(COMMA)){
        parse_tree.prepareNary();
        parse_tree.addNaryChild(nested);
        parse_tree.addNaryChild(end);
        do { parse_tree.addNaryChild(disjunction()); } while(match(COMMA));

        Typeset::Marker right = rMark();
        Typeset::Selection sel(left, right);
        consume(RIGHTBRACE);
        if(noErrors()) registerGrouping(sel);
        return parse_tree.finishNary(OP_LIST, sel);
    }else{
        //It's an interval
        Typeset::Marker right = rMark();

        Op op = OP_INTERVAL_CLOSE_CLOSE;
        if(match(RIGHTPAREN)) op = OP_INTERVAL_CLOSE_OPEN;
        else consume(RIGHTBRACE);

        if(!noErrors()) return PARSE_ERROR;

        Typeset::Selection sel(left, right);
        registerGrouping(sel);

        return parse_tree.addNode<2>(op, sel, {nested, end});
    }
}

ParseNode Parser::parenGrouping() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    match(NEWLINE);
    ParseNode nested = disjunction();
    if(peek(RIGHTPAREN)){
        Typeset::Marker right = rMark();
        Typeset::Selection sel(left, right);
        registerGrouping(sel);
        advance();
        return peek(MAPSTO) ?
                lambda(parse_tree.addUnary(OP_LIST, sel, nested)) :
                parse_tree.addUnary(OP_GROUP_PAREN, sel, nested);
    }

    parse_tree.prepareNary();
    parse_tree.addNaryChild(nested);
    do{
        consume(COMMA);
        if(!noErrors()) break;
        match(NEWLINE);
        parse_tree.addNaryChild(disjunction());
        match(NEWLINE);

        if(match(RIGHTBRACE)){
            Typeset::Selection sel(left, rMarkPrev());
            registerGrouping(sel);
            ParseNode list = parse_tree.finishNary(OP_INTERVAL_OPEN_CLOSE, sel);
            return parse_tree.getNumArgs(list) == 2 ? list : error(UNRECOGNIZED_SYMBOL, tokens[index-1].sel);
        }
    } while(!peek(RIGHTPAREN) && noErrors());
    Typeset::Selection sel(left, rMark());
    if(noErrors()){
        registerGrouping(sel);
        advance();
    }

    ParseNode list = parse_tree.finishNary(OP_LIST, sel);

    return peek(MAPSTO) ? lambda(list) : list;
}

ParseNode Parser::paramList() alloc_except {
    Typeset::Marker left = lMarkPrev();

    if(peek(RIGHTPAREN)){
        Typeset::Selection sel(left, rMark());
        registerGrouping(sel);
        advance();
        return parse_tree.addTerminal(OP_LIST, sel);
    }

    match(NEWLINE);

    parse_tree.prepareNary();
    ParseNode p = param();
    parse_tree.addNaryChild(p);
    while(match(COMMA) && noErrors()){
        if(peek(COMMENT)){
            ParseNode comment = parse_tree.addTerminal(OP_COMMENT, selection());
            parse_tree.setFlag(p, comment);
            advance();
            consume(NEWLINE);
        }
        match(NEWLINE);
        p = param();
        parse_tree.addNaryChild(p);
        match(NEWLINE);
    }
    if(peek(COMMENT)){
        ParseNode comment = parse_tree.addTerminal(OP_COMMENT, selection());
        parse_tree.setFlag(p, comment);
        advance();
        consume(NEWLINE);
    }
    Typeset::Selection sel(left, rMark());
    consume(RIGHTPAREN);
    if(noErrors()){
        registerGrouping(sel);
        return parse_tree.finishNary(OP_LIST, sel);
    }else{
        return PARSE_ERROR;
    }
}

ParseNode Parser::captureList() alloc_except {
    Typeset::Marker left = lMarkPrev();

    parse_tree.prepareNary();
    do{
        match(NEWLINE);
        parse_tree.addNaryChild(isolatedIdentifier());
        match(NEWLINE);
    } while(match(COMMA) && noErrors());
    Typeset::Selection sel(left, rMark());
    consume(RIGHTBRACE);

    if(noErrors()){
        registerGrouping(sel);
        return parse_tree.finishNary(OP_LIST, sel);
    }else{
        return PARSE_ERROR;
    }
}

ParseNode Parser::grouping(size_t type, ForscapeTokenType close) alloc_except {
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = disjunction();
    Typeset::Selection sel(left, rMark());
    if(match(close)){
        registerGrouping(sel);
        return parse_tree.addUnary(type, sel, nested);
    }else{
        return error(EXPECT_GROUPING_CLOSE);
    }
}

ParseNode Parser::set() noexcept {
    Typeset::Marker left = lMark();
    advance();
    if(match(RIGHTBRACKET)){
        Typeset::Selection sel(left, rMarkPrev());
        registerGrouping(sel);
        return parse_tree.addTerminal(OP_EMPTY_SET, sel);
    }

    ParseNode first_element = comparison();

    if(match(BAR) || match(COLON)){
        //Set builder notation
        ParseNode predicate = disjunction();
        consume(RIGHTBRACKET);

        Typeset::Selection sel(left, rMarkPrev());
        if(noErrors()){
            registerGrouping(sel);
            return parse_tree.addNode<2>(OP_SET_BUILDER, sel, {first_element, predicate});
        }else{
            return PARSE_ERROR;
        }
    }else{
        //Enumerated set
        parse_tree.prepareNary();
        parse_tree.addNaryChild(first_element);

        while(!match(RIGHTBRACKET)){
            consume(COMMA);
            if(!noErrors()) return PARSE_ERROR;
            parse_tree.addNaryChild(expression());
        }

        Typeset::Selection sel(left, rMarkPrev());
        if(noErrors()){
            registerGrouping(sel);
            return parse_tree.finishNary(OP_SET_ENUMERATED, sel);
        }else{
            return PARSE_ERROR;
        }
    }
}

template<Op basic, Op positive, Op negative>
ParseNode Parser::setWithSigns() noexcept {
    advance();
    if(!match(TOKEN_SUPERSCRIPT)) return parse_tree.addTerminal(basic, selectionPrev());

    Typeset::Marker left = lMarkPrev();

    switch (currentType()) {
        case PLUS:
            advance();
            consume(ARGCLOSE);
            return parse_tree.addTerminal(positive, Typeset::Selection(left, rMarkPrev()));

        case MINUS:
            if(!lookahead(ARGCLOSE)) break;
            index += 2;
            return parse_tree.addTerminal(negative, Typeset::Selection(left, rMarkPrev()));

        default: break;
    }

    ParseNode pn;

    parsing_dims = true;
    ParseNode dim1 = expression();
    if(match(ARGCLOSE)){
        pn = parse_tree.addUnary(basic, Typeset::Selection(left, rMarkPrev()), dim1);
    }else{
        ParseNode dim2 = expression();
        consume(ARGCLOSE);
        pn = parse_tree.addNode<2>(basic, Typeset::Selection(left, rMarkPrev()), {dim1, dim2});
    }
    parsing_dims = false;

    return pn;
}

ParseNode Parser::generalSet(Op op) noexcept {
    advance();
    if(!match(TOKEN_SUPERSCRIPT)) return parse_tree.addTerminal(op, selectionPrev());

    Typeset::Marker left = lMarkPrev();

    ParseNode pn;

    parsing_dims = true;
    ParseNode dim1 = expression();
    if(match(ARGCLOSE)){
        pn = parse_tree.addUnary(op, Typeset::Selection(left, rMarkPrev()), dim1);
    }else{
        ParseNode dim2 = expression();
        consume(ARGCLOSE);
        pn = parse_tree.addNode<2>(op, Typeset::Selection(left, rMarkPrev()), {dim1, dim2});
    }
    parsing_dims = false;

    return pn;
}

Forscape::ParseNode Forscape::Code::Parser::integerRange() noexcept {
    Typeset::Marker left = lMark();
    advance();
    ParseNode start = expression();
    consume(COMMA);
    ParseNode end = expression();
    consume(RIGHTDOUBLEBRACE);
    Typeset::Selection sel(left, rMarkPrev());
    registerGrouping(sel);

    return parse_tree.addNode<2>(OP_INTERVAL_INTEGER, sel, {start, end});
}

ParseNode Parser::norm() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = expression();
    Typeset::Marker right = rMark();
    Typeset::Selection sel(left, right);
    if(!match(DOUBLEBAR)) return error(EXPECT_DOUBLEBAR);
    registerGrouping(sel);

    if(match(TOKEN_SUBSCRIPT)){
        Typeset::Marker right = rMarkPrev();
        Typeset::Selection s(left, right);
        if(match(INFTY)){
            consume(ARGCLOSE);
            return parse_tree.addUnary(OP_NORM_INFTY, s, nested);
        }else if(peek(INTEGER) && rMark().index - lMark().index == 1){
            if(lMark().charRight() == 1){
                advance();
                consume(ARGCLOSE);
                return parse_tree.addUnary(OP_NORM_1, s, nested);
            }else if(lMark().charRight() == 2){
                advance();
                consume(ARGCLOSE);
                return parse_tree.addUnary(OP_NORM, s, nested);
            }
        }
        ParseNode e = expression();
        consume(ARGCLOSE);
        return parse_tree.addNode<2>(OP_NORM_p, s, {nested, e});
    }else{
        return parse_tree.addUnary(OP_NORM, sel, nested);
    }
}

ParseNode Parser::innerProduct() alloc_except {
    Typeset::Marker left = lMark();
    advance();
    ParseNode lhs = expression();
    if(!match(COMMA)) consume(BAR);
    ParseNode rhs = expression();
    Typeset::Selection sel(left, rMark());
    if(!match(RIGHTANGLE)) return error(EXPECT_RANGLE);
    registerGrouping(sel);

    return parse_tree.addNode<2>(OP_INNER_PRODUCT, sel, {lhs, rhs});
}

ParseNode Parser::integer() alloc_except {
    const Typeset::Marker& left = lMark();
    const Typeset::Selection& sel = selection();

    if(left.charRight() == '0' &&
       rMark().index - left.index > 1) return error(LEADING_ZEROES);

    ParseNode n = terminalAndAdvance(OP_INTEGER_LITERAL);

    switch (currentType()) {
        case TOKEN_SUBSCRIPT:{
            sel.format(SEM_PREDEFINEDMATNUM);
            if(sel.strView() == "0")
                return twoDims(OP_ZERO_MATRIX);
            else if(sel.strView() == "1")
                return twoDims(OP_ONES_MATRIX);
            else return error(UNRECOGNIZED_SYMBOL);
        }
        case PERIOD:{
            advance();
            if(!peek(INTEGER)) return error(TRAILING_DOT, Typeset::Selection(left, rMarkPrev()));
            model->registerCommaSeparatedNumber(sel);
            const Typeset::Selection sel(left, rMark());
            sel.formatNumber();
            ParseNode decimal = terminalAndAdvance(OP_INTEGER_LITERAL);
            ParseNode pn = parse_tree.addNode<2>(OP_DECIMAL_LITERAL, sel, {n, decimal});
            registerParseNodeRegion(pn, index-3);
            registerParseNodeRegion(pn, index-2);
            registerParseNodeRegion(pn, index-1);
            double val = stod(parse_tree.str(pn));
            parse_tree.setDouble(pn, val);
            parse_tree.setRows(pn, 1);
            parse_tree.setCols(pn, 1);
            parse_tree.setValue(pn, val);
            return pn;
        }
        default:
            double val = stod(parse_tree.str(n));
            parse_tree.setDouble(n, val);
            parse_tree.setRows(n, 1);
            parse_tree.setCols(n, 1);
            parse_tree.setValue(n, val);
            model->registerCommaSeparatedNumber(sel);
            registerParseNodeRegion(n, index-1);
            sel.formatNumber();
            return n;
    }
}

ParseNode Parser::identifier() alloc_except{
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
    parse_tree.setSymbol(id, nullptr);
    registerParseNodeRegion(id, index-1);
    return identifierFollowOn(id);
}

ParseNode Parser::identifierFollowOn(ParseNode id) noexcept{
    switch (currentType()) {
        case MAPSTO: return lambda(parse_tree.addUnary(OP_LIST, id));
        case TOKEN_SUBSCRIPT:
            if(parse_tree.str(id) == "e"){
                parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                return oneDim(OP_UNIT_VECTOR_AUTOSIZE);
            }else if(parse_tree.str(id) == "I" && noErrors()){
                size_t index_backup = index;
                ParseNode pn = twoDims(OP_IDENTITY_MATRIX);
                if(noErrors()){
                    parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                    #ifndef FORSCAPE_TYPESET_HEADLESS
                    parse_tree.getLeft(pn).text->retagParseNodeLast(pn);
                    parse_tree.getSelection(pn).mapConstructToParseNode(pn);
                    #endif
                    return pn;
                }
                index = index_backup;

                //EVENTUALLY: you have to address this heinuous hack
                model->errors.clear();
            }

            return id;
        case TOKEN_SUPERSCRIPT:
            if(lookahead(MULTIPLY)){
                advance();
                advance();
                parse_tree.setRight(id, rMark());
                #ifndef FORSCAPE_TYPESET_HEADLESS
                registerParseNodeRegion(id, index-1);
                parse_tree.getSelection(id).mapConstructToParseNode(id);
                #endif
                consume(ARGCLOSE);
                return identifierFollowOn(id);
            }
            return id;
        case TOKEN_DUALSCRIPT:
            if(parse_tree.str(id) == "e"){
                parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                Typeset::Selection c(lMarkPrev(), rMark());
                advance();
                parsing_dims = true;
                ParseNode dim0 = expression();
                if(!noErrors()) return PARSE_ERROR;
                if(!match(TIMES)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode dim1 = expression();
                if(!noErrors()) return PARSE_ERROR;
                if(!match(ARGCLOSE)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode elem = expression();
                if(!noErrors()) return PARSE_ERROR;
                if(!match(ARGCLOSE)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode n = parse_tree.addNode<3>(OP_UNIT_VECTOR, c, {elem, dim0, dim1});
                #ifndef FORSCAPE_TYPESET_HEADLESS
                parse_tree.getLeft(n).text->retagParseNodeLast(n);
                parse_tree.getSelection(n).mapConstructToParseNode(n);
                #endif
                parsing_dims = false;
                return n;
            }
            return id;
        default:
            return id;
    }
}

ParseNode Parser::isolatedIdentifier() alloc_except{
    if(!peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL);
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
    parse_tree.setSymbol(id, nullptr);
    registerParseNodeRegion(id, index-1);
    switch (currentType()) {
        case TOKEN_SUBSCRIPT:
            advance();
            if((match(IDENTIFIER) || match(INTEGER)) && match(ARGCLOSE)){
                parse_tree.setRight(id, rMarkPrev());
                #ifndef FORSCAPE_TYPESET_HEADLESS
                registerParseNodeRegion(id, index-2);
                parse_tree.getSelection(id).mapConstructToParseNode(id);
                #endif
                return id;
            }else{
                return error(INVALID_PARAMETER);
            }
        case TOKEN_SUPERSCRIPT:
            advance();
            if((match(IDENTIFIER) || match(MULTIPLY)) && match(ARGCLOSE)){
                parse_tree.setRight(id, rMarkPrev());
                #ifndef FORSCAPE_TYPESET_HEADLESS
                registerParseNodeRegion(id, index-2);
                parse_tree.getSelection(id).mapConstructToParseNode(id);
                #endif
                return id;
            }else{
                return error(INVALID_PARAMETER);
            }
        default:
            return id;
    }
}

ParseNode Parser::param() alloc_except{
    ParseNode id = isolatedIdentifier();
    return match(EQUALS) ? parse_tree.addNode<2>(OP_EQUAL, {id, expression()}) : id;
}

ParseNode Parser::lambda(ParseNode params) alloc_except{
    advance();

    Typeset::Marker left = parse_tree.getLeft(params);

    ParseNode capture_list = NONE;
    ParseNode referenced_upvalues = NONE;
    ParseNode e = expression();
    if(!noErrors()) return e;

    Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addNode<4>(OP_LAMBDA, sel, {capture_list, referenced_upvalues, params, e});
}

ParseNode Parser::fraction() alloc_except{
    Typeset::Selection c = selection();
    advance();

    switch (currentType()) {
        case DOUBLESTRUCK_D: return fractionDeriv(c, OP_DERIVATIVE, DOUBLESTRUCK_D);
        case PARTIAL: return fractionDeriv(c, OP_PARTIAL, PARTIAL);
        default: return fractionDefault(c);
    }
}

ParseNode Parser::fractionDeriv(const Typeset::Selection& c, Op type, ForscapeTokenType tt) alloc_except{
    advance();
    if(match(ARGCLOSE)){
        consume(tt);
        if(!noErrors()) return PARSE_ERROR; //EVENTUALLY: the parser error strategy is terrible
        ParseNode id = isolatedIdentifier();
        if(!noErrors()) return PARSE_ERROR;
        consume(ARGCLOSE);
        if(!noErrors()) return PARSE_ERROR;
        ParseNode expr = multiplication();
        if(!noErrors()) return PARSE_ERROR;
        Typeset::Selection sel(c.left, rMarkPrev());
        ParseNode val = parse_tree.addTerminal(OP_IDENTIFIER, Typeset::Selection(parse_tree.getSelection(id)));
        return parse_tree.addNode<3>(type, sel, {expr, id, val});
    }else{
        ParseNode expr = multiplication();
        if(!noErrors()) return PARSE_ERROR;
        consume(ARGCLOSE);
        if(!noErrors()) return PARSE_ERROR;
        consume(tt);
        if(!noErrors()) return PARSE_ERROR;
        ParseNode id = isolatedIdentifier();
        if(!noErrors()) return PARSE_ERROR;
        consume(ARGCLOSE);
        if(!noErrors()) return PARSE_ERROR;
        ParseNode val = parse_tree.addTerminal(OP_IDENTIFIER, Typeset::Selection(parse_tree.getSelection(id)));
        return parse_tree.addNode<3>(type, c, {expr, id, val});
    }
}

ParseNode Parser::fractionDefault(const Typeset::Selection& c) alloc_except{
    ParseNode num = expression();
    consume(ARGCLOSE);
    ParseNode den = expression();
    consume(ARGCLOSE);

    return parse_tree.addNode<2>(OP_FRACTION, c, {num, den});
}

ParseNode Parser::binomial() alloc_except{
    Typeset::Selection c = selection();
    advance();

    ParseNode n = expression();
    consume(ARGCLOSE);
    ParseNode k = expression();
    consume(ARGCLOSE);

    return parse_tree.addNode<2>(OP_BINOMIAL, c, {n, k});
}

ParseNode Parser::superscript(ParseNode lhs) alloc_except{
    if(lhs == PARSE_ERROR){
        while(!match(ARGCLOSE)) advance();
        return PARSE_ERROR;
    }
    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);
    advance();

    ParseNode n;

    switch (currentType()) {
        case TRANSPOSE:{
            advance();
            n = parse_tree.addUnary(OP_TRANSPOSE, c, lhs);
            break;
        }
        case DAGGER:{
            advance();
            n = parse_tree.addUnary(OP_DAGGER, c, lhs);
            break;
        }
        case PLUS:{
            advance();
            n = parse_tree.addUnary(OP_PSEUDO_INVERSE, c, lhs);
            break;
        }
        case CONJUNCTION:
        case CARET:{
            advance();
            n = parse_tree.addUnary(OP_ACCENT_HAT, c, lhs);
            break;
        }
        case DISJUNCTION:{
            advance();
            n = parse_tree.addUnary(OP_BIJECTIVE_MAPPING, c, lhs);
            break;
        }
        case COMPLEMENT:{
            advance();
            n = parse_tree.addUnary(OP_COMPLEMENT, c, lhs);
            break;
        }
        default:
            static constexpr size_t TYPESET_RATHER_THAN_CARET = 1;
            if(parse_tree.getOp(lhs) == OP_POWER && parse_tree.getFlag(lhs) == TYPESET_RATHER_THAN_CARET){
                //Due to typesetting the precedence of adjacent scritps x^{a}^{b} would parse incorrectly
                //as (x^{a})^{b}, except that we patch it here
                index--;
                ParseNode new_power = superscript(parse_tree.rhs(lhs));
                parse_tree.setArg<1>(lhs, new_power);
                parse_tree.setRight(lhs, right);
                return lhs;
            }
            n = parse_tree.addNode<2>(OP_POWER, c, {lhs, expression()});
            parse_tree.setFlag(n, TYPESET_RATHER_THAN_CARET);
    }

    consume(ARGCLOSE);

    return n;
}

ParseNode Parser::subscript(ParseNode lhs, const Typeset::Marker& right) alloc_except{
    if(lhs == PARSE_ERROR){
        while(!match(ARGCLOSE)) advance();
        return PARSE_ERROR;
    }

    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Selection selection(left, right);
    advance();

    ParseNode first = subExpr<true>();
    if(parse_tree.getOp(first) == OP_EQUAL){
        parse_tree.prepareNary();
        parse_tree.addNaryChild(lhs);
        parse_tree.addNaryChild(first);
        while(match(COMMA)) { parse_tree.addNaryChild(comparison()); }

        consume(ARGCLOSE);

        return parse_tree.finishNary(OP_EVAL, selection);
    }

    parse_tree.prepareNary();
    parse_tree.addNaryChild(lhs);
    parse_tree.addNaryChild(first);
    while(match(COMMA)) { parse_tree.addNaryChild(subExpr()); }

    consume(ARGCLOSE);

    return parse_tree.finishNary(OP_SUBSCRIPT_ACCESS, selection);
}

ParseNode Parser::dualscript(ParseNode lhs) alloc_except{
    if(lhs == PARSE_ERROR){
        while(!match(ARGCLOSE)) advance();
        return PARSE_ERROR;
    }

    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);
    advance();

    switch (currentType()) {
        case TRANSPOSE:{
            advance();
            require(ARGCLOSE);
            return parse_tree.addUnary(OP_TRANSPOSE, c, subscript(lhs, right));
        }
        case DAGGER:{
            advance();
            require(ARGCLOSE);
            return parse_tree.addUnary(OP_DAGGER, c, subscript(lhs, right));
        }
        case MULTIPLY:{
            parse_tree.setRight(lhs, right);
            advance();
            require(ARGCLOSE);
            return subscript(lhs, right);
        }
        case PLUS:{
            advance();
            require(ARGCLOSE);
            return parse_tree.addUnary(OP_PSEUDO_INVERSE, c, subscript(lhs, right));
        }
        default:
            ParseNode power = expression();
            require(ARGCLOSE);
            return parse_tree.addNode<2>(OP_POWER, c, {subscript(lhs, right), power});
    }
}

template<bool first_arg>
ParseNode Parser::subExpr() alloc_except{
    ParseNode first;

    if(first_arg && match(BAR)){
        Typeset::Marker left = lMarkPrev();
        ParseNode child = comparison();
        if(!match(BAR))
            return parse_tree.getOp(child) == OP_EQUAL ? child : error(EXPECT_RPAREN);
        Typeset::Selection sel(left, rMarkPrev());
        registerGrouping(sel);
        first = parse_tree.addUnary(OP_ABS, sel, child);
    }else{
        first = peek(COLON) ?
              parse_tree.addTerminal(OP_SLICE_ALL, selection()) :
              expression();
    }

    if(!match(COLON))
        return first; //Simple subscript
    else if(peek(COMMA) || peek(ARGCLOSE))
        return parse_tree.addUnary(OP_SLICE, first); //x_:,1 access

    ParseNode second = peek(COLON) ?
                       parse_tree.addTerminal(OP_SLICE_ALL, selection()) :
                       expression();

    if(!match(COLON)) return parse_tree.addNode<2>(OP_SLICE, {first, second});
    else return parse_tree.addNode<3>(OP_SLICE, {first, second, expression()});
}

ParseNode Parser::matrix() alloc_except{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();
    if(argc == 1) return error(SCALAR_MATRIX, c);

    parse_tree.prepareNary();
    for(size_t i = 0; i < argc; i++){
        parse_tree.addNaryChild(expression());
        consume(ARGCLOSE);
    }

    ParseNode m = parse_tree.finishNary(OP_MATRIX, c);
    parse_tree.setFlag(m, c.getMatrixRows());
    #ifndef FORSCAPE_TYPESET_HEADLESS
    if(noErrors()) c.mapConstructToParseNode(m);
    #endif

    return m;
}

ParseNode Parser::cases() alloc_except{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();

    parse_tree.prepareNary();
    for(size_t i = 0; i < argc; i++){
        parse_tree.addNaryChild(disjunction());
        consume(ARGCLOSE);
    }

    return parse_tree.finishNary(OP_CASES, c);
}

ParseNode Parser::squareRoot() alloc_except{
    const Typeset::Selection& c = selection();
    advance();
    ParseNode n = parse_tree.addUnary(OP_SQRT, c, expression());
    consume(ARGCLOSE);

    return n;
}

ParseNode Parser::nRoot() alloc_except{
    const Typeset::Selection& c = selection();
    advance();
    ParseNode base = expression();
    consume(ARGCLOSE);
    ParseNode arg = expression();
    consume(ARGCLOSE);

    return parse_tree.addNode<2>(OP_ROOT, c, {arg, base});
}

ParseNode Parser::limit() alloc_except {
    const Typeset::Marker& left = lMark();
    advance();
    ParseNode id = isolatedIdentifier();
    consume(ARGCLOSE);
    ParseNode lim = expression();
    consume(ARGCLOSE);
    ParseNode expr = expression();
    const Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addNode<3>(OP_LIMIT, sel, {id, lim, expr});
}

ParseNode Parser::indefiniteIntegral() noexcept {
    const Typeset::Marker& left = lMark();
    advance();
    ParseNode kernel = expression();
    consume(DOUBLESTRUCK_D);
    ParseNode var = isolatedIdentifier();
    const Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addNode<2>(OP_INTEGRAL, sel, {var, kernel});
}

ParseNode Parser::definiteIntegral() alloc_except {
    const Typeset::Marker& left = lMark();
    advance();
    ParseNode tf = expression();
    consume(ARGCLOSE);
    ParseNode t0 = expression();
    consume(ARGCLOSE);
    ParseNode kernel = expression();
    consume(DOUBLESTRUCK_D);
    ParseNode var = isolatedIdentifier();
    const Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addNode<4>(OP_DEFINITE_INTEGRAL, sel, {var, tf, t0, kernel});
}

ParseNode Parser::oneDim(Op type) alloc_except{
    Typeset::Selection c(lMarkPrev(), rMark());
    advance();
    parsing_dims = true;
    ParseNode pn = parse_tree.addUnary(type, c, expression());
    consume(ARGCLOSE);
    parsing_dims = false;

    return pn;
}

ParseNode Parser::twoDims(Op type) alloc_except{
    Typeset::Selection c(lMarkPrev(), rMark());
    advance();
    parsing_dims = true;
    ParseNode lhs = expression();
    if(!noErrors()) return PARSE_ERROR;
    if(!match(TIMES)) return error(ErrorCode::INVALID_ARGS, Typeset::Selection(rMarkPrev(), rMarkPrev()));
    ParseNode pn = parse_tree.addNode<2>(type, c, {lhs, expression()});
    if(!noErrors()) return PARSE_ERROR;
    if(!match(ARGCLOSE)) return error(ErrorCode::INVALID_ARGS, Typeset::Selection(rMarkPrev(), rMarkPrev()));
    parsing_dims = false;

    return pn;
}

ParseNode Parser::trig(Op type) alloc_except{
    const Typeset::Marker& left = lMark();
    advance();
    switch (currentType()) {
        case TOKEN_SUPERSCRIPT:{
            advance();
            ParseNode power = expression();
            consume(ARGCLOSE);

            ParseNode fn = parse_tree.addLeftUnary(type, left, arg());
            const Typeset::Marker& right = parse_tree.getRight(fn);
            Typeset::Selection c(left, right);
            if(parse_tree.getOp(power) == OP_UNARY_MINUS)
                return error(AMBIGIOUS_TRIG_POWER, c);
            return parse_tree.addNode<2>(OP_POWER, c, {fn, power});
        }

        default: return parse_tree.addLeftUnary(type, left, arg());
    }
}

ParseNode Parser::log() alloc_except{
    const Typeset::Marker& left = lMark();
    advance();
    switch(currentType()){
        case TOKEN_SUBSCRIPT:{
            advance();
            ParseNode base = expression();
            consume(ARGCLOSE);
            ParseNode child_arg = arg();
            const Typeset::Marker& right = parse_tree.getRight(child_arg);
            Typeset::Selection c(left, right);
            return parse_tree.addNode<2>(OP_LOGARITHM_BASE, c, {child_arg, base});
        }

        default: return parse_tree.addLeftUnary(OP_LOGARITHM, left, arg());
    }
}

ParseNode Parser::oneArgRequireParen(Op type) noexcept {
    Typeset::Marker start = lMark();
    advance();
    Typeset::Marker group_start = lMark();
    consume(LEFTPAREN);
    ParseNode child = expression();
    consume(RIGHTPAREN);

    registerGrouping(group_start, rMarkPrev());

    return parse_tree.addUnary(type, Typeset::Selection(start, rMarkPrev()), child);
}

ParseNode Parser::oneArg(Op type) alloc_except{
    advance();

    return parse_tree.addUnary(type, arg());
}

ParseNode Parser::twoArgs(Op type) alloc_except{
    const Typeset::Marker& left = lMark();
    advance();
    const Typeset::Marker& left_grouping = lMark();
    consume(LEFTPAREN);
    ParseNode a = expression();
    consume(COMMA);
    ParseNode b = expression();
    const Typeset::Marker& right = rMark();
    consume(RIGHTPAREN);
    if(!noErrors()) return PARSE_ERROR;
    registerGrouping(left_grouping, right);
    Typeset::Selection c(left, right);

    return parse_tree.addNode<2>(type, c, {a, b});
}

ParseNode Parser::arg() alloc_except{
    if(peek(LEFTPAREN)) return grouping(OP_GROUP_PAREN, RIGHTPAREN);
    else return leftUnary();
}

ParseNode Parser::big(Op type) alloc_except{
    const Typeset::Marker& left = lMark();
    Typeset::Selection err_sel = selection();
    advance();
    ParseNode end = expression();
    consume(ARGCLOSE);
    if(!noErrors() || !peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
    registerParseNodeRegion(id, index-1);
    if(!match(EQUALS) && !match(LEFTARROW)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode start = expression();
    ParseNode assign = parse_tree.addNode<2>(OP_ASSIGN, {id, start});
    consume(ARGCLOSE);
    if(!noErrors()) return PARSE_ERROR;
    ParseNode body = expression();
    if(!noErrors()) return PARSE_ERROR;
    const Typeset::Marker& right = rMarkPrev();
    Typeset::Selection sel(left, right);

    return parse_tree.addNode<3>(type, sel, {assign, end, body});
}

ParseNode Parser::oneArgConstruct(Op type) alloc_except{
    Typeset::Selection sel = selection();
    advance();
    ParseNode child = disjunction();
    consume(ARGCLOSE);
    return parse_tree.addUnary(type, sel, child);
}

ParseNode Parser::error(ErrorCode code) alloc_except {
    return error(code, selection());
}

ParseNode Parser::error(ErrorCode code, const Typeset::Selection& c) alloc_except {
    if(noErrors()){
        error_stream.fail(c, code);
        error_node = parse_tree.addTerminal(OP_ERROR, Typeset::Selection(c));
    }

    return error_node;
}

void Parser::advance() noexcept{
    index++;
}

bool Parser::match(ForscapeTokenType type) noexcept{
    if(tokens[index].type == type){
        advance();
        return true;
    }else{
        return false;
    }
}

bool Parser::peek(ForscapeTokenType type) const noexcept{
    return tokens[index].type == type;
}

bool Parser::lookahead(ForscapeTokenType type) const noexcept{
    assert(index+1 < tokens.size());
    return tokens[index+1].type == type;
}

void Parser::require(ForscapeTokenType type) noexcept{
    if(tokens[index].type != type) error(CONSUME);
}

void Parser::consume(ForscapeTokenType type) noexcept{
    if(tokens[index].type == type){
        advance();
    }else{
        error(CONSUME);
    }
}

void Parser::skipNewlines() noexcept{
    while(tokens[index].type == NEWLINE || tokens[index].type == COMMENT) index++;
}

void Parser::skipNewline() noexcept{
    if(tokens[index].type == COMMENT) index+=2;
    else if(tokens[index].type == NEWLINE) index++;
}

ForscapeTokenType Parser::currentType() const noexcept{
    return tokens[index].type;
}

ParseNode Parser::makeTerminalNode(size_t type) alloc_except {
    return parse_tree.addTerminal(type, selection());
}

ParseNode Parser::terminalAndAdvance(size_t type) alloc_except {
    ParseNode pn = parse_tree.addTerminal(type, selection());
    index++;

    return pn;
}

const Typeset::Selection Parser::selection() const noexcept{
    if(tokens[index].type == ARGCLOSE) return Typeset::Selection(tokens[index].sel.left, tokens[index].sel.left);
    return tokens[index].sel;
}

const Typeset::Selection Parser::selectionPrev() const noexcept{
    return tokens[index-1].sel;
}

const Typeset::Marker& Parser::lMark() const noexcept{
    return tokens[index].sel.left;
}

const Typeset::Marker& Parser::rMark() const noexcept{
    return tokens[index].sel.right;
}

const Typeset::Marker& Parser::lMarkPrev() const noexcept{
    return tokens[index-1].sel.left;
}

const Typeset::Marker& Parser::rMarkPrev() const noexcept{
    return tokens[index-1].sel.right;
}

bool Parser::noErrors() const noexcept{
    return model->errors.empty();
}

void Parser::recover() noexcept{
    //error_node = NONE;
    index = tokens.size()-1; //Give up for now //EVENTUALLY: improve error recovery
}

#ifndef FORSCAPE_TYPESET_HEADLESS
void Parser::registerParseNodeRegion(ParseNode pn, size_t token_index) alloc_except {
    if(pn == PARSE_ERROR) return;

    assert(token_index < tokens.size());
    assert(parse_tree.isLastAllocatedNode(pn));

    const Typeset::Selection& sel = tokens[token_index].sel;
    const Typeset::Marker& left = sel.left;
    left.text->tagParseNode(pn, left.index, sel.right.index);
}

void Parser::registerParseNodeRegionToPatch(size_t token_index) alloc_except {
    assert(!token_stack_frames.empty());
    const Typeset::Selection& sel = tokens[token_index].sel;
    const Typeset::Marker& left = sel.left;
    size_t tag_index = left.text->tagParseNode(NONE, left.index, sel.right.index);
    token_map_stack.push_back(std::make_pair(token_index, tag_index));
}

void Parser::startPatch() noexcept {
    token_stack_frames.push_back(token_map_stack.size());
}

void Parser::finishPatch(ParseNode pn) noexcept {
    if(!noErrors()) return;

    assert(!token_stack_frames.empty());
    size_t frame = token_stack_frames.back();
    token_stack_frames.pop_back();

    for(size_t i = frame; i < token_map_stack.size(); i++){
        const auto& entry = token_map_stack[i];
        const Typeset::Marker& m = tokens[entry.first].sel.left;
        m.text->patchParseNode(pn, entry.second);
    }

    token_map_stack.resize(frame);
}
#endif

}

}
