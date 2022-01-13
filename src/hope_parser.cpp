#include "hope_parser.h"

#include <code_parsenode_ops.h>
#include "typeset_model.h"

namespace Hope {

namespace Code {

Parser::Parser(const Scanner &scanner, Typeset::Model* model)
    : tokens(scanner.tokens), markers(scanner.markers), errors(model->errors), model(model) {
    //DO NOTHING
}

void Parser::parseAll(){
    reset();

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_BLOCK);
    skipNewlines();
    while (!peek(ENDOFFILE)) {
        builder.addNaryChild( checkedStatement() );
        skipNewlines();
    }

    Typeset::Selection c(markers.front().first, markers.back().second);
    parse_tree.root = builder.finalize(c);
}

void Parser::reset() noexcept{
    parse_tree.clear();
    open_symbols.clear();
    close_symbols.clear();
    index = 0;
    parsing_dims = false;
    loops = 0;
    error_node = UNITIALIZED;
}

void Parser::registerGrouping(const Typeset::Selection& sel){
    registerGrouping(sel.left, sel.right);
}

void Parser::registerGrouping(const Typeset::Marker& l, const Typeset::Marker& r){
    open_symbols[l] = r;
    close_symbols[r] = l;
}

ParseNode Parser::checkedStatement() noexcept{
    ParseNode n = statement();
    if(noErrors()){
        return n;
    }else{
        recover();
        return n;
    }
}

ParseNode Parser::statement() noexcept{
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
        default: return mathStatement();
    }
}

ParseNode Parser::ifStatement() noexcept{
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

        return parse_tree.addTernary(OP_IF_ELSE, c, condition, body, el);
    }else{
        Typeset::Marker right = rMark();
        Typeset::Selection c(left, right);

        return parse_tree.addBinary(OP_IF, c, condition, body);
    }
}

Parser::ParseNode Parser::whileStatement() noexcept{
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

    return parse_tree.addBinary(OP_WHILE, c, condition, body);
}

Parser::ParseNode Parser::forStatement() noexcept{
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

    return parse_tree.addQuadary(OP_FOR, c, initializer, condition, update, body);
}

ParseNode Parser::printStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker l_group = lMark();
    if(!match(LEFTPAREN)) return error(EXPECT_LPAREN);

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_PRINT);

    do{
        builder.addNaryChild(disjunction());
    } while(noErrors() && match(COMMA));

    Typeset::Marker right = rMark();
    Typeset::Selection sel(left, right);
    if(!match(RIGHTPAREN)){
        builder.finalize(sel);
        return error(EXPECT_RPAREN);
    }
    registerGrouping(l_group, right);

    return builder.finalize(sel);
}

Parser::ParseNode Parser::assertStatement() noexcept{
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

ParseNode Parser::blockStatement() noexcept{
    match(NEWLINE);
    if(!peek(LEFTBRACKET)){
        ParseNode pn = statement();
        match(NEWLINE);
        return pn;
    }

    Typeset::Marker left = lMark();
    consume(LEFTBRACKET);
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_BLOCK);

    skipNewlines();
    while(noErrors() && !match(RIGHTBRACKET)){
        builder.addNaryChild( statement() );
        skipNewlines();
    }

    Typeset::Selection sel(left, rMarkPrev());
    if(noErrors()) registerGrouping(sel);

    return builder.finalize(sel);
}

Parser::ParseNode Parser::algStatement() noexcept{
    advance();

    ParseNode id = isolatedIdentifier();

    if(!peek(LEFTPAREN) & !peek(LEFTBRACE))
        return parse_tree.addUnary(OP_PROTOTYPE_ALG, id);

    ParseNode captures = match(LEFTBRACE) ? captureList() : ParseTree::EMPTY;

    ParseNode referenced_upvalues = ParseTree::EMPTY;

    consume(LEFTPAREN);
    ParseNode params = paramList();

    size_t loops_backup = 0;
    std::swap(loops_backup, loops);
    ParseNode body = blockStatement();
    std::swap(loops_backup, loops);

    return parse_tree.addPentary(
                OP_ALGORITHM,
                id,
                captures,
                referenced_upvalues,
                params,
                body
           );
}

Parser::ParseNode Parser::returnStatement() noexcept{
    Typeset::Marker m = lMark();
    advance();
    return parse_tree.addLeftUnary(OP_RETURN, m, disjunction());
}

