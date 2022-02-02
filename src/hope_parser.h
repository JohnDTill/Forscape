#ifndef HOPE_PARSER_H
#define HOPE_PARSER_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "hope_scanner.h"
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

class Parser {
public:
    Parser(const Scanner& scanner, Typeset::Model* model);
    void parseAll();
    ParseTree parse_tree;
    std::unordered_map<Typeset::Marker, Typeset::Marker> open_symbols;
    std::unordered_map<Typeset::Marker, Typeset::Marker> close_symbols;

private:
    //Predominant noexcept behavior requires elimination of dynamic allocation and recursion

    typedef size_t ParseNode;

    void reset() noexcept;
    void registerGrouping(const Typeset::Selection& sel);
    void registerGrouping(const Typeset::Marker& l, const Typeset::Marker& r);
    ParseNode checkedStatement() noexcept;
    ParseNode statement() noexcept;
    ParseNode ifStatement() noexcept;
    ParseNode whileStatement() noexcept;
    ParseNode forStatement() noexcept;
    ParseNode printStatement() noexcept;
    ParseNode assertStatement() noexcept;
    ParseNode blockStatement() noexcept;
    ParseNode algStatement() noexcept;
    ParseNode returnStatement() noexcept;
    ParseNode mathStatement() noexcept;
    ParseNode assignment(const ParseNode& lhs) noexcept;
    ParseNode expression() noexcept;
    ParseNode equality(const ParseNode& lhs) noexcept;
    ParseNode disjunction() noexcept;
    ParseNode conjunction() noexcept;
    ParseNode comparison() noexcept;
    ParseNode addition() noexcept;
    ParseNode multiplication() noexcept;
    ParseNode leftUnary() noexcept;
    ParseNode implicitMult() noexcept;
    ParseNode collectImplicitMult(ParseNode n) noexcept;
    ParseNode rightUnary() noexcept;
    ParseNode primary() noexcept;
    ParseNode parenGrouping() noexcept;
    ParseNode paramList() noexcept;
    ParseNode captureList() noexcept;
    ParseNode grouping(size_t type, TokenType close) noexcept;
    ParseNode norm() noexcept;
    ParseNode innerProduct() noexcept;
    ParseNode integer() noexcept;
    ParseNode identifier() noexcept;
    ParseNode isolatedIdentifier() noexcept;
    ParseNode param() noexcept;
    ParseNode call(const ParseNode& id) noexcept;
    ParseNode lambda(const ParseNode& params) noexcept;
    ParseNode fraction() noexcept;
    ParseNode binomial() noexcept;
    ParseNode superscript(const ParseNode& lhs) noexcept;
    ParseNode subscript(const ParseNode& lhs, const Typeset::Marker& right) noexcept;
    ParseNode dualscript(const ParseNode& lhs) noexcept;
    ParseNode subExpr() noexcept;
    ParseNode matrix() noexcept;
    ParseNode cases() noexcept;
    ParseNode squareRoot() noexcept;
    ParseNode nRoot() noexcept;
    ParseNode oneDim(Op type) noexcept;
    ParseNode twoDims(Op type) noexcept;
    ParseNode length() noexcept;
    ParseNode trig(Op type) noexcept;
    ParseNode log() noexcept;
    ParseNode oneArg(Op type) noexcept;
    ParseNode twoArgs(Op type) noexcept;
    ParseNode big(Op type) noexcept;
    ParseNode oneArgConstruct(Op type) noexcept;
    ParseNode error(ErrorCode code);
    ParseNode error(ErrorCode code, const Typeset::Selection& c);
    void advance() noexcept;
    bool match(TokenType type) noexcept;
    bool peek(TokenType type) const noexcept;
    void require(TokenType type) noexcept;
    void consume(TokenType type) noexcept;
    void skipNewlines() noexcept;
    void skipNewline() noexcept;
    TokenType currentType() const noexcept;
    ParseNode makeTerminalNode(size_t type) noexcept;
    ParseNode terminalAndAdvance(size_t type) noexcept;
    const Typeset::Selection selection() const noexcept;
    const Typeset::Selection selectionPrev() const noexcept;
    const Typeset::Marker& lMark() const noexcept;
    const Typeset::Marker& rMark() const noexcept;
    const Typeset::Marker& lMarkPrev() const noexcept;
    const Typeset::Marker& rMarkPrev() const noexcept;

    const std::vector<TokenType>& tokens;
    const std::vector<std::pair<Typeset::Marker, Typeset::Marker>>& markers;
    std::vector<Error>& errors;
    Typeset::Model* model;
    size_t index = 0;
    bool parsing_dims = false;
    size_t loops = 0;

    static constexpr size_t UNITIALIZED = std::numeric_limits<size_t>::max();
    size_t error_node = UNITIALIZED;

    bool noErrors() const noexcept{
        return error_node == UNITIALIZED;
    }

    void recover() noexcept{
        error_node = UNITIALIZED;
        index = tokens.size()-1; //Give up for now
    }
};

}

}

#endif // HOPE_PARSER_H
