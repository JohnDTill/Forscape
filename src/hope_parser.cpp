#include "hope_parser.h"

#include <code_parsenodetype.h>
#include "typeset_model.h"

namespace Hope {

namespace Code {

Parser::Parser(const Scanner &scanner, Typeset::Model* model)
    : tokens(scanner.tokens), markers(scanner.markers), errors(model->errors), model(model) {
    //DO NOTHING
}

void Parser::parseAll(){
    parse_tree.clear();
    index = 0;
    parsing_dims = false;
    loops = 0;
    error_node = UNITIALIZED;

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_BLOCK);
    skipNewlines();
    while (!peek(ENDOFFILE)) {
        builder.addNaryChild( checkedStatement() );
        skipNewlines();
    }

    Typeset::Selection c(markers.front().first, markers.back().second);
    builder.finalize(c);
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
        case BREAK: return loops ? terminalAndAdvance(PN_BREAK) : error(BAD_BREAK);
        case CONTINUE: return loops ? terminalAndAdvance(PN_CONTINUE) : error(BAD_CONTINUE);
        default: return mathStatement();
    }
}

ParseNode Parser::ifStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode condition = disjunction();
    consume(RIGHTPAREN);
    ParseNode body = blockStatement();
    if(match(ELSE)){
        size_t el = blockStatement();
        Typeset::Marker right = rMark();
        Typeset::Selection c(left, right);

        return parse_tree.addTernary(PN_IF_ELSE, c, condition, body, el);
    }else{
        Typeset::Marker right = rMark();
        Typeset::Selection c(left, right);

        return parse_tree.addBinary(PN_IF, c, condition, body);
    }
}

Parser::ParseNode Parser::whileStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode condition = disjunction();
    consume(RIGHTPAREN);
    loops++;
    ParseNode body = blockStatement();
    loops--;
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);

    return parse_tree.addBinary(PN_WHILE, c, condition, body);
}

Parser::ParseNode Parser::forStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode initializer = peek(SEMICOLON) ?
                            makeTerminalNode(PN_BLOCK) :
                            statement();
    consume(SEMICOLON);
    ParseNode condition = peek(SEMICOLON) ?
                          makeTerminalNode(PN_TRUE) :
                          disjunction();
    consume(SEMICOLON);
    ParseNode update = peek(RIGHTPAREN) ?
                       makeTerminalNode(PN_BLOCK) :
                       statement();
    consume(RIGHTPAREN);
    loops++;
    ParseNode body = blockStatement();
    loops--;
    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);

    return parse_tree.addQuadary(PN_FOR, c, initializer, condition, update, body);
}

ParseNode Parser::printStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    consume(LEFTPAREN);

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_PRINT);

    do{
        builder.addNaryChild(disjunction());
    } while(noErrors() && match(COMMA));

    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);
    consume(RIGHTPAREN);

    return builder.finalize(c);
}

Parser::ParseNode Parser::assertStatement() noexcept{
    Typeset::Marker left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode e = disjunction();

    Typeset::Marker right = rMark();
    Typeset::Selection c(left, right);
    consume(RIGHTPAREN);

    return parse_tree.addUnary(PN_ASSERT, c, e);
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
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_BLOCK);

    skipNewlines();
    while(noErrors() && !match(RIGHTBRACKET)){
        builder.addNaryChild( statement() );
        skipNewlines();
    }

    Typeset::Marker right = lMarkPrev();
    Typeset::Selection c(left, right);

    return builder.finalize(c);
}

