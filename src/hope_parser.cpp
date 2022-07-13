#include "hope_parser.h"

#include <code_parsenode_ops.h>
#include <hope_common.h>
#include "typeset_model.h"

#ifdef HOPE_TYPESET_HEADLESS
#define registerParseNodeRegion(a, b)
#define registerParseNodeRegionToPatch(a)
#define startPatch()
#define finishPatch(a)
#endif

namespace Hope {

namespace Code {

Parser::Parser(const Scanner& scanner, Typeset::Model* model) noexcept
    : tokens(scanner.tokens), errors(model->errors), model(model), error_node(NONE) {}

void Parser::parseAll() alloc_except {
    reset();

    #ifndef NDEBUG
    bool scanner_error = !errors.empty();
    #endif

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

    assert(!errors.empty() || parse_tree.inFinalState());
    assert((errors.empty() == (error_node == NONE)) || (scanner_error && (error_node == NONE)));
}

void Parser::reset() noexcept {
    parse_tree.clear();
    token_map_stack.clear();
    #ifndef HOPE_TYPESET_HEADLESS
    open_symbols.clear();
    close_symbols.clear();
    #endif
    index = 0;
    parsing_dims = false;
    loops = 0;
    error_node = NONE;
}

void Parser::registerGrouping(const Typeset::Selection& sel) alloc_except {
    registerGrouping(sel.left, sel.right);
}

void Parser::registerGrouping(const Typeset::Marker& l, const Typeset::Marker& r) alloc_except {
    #ifndef HOPE_TYPESET_HEADLESS
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
        assert(!errors.empty());
        recover();
        return n;
    }
}

ParseNode Parser::statement() alloc_except {
    skipNewlines();

    switch (currentType()) {
        case IF: return ifStatement();
        case WHILE: return whileStatement();
        case FOR: return forStatement();
        case PRINT: return printStatement();
        case ASSERT: return assertStatement();
        case ALGORITHM: return algStatement();
        case RETURN: return returnStatement();
        case BREAK: return loops ? terminalAndAdvance(OP_BREAK) : error(BAD_BREAK);
        case CONTINUE: return loops ? terminalAndAdvance(OP_CONTINUE) : error(BAD_CONTINUE);
        case PLOT: return plotStatement();
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
    consume(SEMICOLON);
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
    consume(LEFTBRACKET);
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

ParseNode Parser::algStatement() alloc_except {
    Typeset::Marker left = lMark();
    advance();

    ParseNode id = isolatedIdentifier();

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

ParseNode Parser::mathStatement() alloc_except {
    ParseNode n = expression();

    switch (currentType()) {
        case EQUALS:
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
    if(!noErrors()) return error_node;
    ParseNode body = parse_tree.addUnary(OP_RETURN, expr);

    ParseNode id = parse_tree.arg<0>(call);
    if(parse_tree.getOp(id) == OP_SUBSCRIPT_ACCESS){
        parse_tree.setOp(id, OP_IDENTIFIER);

        #ifndef HOPE_TYPESET_HEADLESS
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
    for(size_t i = 0; i < nargs; i++)
        parse_tree.setArg(params, i, parse_tree.arg(call, i+1));
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
    parse_tree.setFlag(lhs, match(COMMENT) ? parse_tree.addTerminal(OP_COMMENT, selectionPrev()) : NONE);
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
    parse_tree.setFlag(lhs, match(COMMENT) ? parse_tree.addTerminal(OP_COMMENT, selectionPrev()) : NONE);
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
        default: return implicitMult();
    }
}

ParseNode Parser::implicitMult() alloc_except {
    ParseNode n = rightUnary();

    switch (currentType()) {
        HOPE_IMPLICIT_MULT_TOKENS
        case LEFTPAREN:{
            ParseNode pn = collectImplicitMult(n);
            return parse_tree.getNumArgs(pn) != 1 ? pn : parse_tree.child(pn);
        }
        default:
            return n;
    }
}

ParseNode Parser::collectImplicitMult(ParseNode n) alloc_except {
    parse_tree.prepareNary();
    parse_tree.addNaryChild(n);

    for(;;){
        if(!noErrors()) return error_node;

        switch (currentType()) {
            HOPE_IMPLICIT_MULT_TOKENS
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

    while(!match(RIGHTPAREN)){
        consume(COMMA);
        if(!noErrors()) return error_node;
        parse_tree.addNaryChild(disjunction());
    }

    if(!noErrors()) return error_node;

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
                startPatch();
                registerParseNodeRegionToPatch(index);
                advance();
                ParseNode pn = parse_tree.addNode<2>(OP_POWER, {n, implicitMult()});
                finishPatch(pn);
                return pn;
            }
            case TOKEN_SUPERSCRIPT: n = superscript(n); break;
            case TOKEN_SUBSCRIPT: n = subscript(n, rMark()); break;
            case TOKEN_DUALSCRIPT: n = dualscript(n); break;
            default: return n;
        }
    }
}

ParseNode Parser::primary() alloc_except {
    switch (currentType()) {
        case INTEGER: return integer();
        case IDENTIFIER: return identifier();
        case STRING: return string();

        //Value literal
        case INFTY: return terminalAndAdvance(OP_INFTY);
        case EMPTYSET: return terminalAndAdvance(OP_EMPTY_SET);
        case TRUELITERAL: return terminalAndAdvance(OP_TRUE);
        case FALSELITERAL: return terminalAndAdvance(OP_FALSE);
        case GRAVITY: return terminalAndAdvance(OP_GRAVITY);

        //Grouping
        case LEFTPAREN: return parenGrouping();
        case LEFTCEIL: return grouping(OP_CEIL, RIGHTCEIL);
        case LEFTFLOOR: return grouping(OP_FLOOR, RIGHTFLOOR);
        case BAR: return grouping(OP_ABS, BAR);
        case DOUBLEBAR: return norm();
        case LEFTANGLE: return innerProduct();

        //Constructs
        case TOKEN_FRACTION: return fraction();
        case TOKEN_BINOMIAL: return binomial();
        case TOKEN_MATRIX: return matrix();
        case TOKEN_CASES: return cases();
        case TOKEN_SQRT: return squareRoot();
        case TOKEN_NRT: return nRoot();
        case TOKEN_LIMIT: return limit();
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

        //Keyword funcs
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

    parse_tree.prepareNary();
    do{
        match(NEWLINE);
        parse_tree.addNaryChild(param());
        match(NEWLINE);
    } while(match(COMMA) && noErrors());
    Typeset::Selection sel(left, rMark());
    consume(RIGHTPAREN);
    if(noErrors()){
        registerGrouping(sel);
        return parse_tree.finishNary(OP_LIST, sel);
    }else{
        return error_node;
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
        return error_node;
    }
}

ParseNode Parser::grouping(size_t type, HopeTokenType close) alloc_except {
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = disjunction();
    Typeset::Selection sel(left, rMark());
    consume(close);
    if(noErrors()) registerGrouping(sel);

    return parse_tree.addUnary(type, sel, nested);
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
    consume(BAR);
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
            const Typeset::Selection sel(left, rMark());
            sel.formatNumber();
            ParseNode decimal = terminalAndAdvance(OP_INTEGER_LITERAL);
            ParseNode pn = parse_tree.addNode<2>(OP_DECIMAL_LITERAL, sel, {n, decimal});
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
            sel.formatNumber();
            return n;
    }
}

ParseNode Parser::identifier() alloc_except{
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
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
                    #ifndef HOPE_TYPESET_HEADLESS
                    parse_tree.getLeft(pn).text->retagParseNodeLast(pn);
                    parse_tree.getSelection(pn).mapConstructToParseNode(pn);
                    #endif
                    return pn;
                }
                index = index_backup;
                errors.clear();
                error_node = NONE;
            }

            return id;
        case TOKEN_SUPERSCRIPT:
            if(lookahead(MULTIPLY)){
                advance();
                advance();
                parse_tree.setRight(id, rMark());
                #ifndef HOPE_TYPESET_HEADLESS
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
                if(!noErrors()) return error_node;
                if(!match(TIMES)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode dim1 = expression();
                if(!noErrors()) return error_node;
                if(!match(ARGCLOSE)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode elem = expression();
                if(!noErrors()) return error_node;
                if(!match(ARGCLOSE)) return error(UNRECOGNIZED_EXPR, Typeset::Selection(rMarkPrev(), rMarkPrev()));
                ParseNode n = parse_tree.addNode<3>(OP_UNIT_VECTOR, c, {elem, dim0, dim1});
                #ifndef HOPE_TYPESET_HEADLESS
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
    registerParseNodeRegion(id, index-1);
    switch (currentType()) {
        case TOKEN_SUBSCRIPT:
            advance();
            if((match(IDENTIFIER) || match(INTEGER)) && match(ARGCLOSE)){
                parse_tree.setRight(id, rMarkPrev());
                #ifndef HOPE_TYPESET_HEADLESS
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
                #ifndef HOPE_TYPESET_HEADLESS
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

ParseNode Parser::fractionDeriv(const Typeset::Selection& c, Op type, HopeTokenType tt) alloc_except{
    advance();
    if(match(ARGCLOSE)){
        consume(tt);
        if(!errors.empty()) return error_node;
        ParseNode id = isolatedIdentifier();
        if(!errors.empty()) return error_node;
        consume(ARGCLOSE);
        if(!errors.empty()) return error_node;
        ParseNode expr = multiplication();
        if(!errors.empty()) return error_node;
        Typeset::Selection sel(c.left, rMarkPrev());
        ParseNode val = parse_tree.addTerminal(OP_IDENTIFIER, Typeset::Selection(parse_tree.getSelection(id)));
        return parse_tree.addNode<3>(type, sel, {expr, id, val});
    }else{
        ParseNode expr = multiplication();
        if(!errors.empty()) return error_node;
        consume(ARGCLOSE);
        if(!errors.empty()) return error_node;
        consume(tt);
        if(!errors.empty()) return error_node;
        ParseNode id = isolatedIdentifier();
        if(!errors.empty()) return error_node;
        consume(ARGCLOSE);
        if(!errors.empty()) return error_node;
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
        default: n = parse_tree.addNode<2>(OP_POWER, c, {lhs, expression()});
    }

    consume(ARGCLOSE);

    return n;
}

ParseNode Parser::subscript(ParseNode lhs, const Typeset::Marker& right) alloc_except{
    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Selection selection(left, right);
    advance();

    parse_tree.prepareNary(); parse_tree.addNaryChild(lhs);
    do{ parse_tree.addNaryChild(subExpr()); } while(match(COMMA));

    consume(ARGCLOSE);

    return parse_tree.finishNary(OP_SUBSCRIPT_ACCESS, selection);
}

ParseNode Parser::dualscript(ParseNode lhs) alloc_except{
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

ParseNode Parser::subExpr() alloc_except{
    ParseNode first = peek(COLON) ?
                      parse_tree.addTerminal(OP_SLICE_ALL, selection()) :
                      expression();

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
    #ifndef HOPE_TYPESET_HEADLESS
    c.mapConstructToParseNode(m);
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
    if(!noErrors()) return error_node;
    if(!match(TIMES)) return error(ErrorCode::INVALID_ARGS, Typeset::Selection(rMarkPrev(), rMarkPrev()));
    ParseNode pn = parse_tree.addNode<2>(type, c, {lhs, expression()});
    if(!noErrors()) return error_node;
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

ParseNode Parser::oneArg(Op type) alloc_except{
    advance();

    return parse_tree.addUnary(type, arg());
}

ParseNode Parser::twoArgs(Op type) alloc_except{
    const Typeset::Marker& left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode a = expression();
    consume(COMMA);
    ParseNode b = expression();
    const Typeset::Marker& right = rMark();
    consume(RIGHTPAREN);
    if(!noErrors()) return error_node;
    registerGrouping(left, right);
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
    if(!noErrors()) return error_node;
    ParseNode body = expression();
    if(!noErrors()) return error_node;
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
        error_node = parse_tree.addTerminal(SCANNER_ERROR, c);
        errors.push_back(Error(c, code));
    }

    assert(errors.empty() == (error_node == NONE));
    assert(error_node != NONE);

    return error_node;
}

void Parser::advance() noexcept{
    index++;
}

bool Parser::match(HopeTokenType type) noexcept{
    if(tokens[index].type == type){
        advance();
        return true;
    }else{
        return false;
    }
}

bool Parser::peek(HopeTokenType type) const noexcept{
    return tokens[index].type == type;
}

bool Parser::lookahead(HopeTokenType type) const noexcept{
    assert(index+1 < tokens.size());
    return tokens[index+1].type == type;
}

void Parser::require(HopeTokenType type) noexcept{
    if(tokens[index].type != type) error(CONSUME);
}

void Parser::consume(HopeTokenType type) noexcept{
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

HopeTokenType Parser::currentType() const noexcept{
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
    return error_node == NONE;
}

void Parser::recover() noexcept{
    //error_node = NONE;
    index = tokens.size()-1; //Give up for now //EVENTUALLY: improve error recovery
}

#ifndef HOPE_TYPESET_HEADLESS
void Parser::registerParseNodeRegion(ParseNode pn, size_t token_index) alloc_except {
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