ParseNode Parser::mathStatement() noexcept{
    ParseNode n = expression();

    switch (currentType()) {
        case EQUALS: return equality(n);
        case LEFTARROW: return assignment(n);
        default: return parse_tree.addUnary(OP_EXPR_STMT, n);
    }
}

ParseNode Parser::assignment(const ParseNode& lhs) noexcept{
    advance();
    return parse_tree.addBinary(OP_ASSIGN, lhs, expression());
}

ParseNode Parser::expression() noexcept{
    return addition();
}

ParseNode Parser::equality(const ParseNode& lhs) noexcept{
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_EQUAL);
    builder.addNaryChild(lhs);

    do {
        advance();
        builder.addNaryChild(expression());
    } while(peek(EQUALS));

    return builder.finalize();
}

Parser::ParseNode Parser::disjunction() noexcept{
    ParseNode n = conjunction();

    for(;;){
        switch (currentType()) {
            case DISJUNCTION: advance(); n = parse_tree.addBinary(OP_LOGICAL_OR, n, conjunction()); break;
            default: return n;
        }
    }
}

Parser::ParseNode Parser::conjunction() noexcept{
    ParseNode n = comparison();

    for(;;){
        switch (currentType()) {
            case CONJUNCTION: advance(); n = parse_tree.addBinary(OP_LOGICAL_AND, n, comparison()); break;
            default: return n;
        }
    }
}

Parser::ParseNode Parser::comparison() noexcept{
    ParseNode n = addition();

    for(;;){
        switch (currentType()) {
            case EQUALS: advance(); n = parse_tree.addBinary(OP_EQUAL, n, addition()); break;
            case NOTEQUAL: advance(); n = parse_tree.addBinary(OP_NOT_EQUAL, n, addition()); break;
            case LESS: advance(); n = parse_tree.addBinary(OP_LESS, n, addition()); break;
            case LESSEQUAL: advance(); n = parse_tree.addBinary(OP_LESS_EQUAL, n, addition()); break;
            case GREATER: advance(); n = parse_tree.addBinary(OP_GREATER, n, addition()); break;
            case GREATEREQUAL: advance(); n = parse_tree.addBinary(OP_GREATER_EQUAL, n, addition()); break;
            default: return n;
        }
    }
}

ParseNode Parser::addition() noexcept{
    ParseNode n = multiplication();

    for(;;){
        switch (currentType()) {
            case PLUS: advance(); n = parse_tree.addBinary(OP_ADDITION, n, multiplication()); break;
            case MINUS: advance(); n = parse_tree.addBinary(OP_SUBTRACTION, n, multiplication()); break;
            default: return n;
        }
    }
}

ParseNode Parser::multiplication() noexcept{
    ParseNode n = leftUnary();

    for(;;){
        switch (currentType()) {
            case MULTIPLY: advance(); n = parse_tree.addBinary(OP_MULTIPLICATION, n, leftUnary()); break;
            case DIVIDE: advance(); n = parse_tree.addBinary(OP_DIVIDE, n, leftUnary()); break;
            case FORWARDSLASH: advance(); n = parse_tree.addBinary(OP_FORWARDSLASH, n, leftUnary()); break;
            case BACKSLASH: advance(); n = parse_tree.addBinary(OP_BACKSLASH, n, leftUnary()); break;
            case TIMES: if(parsing_dims) return n; advance(); n = parse_tree.addBinary(OP_CROSS, n, leftUnary()); break;
            case DOTPRODUCT: advance(); n = parse_tree.addBinary(OP_DOT, n, leftUnary()); break;
            case PERCENT: advance(); n = parse_tree.addBinary(OP_MODULUS, n, leftUnary()); break;
            case OUTERPRODUCT: advance(); n = parse_tree.addBinary(OP_OUTER_PRODUCT, n, leftUnary()); break;
            case ODOT: advance(); n = parse_tree.addBinary(OP_ODOT, n, leftUnary()); break;
            default: return n;
        }
    }
}

ParseNode Parser::leftUnary() noexcept{
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

ParseNode Parser::implicitMult() noexcept{
    ParseNode n = rightUnary();

    switch (currentType()) {
        HOPE_IMPLICIT_MULT_TOKENS
            return collectImplicitMult(n);
        case INTEGER:
            return error(TRAILING_CONSTANT);
        default:
            return n;
    }
}

Parser::ParseNode Parser::collectImplicitMult(ParseNode n) noexcept{
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_IMPLICIT_MULTIPLY);
    builder.addNaryChild(n);
    builder.addNaryChild(rightUnary());

    for(;;){
        if(!noErrors()) return builder.finalize();

        switch (currentType()) {
            HOPE_IMPLICIT_MULT_TOKENS
                builder.addNaryChild(rightUnary());
                break;
            case INTEGER:
                builder.finalize();
                return error(TRAILING_CONSTANT);
            default:
                return builder.finalize();
        }
    }
}