Parser::ParseNode Parser::algStatement() noexcept{
    advance();

    ParseNode id = param();

    if(!peek(LEFTPAREN) & !peek(LEFTBRACE))
        return parse_tree.addUnary(PN_PROTOTYPE_ALG, id);

    ParseNode captures = match(LEFTBRACE) ?
                         paramList(RIGHTBRACE) :
                         ParseTree::EMPTY;

    ParseNode referenced_upvalues = ParseTree::EMPTY;

    consume(LEFTPAREN);
    ParseNode params = paramList(RIGHTPAREN);

    size_t loops_backup = 0;
    std::swap(loops_backup, loops);
    ParseNode body = blockStatement();
    std::swap(loops_backup, loops);

    return parse_tree.addPentary(
                PN_ALGORITHM,
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
    return parse_tree.addLeftUnary(PN_RETURN, m, disjunction());
}

ParseNode Parser::mathStatement() noexcept{
    ParseNode n = expression();

    switch (currentType()) {
        case EQUALS: return equality(n);
        case LEFTARROW: return assignment(n);
        default: return parse_tree.addUnary(PN_EXPR_STMT, n);
    }
}

ParseNode Parser::assignment(const ParseNode& lhs) noexcept{
    advance();
    return parse_tree.addBinary(PN_ASSIGN, lhs, expression());
}

ParseNode Parser::expression() noexcept{
    return addition();
}

ParseNode Parser::equality(const ParseNode& lhs) noexcept{
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_EQUAL);
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
            case DISJUNCTION: advance(); n = parse_tree.addBinary(PN_LOGICAL_OR, n, conjunction()); break;
            default: return n;
        }
    }
}

Parser::ParseNode Parser::conjunction() noexcept{
    ParseNode n = comparison();

    for(;;){
        switch (currentType()) {
            case CONJUNCTION: advance(); n = parse_tree.addBinary(PN_LOGICAL_AND, n, comparison()); break;
            default: return n;
        }
    }
}

Parser::ParseNode Parser::comparison() noexcept{
    ParseNode n = addition();

    for(;;){
        switch (currentType()) {
            case EQUALS: advance(); n = parse_tree.addBinary(PN_EQUAL, n, addition()); break;
            case NOTEQUAL: advance(); n = parse_tree.addBinary(PN_NOT_EQUAL, n, addition()); break;
            case LESS: advance(); n = parse_tree.addBinary(PN_LESS, n, addition()); break;
            case LESSEQUAL: advance(); n = parse_tree.addBinary(PN_LESS_EQUAL, n, addition()); break;
            case GREATER: advance(); n = parse_tree.addBinary(PN_GREATER, n, addition()); break;
            case GREATEREQUAL: advance(); n = parse_tree.addBinary(PN_GREATER_EQUAL, n, addition()); break;
            default: return n;
        }
    }
}

ParseNode Parser::addition() noexcept{
    ParseNode n = multiplication();

    for(;;){
        switch (currentType()) {
            case PLUS: advance(); n = parse_tree.addBinary(PN_ADDITION, n, multiplication()); break;
            case MINUS: advance(); n = parse_tree.addBinary(PN_SUBTRACTION, n, multiplication()); break;
            default: return n;
        }
    }
}

ParseNode Parser::multiplication() noexcept{
    ParseNode n = leftUnary();

    for(;;){
        switch (currentType()) {
            case MULTIPLY: advance(); n = parse_tree.addBinary(PN_MULTIPLICATION, n, leftUnary()); break;
            case DIVIDE: advance(); n = parse_tree.addBinary(PN_DIVIDE, n, leftUnary()); break;
            case FORWARDSLASH: advance(); n = parse_tree.addBinary(PN_FORWARDSLASH, n, leftUnary()); break;
            case BACKSLASH: advance(); n = parse_tree.addBinary(PN_BACKSLASH, n, leftUnary()); break;
            case TIMES: if(parsing_dims) return n; advance(); n = parse_tree.addBinary(PN_CROSS, n, leftUnary()); break;
            case DOTPRODUCT: advance(); n = parse_tree.addBinary(PN_DOT, n, leftUnary()); break;
            case PERCENT: advance(); n = parse_tree.addBinary(PN_MODULUS, n, leftUnary()); break;
            case OUTERPRODUCT: advance(); n = parse_tree.addBinary(PN_OUTER_PRODUCT, n, leftUnary()); break;
            case ODOT: advance(); n = parse_tree.addBinary(PN_ODOT, n, leftUnary()); break;
            default: return n;
        }
    }
}

ParseNode Parser::leftUnary() noexcept{
    switch (currentType()) {
        case MINUS:{
            Typeset::Marker m = lMark();
            advance();
            return parse_tree.addLeftUnary(PN_UNARY_MINUS, m, implicitMult());
        }
        case NOT:{
            Typeset::Marker m = lMark();
            advance();
            return parse_tree.addLeftUnary(PN_LOGICAL_NOT, m, implicitMult());
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
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_IMPLICIT_MULTIPLY);
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
                return parse_tree.addRightUnary(PN_FACTORIAL, m, n);
            }
            case CARET: advance(); return parse_tree.addBinary(PN_POWER, n, implicitMult());
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
        case INFTY: return terminalAndAdvance(PN_INFTY);
        case EMPTYSET: return terminalAndAdvance(PN_EMPTY_SET);
        case TRUE: return terminalAndAdvance(PN_TRUE);
        case FALSE: return terminalAndAdvance(PN_FALSE);
        case STRING: return terminalAndAdvance(PN_STRING);
        case GRAVITY: return terminalAndAdvance(PN_GRAVITY);

        //Grouping
        case LEFTPAREN: return parenGrouping();
        case LEFTCEIL: return grouping(PN_CEIL, RIGHTCEIL);
        case LEFTFLOOR: return grouping(PN_FLOOR, RIGHTFLOOR);
        case BAR: return grouping(PN_ABS, BAR);
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

        case TOKEN_BIGSUM2: return big(PN_SUMMATION);
        case TOKEN_BIGPROD2: return big(PN_PRODUCT);

        case TOKEN_ACCENTHAT: return oneArgConstruct(PN_ACCENT_HAT);

        //Keyword funcs
        case LENGTH: return length();
        case SIN: return trig(PN_SINE);
        case COS: return trig(PN_COSINE);
        case TAN: return trig(PN_TANGENT);
        case ARCSIN: return trig(PN_ARCSINE);
        case ARCCOS: return trig(PN_ARCCOSINE);
        case ARCTAN: return trig(PN_ARCTANGENT);
        case ARCTAN2: return twoArgs(PN_ARCTANGENT2);
        case CSC: return trig(PN_COSECANT);
        case SEC: return trig(PN_SECANT);
        case COT: return trig(PN_COTANGENT);
        case ARCCSC: return trig(PN_ARCCOSECANT);
        case ARCSEC: return trig(PN_ARCSECANT);
        case ARCCOT: return trig(PN_ARCCOTANGENT);
        case SINH: return trig(PN_HYPERBOLIC_SINE);
        case COSH: return trig(PN_HYPERBOLIC_COSINE);
        case TANH: return trig(PN_HYPERBOLIC_TANGENT);
        case ARCSINH: return trig(PN_HYPERBOLIC_ARCSINE);
        case ARCCOSH: return trig(PN_HYPERBOLIC_ARCCOSINE);
        case ARCTANH: return trig(PN_HYPERBOLIC_ARCTANGENT);
        case CSCH: return trig(PN_HYPERBOLIC_COSECANT);
        case SECH: return trig(PN_HYPERBOLIC_SECANT);
        case COTH: return trig(PN_HYPERBOLIC_COTANGENT);
        case ARCCSCH: return trig(PN_HYPERBOLIC_ARCCOSECANT);
        case ARCSECH: return trig(PN_HYPERBOLIC_ARCSECANT);
        case ARCCOTH: return trig(PN_HYPERBOLIC_ARCCOTANGENT);
        case EXP: return oneArg(PN_EXP);
        case NATURALLOG: return oneArg(PN_NATURAL_LOG);
        case LOG: return log();
        case ERRORFUNCTION: return oneArg(PN_ERROR_FUNCTION);
        case COMPERRFUNC: return oneArg(PN_COMP_ERR_FUNC);

        default:
            return error(EXPECTED_PRIMARY);
    }
}

Parser::ParseNode Parser::parenGrouping() noexcept{
    Typeset::Marker left = lMark();
    advance();
    match(NEWLINE);
    ParseNode nested = expression();
    if(peek(RIGHTPAREN)){
        Typeset::Marker right = rMark();
        advance();
        return parse_tree.addUnary(PN_GROUP_PAREN, Typeset::Selection(left, right), nested);
    }

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_LIST);
    builder.addNaryChild(nested);
    do{
        consume(COMMA);
        match(NEWLINE);
        builder.addNaryChild(expression());
        match(NEWLINE);
    } while(!peek(RIGHTPAREN) && noErrors());
    Typeset::Marker right = rMark();
    if(noErrors()) advance();

    ParseNode list = builder.finalize(Typeset::Selection(left, right));

    if(!match(MAPSTO)) return list;

    ParseNode expr = expression();
    Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addTernary(
                PN_LAMBDA,
                sel,
                ParseTree::EMPTY,
                list,
                expr
           );
}