ParseNode Parser::rightUnary() noexcept{
    ParseNode n = primary();

    for(;;){
        switch (currentType()) {
            case EXCLAM:{
                Typeset::Marker m = rMark();
                advance();
                return parse_tree.addRightUnary(OP_FACTORIAL, m, n);
            }
            case CARET: advance(); return parse_tree.addBinary(OP_POWER, n, implicitMult());
            case TOKEN_SUPERSCRIPT: n = superscript(n); break;
            case TOKEN_SUBSCRIPT: n = subscript(n, rMark()); break;
            case TOKEN_DUALSCRIPT: n = dualscript(n); break;
            default: return n;
        }
    }
}

ParseNode Parser::primary() noexcept{
    switch (currentType()) {
        case INTEGER: return integer();
        case IDENTIFIER: return identifier();

        //Value literal
        case INFTY: return terminalAndAdvance(OP_INFTY);
        case EMPTYSET: return terminalAndAdvance(OP_EMPTY_SET);
        case TRUE: return terminalAndAdvance(OP_TRUE);
        case FALSE: return terminalAndAdvance(OP_FALSE);
        case STRING: return terminalAndAdvance(OP_STRING);
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
        case LENGTH: return length();
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

        default:
            return error(EXPECTED_PRIMARY);
    }
}

Parser::ParseNode Parser::parenGrouping() noexcept{
    Typeset::Marker left = lMark();
    advance();
    match(NEWLINE);
    ParseNode nested = disjunction();
    if(peek(RIGHTPAREN)){
        Typeset::Marker right = rMark();
        Typeset::Selection sel(left, right);
        registerGrouping(sel);
        advance();
        return parse_tree.addUnary(OP_GROUP_PAREN, sel, nested);
    }

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_LIST);
    builder.addNaryChild(nested);
    do{
        consume(COMMA);
        match(NEWLINE);
        builder.addNaryChild(disjunction());
        match(NEWLINE);
    } while(!peek(RIGHTPAREN) && noErrors());
    Typeset::Selection sel(left, rMark());
    if(noErrors()){
        registerGrouping(sel);
        advance();
    }

    ParseNode list = builder.finalize(sel);

    if(!match(MAPSTO)) return list;

    ParseNode expr = expression();
    sel.right = rMarkPrev();

    return parse_tree.addTernary(
                OP_LAMBDA,
                sel,
                ParseTree::EMPTY,
                list,
                expr
           );
}

Parser::ParseNode Parser::paramList() noexcept{
    Typeset::Marker left = lMarkPrev();

    if(peek(RIGHTPAREN)){
        Typeset::Selection sel(left, rMark());
        registerGrouping(sel);
        advance();
        return parse_tree.addTerminal(OP_LIST, sel);
    }

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_LIST);
    do{
        match(NEWLINE);
        builder.addNaryChild(param());
        match(NEWLINE);
    } while(match(COMMA) && noErrors());
    Typeset::Selection sel(left, rMark());
    consume(RIGHTPAREN);
    if(noErrors()) registerGrouping(sel);

    ParseNode list = builder.finalize(sel);

    if(noErrors())
        return list;
    else
        return parse_tree.addTerminal(OP_LIST, sel);
}

Parser::ParseNode Parser::captureList() noexcept{
    Typeset::Marker left = lMarkPrev();

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_LIST);
    do{
        match(NEWLINE);
        builder.addNaryChild(isolatedIdentifier());
        match(NEWLINE);
    } while(match(COMMA) && noErrors());
    Typeset::Selection sel(left, rMark());
    consume(RIGHTBRACE);

    if(noErrors()){
        registerGrouping(sel);
        ParseNode list = builder.finalize(sel);
        return list;
    }else{
        return parse_tree.addTerminal(OP_LIST, sel);
    }
}

ParseNode Parser::grouping(size_t type, TokenType close) noexcept{
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = disjunction();
    Typeset::Selection sel(left, rMark());
    consume(close);
    if(noErrors()) registerGrouping(sel);

    return parse_tree.addUnary(type, sel, nested);
}