Parser::ParseNode Parser::paramList(TokenType close) noexcept{
    Typeset::Marker left = lMarkPrev();

    if(peek(RIGHTPAREN)){
        Typeset::Marker right = rMark();
        advance();
        return parse_tree.addTerminal(PN_LIST, Typeset::Selection(left, right));
    }

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_LIST);
    do{
        match(NEWLINE);
        builder.addNaryChild(param());
        match(NEWLINE);
    } while(match(COMMA) && noErrors());
    Typeset::Marker right = rMark();
    consume(close);

    ParseNode list = builder.finalize(Typeset::Selection(left, right));

    if(noErrors())
        return list;
    else
        return parse_tree.addTerminal(PN_LIST, Typeset::Selection(left, right));
}

ParseNode Parser::grouping(size_t type, TokenType close) noexcept{
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = disjunction();
    Typeset::Marker right = rMark();
    Typeset::Selection s(left, right);
    consume(close);

    return parse_tree.addUnary(type, s, nested);
}

Parser::ParseNode Parser::norm() noexcept{
    Typeset::Marker left = lMark();
    advance();
    ParseNode nested = expression();
    Typeset::Marker right = rMark();
    Typeset::Selection s(left, right);
    consume(DOUBLEBAR);

    if(match(TOKEN_SUBSCRIPT)){
        Typeset::Marker right = rMarkPrev();
        Typeset::Selection s(left, right);
        if(match(INFTY)){
            consume(ARGCLOSE);
            return parse_tree.addUnary(PN_NORM_INFTY, s, nested);
        }else if(peek(INTEGER) && rMark().index - lMark().index == 1){
            if(lMark().charRight() == 1){
                advance();
                consume(ARGCLOSE);
                return parse_tree.addUnary(PN_NORM_1, s, nested);
            }else if(lMark().charRight() == 2){
                advance();
                consume(ARGCLOSE);
                return parse_tree.addUnary(PN_NORM, s, nested);
            }
        }
        ParseNode e = expression();
        consume(ARGCLOSE);
        return parse_tree.addBinary(PN_NORM_p, s, nested, e);
    }else{
        return parse_tree.addUnary(PN_NORM, s, nested);
    }
}

Parser::ParseNode Parser::innerProduct() noexcept{
    Typeset::Marker left = lMark();
    advance();
    ParseNode lhs = expression();
    consume(BAR);
    ParseNode rhs = expression();
    consume(RIGHTANGLE);

    return parse_tree.addBinary(PN_INNER_PRODUCT, Typeset::Selection(left, rMarkPrev()), lhs, rhs);
}

ParseNode Parser::integer() noexcept{
    const Typeset::Marker& left = lMark();
    const Typeset::Selection& sel = selection();

    if(left.charRight() == '0' &&
       rMark().index - left.index > 1) return error(LEADING_ZEROES);

    ParseNode n = terminalAndAdvance(PN_INTEGER_LITERAL);
    switch (currentType()) {
        case TOKEN_SUBSCRIPT:{
            sel.format(SEM_PREDEFINEDMATNUM);
            if(sel.strView() == "0")
                return twoDims(PN_ZERO_MATRIX);
            else if(sel.strView() == "1")
                return twoDims(PN_ONES_MATRIX);
            else return error(UNRECOGNIZED_SYMBOL);
        }
        case PERIOD:{
            advance();
            if(!peek(INTEGER)){
                return error(TRAILING_DOT, Typeset::Selection(left, rMarkPrev()));
            }
            const Typeset::Selection sel(left, rMark());
            sel.formatNumber();
            ParseNode decimal = terminalAndAdvance(PN_INTEGER_LITERAL);
            return parse_tree.addBinary(PN_DECIMAL_LITERAL, sel, n, decimal);
        }
        default:
            sel.formatNumber();
            return n;
    }
}

ParseNode Parser::identifier() noexcept{
    ParseNode id = terminalAndAdvance(PN_IDENTIFIER);

    switch (currentType()) {
        case LEFTPAREN: return call(id);
        case MAPSTO: return lambda(id);
        case TOKEN_SUBSCRIPT:
            if(parse_tree.str(id) == "e"){
                parse_tree.getSelection(id).format(SEM_PREDEFINEDMAT);
                return oneDim(PN_UNIT_VECTOR);
            }else if(parse_tree.str(id) == "I" && noErrors()){
                size_t index_backup = index;
                ParseNode pn = twoDims(PN_IDENTITY_MATRIX);
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
                ParseNode n = parse_tree.addTernary(PN_UNIT_VECTOR, c, elem, dim0, dim1);
                parsing_dims = false;
                return n;
            }
            return id;
        default: return id;
    }
}

Parser::ParseNode Parser::param() noexcept{
    if(!peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL);
    ParseNode id = terminalAndAdvance(PN_IDENTIFIER);
    return match(EQUALS) ?
           parse_tree.addBinary(PN_EQUAL, id, expression()) :
           id;
}

ParseNode Parser::call(const ParseNode& id) noexcept{
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_CALL);
    builder.addNaryChild(id);
    advance();
    if(!peek(RIGHTPAREN)){
        builder.addNaryChild( expression() );
        while(noErrors() && !peek(RIGHTPAREN)){
            consume(COMMA);
            builder.addNaryChild( expression() );
        }
    }

    size_t n = builder.finalize(rMark());
    if(noErrors()) advance();

    return n;
}

Parser::ParseNode Parser::lambda(const ParseNode& id) noexcept{
    advance();

    Typeset::Marker left = parse_tree.getLeft(id);

    ParseNode referenced_upvalues = ParseTree::EMPTY;
    ParseNode e = expression();
    if(!noErrors()) return e;

    Typeset::Selection sel(left, rMarkPrev());

    return parse_tree.addTernary(
                PN_LAMBDA,
                sel,
                referenced_upvalues,
                parse_tree.addUnary(PN_LIST, id),
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

    return parse_tree.addBinary(PN_FRACTION, c, num, den);
}

Parser::ParseNode Parser::binomial() noexcept{
    Typeset::Selection c = selection();
    advance();

    ParseNode n = expression();
    consume(ARGCLOSE);
    ParseNode k = expression();
    consume(ARGCLOSE);

    return parse_tree.addBinary(PN_BINOMIAL, c, n, k);
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
            n = parse_tree.addUnary(PN_TRANSPOSE, c, lhs);
            break;
        }
        case DAGGER:{
            advance();
            n = parse_tree.addUnary(PN_DAGGER, c, lhs);
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
            n = parse_tree.addUnary(PN_PSEUDO_INVERSE, c, lhs);
            break;
        }
        case CONJUNCTION:
        case CARET:{
            advance();
            n = parse_tree.addUnary(PN_ACCENT_HAT, c, lhs);
            break;
        }
        default: n = parse_tree.addBinary(PN_POWER, c, lhs, expression());
    }

    consume(ARGCLOSE);

    return n;
}

Parser::ParseNode Parser::subscript(const ParseNode& lhs, const Typeset::Marker& right) noexcept{
    Typeset::Marker left = parse_tree.getLeft(lhs);
    Typeset::Selection selection(left, right);
    advance();

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_SUBSCRIPT_ACCESS);
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
            return parse_tree.addUnary(PN_TRANSPOSE, c, subscript(lhs, right));
        }
        case DAGGER:{
            advance();
            require(ARGCLOSE);
            return parse_tree.addUnary(PN_DAGGER, c, subscript(lhs, right));
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
            return parse_tree.addUnary(PN_PSEUDO_INVERSE, c, subscript(lhs, right));
        }
        default:
            ParseNode power = expression();
            require(ARGCLOSE);
            return parse_tree.addBinary(PN_POWER, c, subscript(lhs, right), power);
    }
}

Parser::ParseNode Parser::subExpr() noexcept{
    ParseNode first = peek(COLON) ?
                      parse_tree.addTerminal(PN_SLICE_ALL, selection()) :
                      expression();

    if(!match(COLON))
        return first; //Simple subscript
    else if(peek(COMMA) || peek(ARGCLOSE))
        return parse_tree.addUnary(PN_SLICE, first); //x_:,1 access

    ParseNode second = peek(COLON) ?
                       parse_tree.addTerminal(PN_SLICE_ALL, selection()) :
                       expression();

    if(!match(COLON)) return parse_tree.addBinary(PN_SLICE, first, second);
    else return parse_tree.addTernary(PN_SLICE, first, second, expression());
}