Parser::ParseNode Parser::norm() noexcept{
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
        return parse_tree.addBinary(OP_NORM_p, s, nested, e);
    }else{
        return parse_tree.addUnary(OP_NORM, sel, nested);
    }
}

Parser::ParseNode Parser::innerProduct() noexcept{
    Typeset::Marker left = lMark();
    advance();
    ParseNode lhs = expression();
    consume(BAR);
    ParseNode rhs = expression();
    Typeset::Selection sel(left, rMark());
    if(!match(RIGHTANGLE)) return error(EXPECT_RANGLE);
    registerGrouping(sel);

    return parse_tree.addBinary(OP_INNER_PRODUCT, sel, lhs, rhs);
}

ParseNode Parser::integer() noexcept{
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
            if(!peek(INTEGER)){
                return error(TRAILING_DOT, Typeset::Selection(left, rMarkPrev()));
            }
            const Typeset::Selection sel(left, rMark());
            sel.formatNumber();
            ParseNode decimal = terminalAndAdvance(OP_INTEGER_LITERAL);
            return parse_tree.addBinary(OP_DECIMAL_LITERAL, sel, n, decimal);
        }
        default:
            sel.formatNumber();
            return n;
    }
}

ParseNode Parser::identifier() noexcept{
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);

    switch (currentType()) {
        case LEFTPAREN: return call(id);
        case MAPSTO: return lambda(id);
        case TOKEN_SUBSCRIPT:
            if(parse_tree.str(id) == "e"){
                parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                return oneDim(OP_UNIT_VECTOR);
            }else if(parse_tree.str(id) == "I" && noErrors()){
                size_t index_backup = index;
                ParseNode pn = twoDims(OP_IDENTITY_MATRIX);
                if(noErrors()){
                    parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                    return pn;
                }
                index = index_backup;
                errors.clear();
            }
            return id;
        case TOKEN_DUALSCRIPT:
            if(parse_tree.str(id) == "e"){
                parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                Typeset::Selection c(lMarkPrev(), rMark());
                advance();
                parsing_dims = true;
                ParseNode dim0 = expression();
                consume(TIMES);
                ParseNode dim1 = expression();
                consume(ARGCLOSE);
                ParseNode elem = expression();
                consume(ARGCLOSE);
                ParseNode n = parse_tree.addTernary(OP_UNIT_VECTOR, c, elem, dim0, dim1);
                parsing_dims = false;
                return n;
            }
            return id;
        default: return id;
    }
}

Parser::ParseNode Parser::isolatedIdentifier() noexcept{
    if(!peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL);
    return terminalAndAdvance(OP_IDENTIFIER);
}

Parser::ParseNode Parser::param() noexcept{
    if(!peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL);
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
    return match(EQUALS) ? parse_tree.addBinary(OP_EQUAL, id, expression()) : id;
}

ParseNode Parser::call(const ParseNode& id) noexcept{
    Typeset::Marker lmark = lMark();

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_CALL);
    builder.addNaryChild(id);
    advance();
    if(!peek(RIGHTPAREN)){
        builder.addNaryChild( expression() );
        while(noErrors() && !peek(RIGHTPAREN)){
            consume(COMMA);
            builder.addNaryChild( expression() );
        }
    }

    Typeset::Marker rmark = rMark();
    registerGrouping(Typeset::Selection(lmark, rmark));

    size_t n = builder.finalize(rmark);
    if(noErrors()) advance();

    return n;
}

Parser::ParseNode Parser::lambda(const ParseNode& id) noexcept{
    advance();

    Typeset::Marker left = parse_tree.getLeft(id);

    ParseNode capture_list = ParseTree::EMPTY;
    ParseNode referenced_upvalues = ParseTree::EMPTY;
    ParseNode e = expression();
    if(!noErrors()) return e;

    Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addQuadary(
                OP_LAMBDA,
                sel,
                capture_list,
                referenced_upvalues,
                parse_tree.addUnary(OP_LIST, id),
                e
           );
}

ParseNode Parser::fraction() noexcept{
    Typeset::Selection c = selection();
    advance();

    ParseNode num = expression();
    consume(ARGCLOSE);
    ParseNode den = expression();
    consume(ARGCLOSE);

    return parse_tree.addBinary(OP_FRACTION, c, num, den);
}