ParseNode Parser::matrix() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();
    if(argc == 1) return error(SCALAR_MATRIX, c);

    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_MATRIX);

    for(size_t i = 0; i < argc; i++){
        builder.addNaryChild( expression() );
        consume(ARGCLOSE);
    }

    return builder.finalize(c);
}

Parser::ParseNode Parser::cases() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    size_t argc = c.getConstructArgSize();
    ParseTree::NaryBuilder builder = parse_tree.naryBuilder(PN_CASES);

    for(size_t i = 0; i < argc; i++){
        builder.addNaryChild( disjunction() );
        consume(ARGCLOSE);
    }

    return builder.finalize(c);
}

Parser::ParseNode Parser::squareRoot() noexcept{
    const Typeset::Selection& c = selection();
    advance();
    ParseNode n = parse_tree.addUnary(PN_SQRT, c, expression());
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

    return parse_tree.addBinary(PN_ROOT, c, arg, base);
}

Parser::ParseNode Parser::oneDim(ParseNodeType type) noexcept{
    Typeset::Selection c(lMarkPrev(), rMark());
    advance();
    parsing_dims = true;
    ParseNode pn = parse_tree.addUnary(type, c, expression());
    consume(ARGCLOSE);
    parsing_dims = false;

    return pn;
}

Parser::ParseNode Parser::twoDims(ParseNodeType type) noexcept{
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
    consume(LEFTPAREN);
    ParseNode arg = expression();
    Typeset::Marker right = rMark();
    consume(RIGHTPAREN);

    if(!noErrors()) return error_node;
    return parse_tree.addUnary(PN_LENGTH, Typeset::Selection(left, right), arg);
}

Parser::ParseNode Parser::trig(ParseNodeType type) noexcept{
    const Typeset::Marker& left = lMark();
    advance();
    if(match(TOKEN_SUPERSCRIPT)){
        ParseNode power = expression();
        consume(ARGCLOSE);
        ParseNode fn = parse_tree.addLeftUnary(type, left, leftUnary());
        const Typeset::Marker& right = parse_tree.getRight(fn);
        Typeset::Selection c(left, right);
        if(parse_tree.getType(power) == PN_UNARY_MINUS)
            return error(AMBIGIOUS_TRIG_POWER, c);
        return parse_tree.addBinary(PN_POWER, c, fn, power);
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
        return parse_tree.addBinary(PN_LOGARITHM_BASE, c, arg, base);
    }

    return parse_tree.addLeftUnary(PN_LOGARITHM, left, leftUnary());
}

Parser::ParseNode Parser::oneArg(ParseNodeType type) noexcept{
    const Typeset::Marker& left = lMark();
    advance();
    consume(LEFTPAREN);
    ParseNode e = expression();
    const Typeset::Marker& right = rMark();
    consume(RIGHTPAREN);
    Typeset::Selection c(left, right);

    return parse_tree.addUnary(type, c, e);
}

Parser::ParseNode Parser::twoArgs(ParseNodeType type) noexcept{
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

Parser::ParseNode Parser::big(ParseNodeType type) noexcept{
    const Typeset::Marker& left = lMark();
    Typeset::Selection err_sel(left, rMark());
    advance();
    ParseNode end = expression();
    consume(ARGCLOSE);
    if(!noErrors() || !peek(IDENTIFIER)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode id = terminalAndAdvance(PN_IDENTIFIER);
    if(!match(EQUALS) && !match(LEFTARROW)) return error(UNRECOGNIZED_SYMBOL, err_sel);
    ParseNode start = expression();
    ParseNode assign = parse_tree.addBinary(PN_ASSIGN, id, start);
    consume(ARGCLOSE);
    ParseNode body = expression();
    if(!noErrors()) return error_node;
    const Typeset::Marker& right = rMarkPrev();
    Typeset::Selection sel(left, right);

    return parse_tree.addTernary(type, sel, assign, end, body);
}

Parser::ParseNode Parser::oneArgConstruct(ParseNodeType type) noexcept{
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