Parser::ParseNode Parser::binomial() noexcept{
    Typeset::Selection c = selection();
    advance();

    ParseNode n = expression();
    consume(ARGCLOSE);
    ParseNode k = expression();
    consume(ARGCLOSE);

    return parse_tree.addBinary(OP_BINOMIAL, c, n, k);
}

ParseNode Parser::superscript(const ParseNode& lhs) noexcept{
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
        case MULTIPLY:{
            parse_tree.setRight(lhs, right);
            advance();
            n = lhs;
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
        default: n = parse_tree.addBinary(OP_POWER, c, lhs, expression());
    }

    consume(ARGCLOSE);

    return n;
}

Parser::ParseNode Parser::subscript(const ParseNode& lhs, const Typeset::Marker& right) noexcept{
    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Selection selection(left, right);
    advance();

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_SUBSCRIPT_ACCESS);
    builder.addNaryChild(lhs);
    do{ builder.addNaryChild(subExpr()); } while(match(COMMA));

    consume(ARGCLOSE);

    return builder.finalize(selection);
}

Parser::ParseNode Parser::dualscript(const ParseNode& lhs) noexcept{
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
            return parse_tree.addBinary(OP_POWER, c, subscript(lhs, right), power);
    }
}

Parser::ParseNode Parser::subExpr() noexcept{
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

    if(!match(COLON)) return parse_tree.addBinary(OP_SLICE, first, second);
    else return parse_tree.addTernary(OP_SLICE, first, second, expression());
}

ParseNode Parser::matrix() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();
    if(argc == 1) return error(SCALAR_MATRIX, c);

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_MATRIX);

    for(size_t i = 0; i < argc; i++){
        builder.addNaryChild( expression() );
        consume(ARGCLOSE);
    }

    ParseNode m = builder.finalize(c);
    parse_tree.setFlag(m, c.getMatrixRows());

    return m;
}

Parser::ParseNode Parser::cases() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(OP_CASES);

    for(size_t i = 0; i < argc; i++){
        builder.addNaryChild( disjunction() );
        consume(ARGCLOSE);
    }

    return builder.finalize(c);
}

Parser::ParseNode Parser::squareRoot() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    ParseNode n = parse_tree.addUnary(OP_SQRT, c, expression());
    consume(ARGCLOSE);

    return n;
}

Parser::ParseNode Parser::nRoot() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    ParseNode base = expression();
    consume(ARGCLOSE);
    ParseNode arg = expression();
    consume(ARGCLOSE);

    return parse_tree.addBinary(OP_ROOT, c, arg, base);
}

Parser::ParseNode Parser::oneDim(Op type) noexcept{
    Typeset::Selection c(lMarkPrev(), rMark());
    advance();
    parsing_dims = true;
    ParseNode pn = parse_tree.addUnary(type, c, expression());
    consume(ARGCLOSE);
    parsing_dims = false;

    return pn;
}

Parser::ParseNode Parser::twoDims(Op type) noexcept{
    Typeset::Selection c(lMarkPrev(), rMark());
    advance();
    parsing_dims = true;
    ParseNode lhs = expression();
    consume(TIMES);
    ParseNode pn = parse_tree.addBinary(type, c, lhs, expression());
    consume(ARGCLOSE);
    parsing_dims = false;

    return pn;
}

Parser::ParseNode Parser::length() noexcept{
    Typeset::Marker left = lMark();
    advance();
    Typeset::Marker lparen_mark = lMark();
    consume(LEFTPAREN);
    ParseNode arg = expression();
    Typeset::Marker right = rMark();
    consume(RIGHTPAREN);
    registerGrouping(Typeset::Selection(lparen_mark, right));

    if(!noErrors()) return error_node;
    return parse_tree.addUnary(OP_LENGTH, Typeset::Selection(left, right), arg);
}

Parser::ParseNode Parser::trig(Op type) noexcept{
    const Typeset::Marker& left = lMark();
    advance();
    if(match(TOKEN_SUPERSCRIPT)){
        ParseNode power = expression();
        consume(ARGCLOSE);
        ParseNode fn = parse_tree.addLeftUnary(type, left, leftUnary());
        const Typeset::Marker& right = parse_tree.getRight(fn);
        Typeset::Selection c(left, right);
        if(parse_tree.getOp(power) == OP_UNARY_MINUS)
            return error(AMBIGIOUS_TRIG_POWER, c);
        return parse_tree.addBinary(OP_POWER, c, fn, power);
    }

    return parse_tree.addLeftUnary(type, left, leftUnary());
}

Parser::ParseNode Parser::log() noexcept{
const Typeset::Marker& left = lMark();
    advance();
    if(match(TOKEN_SUBSCRIPT)){
        ParseNode base = expression();
        consume(ARGCLOSE);
        ParseNode arg = leftUnary();
        const Typeset::Marker& right = parse_tree.getRight(arg);
        Typeset::Selection c(left, right);
        return parse_tree.addBinary(OP_LOGARITHM_BASE, c, arg, base);
    }

    return parse_tree.addLeftUnary(OP_LOGARITHM, left, leftUnary());
}

Parser::ParseNode Parser::oneArg(Op type) noexcept{
    const Typeset::Marker& left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode e = expression();
    const Typeset::Marker& right = rMark();
    consume(RIGHTPAREN);
    Typeset::Selection c(left, right);

    return parse_tree.addUnary(type, c, e);
}

Parser::ParseNode Parser::twoArgs(Op type) noexcept{
    const Typeset::Marker& left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode a = expression();
    consume(COMMA);
    ParseNode b = expression();
    const Typeset::Marker& right = rMark();
    consume(RIGHTPAREN);
    Typeset::Selection c(left, right);

    return parse_tree.addBinary(type, c, a, b);
}

Parser::ParseNode Parser::big(Op type) noexcept{
    const Typeset::Marker& left = lMark();
    Typeset::Selection err_sel(left, rMark());
    advance();
    ParseNode end = expression();
    consume(ARGCLOSE);
    if(!noErrors() || !peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode id = terminalAndAdvance(OP_IDENTIFIER);
    if(!match(EQUALS) && !match(LEFTARROW)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode start = expression();
    ParseNode assign = parse_tree.addBinary(OP_ASSIGN, id, start);
    consume(ARGCLOSE);
    ParseNode body = expression();
    if(!noErrors()) return error_node;
    const Typeset::Marker& right = rMarkPrev();
    Typeset::Selection sel(left, right);

    return parse_tree.addTernary(type, sel, assign, end, body);
}

Parser::ParseNode Parser::oneArgConstruct(Op type) noexcept{
    Typeset::Selection sel(lMark(), rMark());
    advance();
    ParseNode child = disjunction();
    consume(ARGCLOSE);
    return parse_tree.addUnary(type, sel, child);
}

ParseNode Parser::error(ErrorCode code){
    return error(code, selection());
}

Parser::ParseNode Parser::error(ErrorCode code, const Typeset::Selection& c){
    if(noErrors()){
        error_node = parse_tree.addTerminal(ERROR, c);
        errors.push_back(Error(c, code));
    }

    return error_node;
}

void Parser::advance() noexcept{
    index++;
}

bool Parser::match(TokenType type) noexcept{
    if(tokens[index] == type){
        advance();
        return true;
    }else{
        return false;
    }
}

bool Parser::peek(TokenType type) const noexcept{
    return tokens[index] == type;
}

void Parser::require(TokenType type) noexcept{
    if(tokens[index] != type) error(CONSUME);
}

void Parser::consume(TokenType type) noexcept{
    if(tokens[index] == type){
        advance();
    }else{
        error(CONSUME);
    }
}

void Parser::skipNewlines() noexcept{
    while(tokens[index] == NEWLINE) index++;
}

TokenType Parser::currentType() const noexcept{
    return tokens[index];
}

Parser::ParseNode Parser::makeTerminalNode(size_t type) noexcept{
    return parse_tree.addTerminal(type, selection());
}

ParseNode Parser::terminalAndAdvance(size_t type) noexcept{
    ParseNode pn = parse_tree.addTerminal(type, selection());
    index++;

    return pn;
}

const Typeset::Selection Parser::selection() const noexcept{
    return Typeset::Selection(markers[index]);
}

const Typeset::Selection Parser::selectionPrev() const noexcept{
    return Typeset::Selection(markers[index-1]);
}

const Typeset::Marker& Parser::lMark() const noexcept{
    return markers[index].first;
}

const Typeset::Marker& Parser::rMark() const noexcept{
    return markers[index].second;
}

const Typeset::Marker& Parser::lMarkPrev() const noexcept{
    return markers[index-1].first;
}

const Typeset::Marker& Parser::rMarkPrev() const noexcept{
    return markers[index-1].second;
}

}

}
