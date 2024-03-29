#ifndef FORSCAPE_PARSER_H
#define FORSCAPE_PARSER_H

#include "forscape_error.h"
#include "forscape_parse_tree.h"
#include "forscape_scanner.h"
#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Code {

class Parser {
public:
    Parser(const Scanner& scanner, Typeset::Model* model) noexcept;
    void parseAll() alloc_except;
    ParseTree parse_tree;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    FORSCAPE_UNORDERED_MAP<Typeset::Marker, Typeset::Marker> open_symbols;
    FORSCAPE_UNORDERED_MAP<Typeset::Marker, Typeset::Marker> close_symbols;
    #endif

private:
    void reset() noexcept;
    void registerGrouping(const Typeset::Selection& sel) alloc_except;
    void registerGrouping(const Typeset::Marker& l, const Typeset::Marker& r) alloc_except;
    ParseNode checkedStatement() alloc_except;
    ParseNode statement() alloc_except;
    ParseNode ifStatement() alloc_except;
    ParseNode whileStatement() alloc_except;
    ParseNode forStatement() alloc_except;
    ParseNode rangedFor(Typeset::Marker stmt_left, Typeset::Marker paren_left, ParseNode initialiser) alloc_except;
    ParseNode enumStatement() alloc_except;
    ParseNode settingsStatement() alloc_except;
    ParseNode switchStatement() alloc_except;
    ParseNode printStatement() alloc_except;
    ParseNode assertStatement() alloc_except;
    ParseNode blockStatement() alloc_except;
    ParseNode lexicalScopeStatement() alloc_except;
    ParseNode algStatement() alloc_except;
    ParseNode returnStatement() alloc_except;
    ParseNode plotStatement() alloc_except;
    ParseNode importStatement() alloc_except;
    ParseNode fromStatement() alloc_except;
    ParseNode filename() alloc_except;
    ParseNode namespaceStatement() alloc_except; //EVENTUALLY: namespaces don't serve much purpose with modules
    ParseNode unknownsStatement() alloc_except;
    ParseNode classStatement() alloc_except;
    ParseNode mathStatement() alloc_except;
    ParseNode namedLambdaStmt(ParseNode call) alloc_except;
    ParseNode assignment(ParseNode lhs) alloc_except;
    ParseNode expression() alloc_except;
    ParseNode equality(ParseNode lhs) alloc_except;
    ParseNode disjunction() alloc_except;
    ParseNode conjunction() alloc_except;
    ParseNode comparison() alloc_except;
    ParseNode less(ParseNode first, size_t flag) alloc_except;
    ParseNode greater(ParseNode first, size_t flag) alloc_except;
    ParseNode addition() alloc_except;
    ParseNode multiplication() alloc_except;
    ParseNode leftUnary() alloc_except;
    ParseNode implicitMult() alloc_except;
    ParseNode collectImplicitMult(ParseNode n) alloc_except;
    ParseNode call_or_mult(ParseNode n) alloc_except;
    ParseNode rightUnary() alloc_except;
    ParseNode rightUnary(ParseNode n) alloc_except;
    ParseNode primary() alloc_except;
    ParseNode string() alloc_except;
    ParseNode braceGrouping() alloc_except;
    ParseNode parenGrouping() alloc_except;
    ParseNode paramList() alloc_except;
    ParseNode captureList() alloc_except;
    ParseNode grouping(size_t type, ForscapeTokenType close) alloc_except;
    ParseNode set() alloc_except;
    template<Op basic, Op positive, Op negative> ParseNode setWithSigns() alloc_except;
    ParseNode generalSet(Op op) alloc_except;
    ParseNode integerRange() alloc_except;
    ParseNode norm() alloc_except;
    ParseNode innerProduct() alloc_except;
    ParseNode integer() alloc_except;
    ParseNode identifier() alloc_except;
    ParseNode identifierFollowOn(ParseNode id) alloc_except;
    ParseNode isolatedIdentifier() alloc_except;
    ParseNode param() alloc_except;
    ParseNode lambda(ParseNode params) alloc_except;
    ParseNode fraction() alloc_except;
    ParseNode fractionDeriv(const Typeset::Selection& c, Op type, ForscapeTokenType tt) alloc_except;
    ParseNode fractionDefault(const Typeset::Selection& c) alloc_except;
    ParseNode binomial() alloc_except;
    ParseNode superscript(ParseNode lhs, Typeset::Marker left) alloc_except;
    ParseNode subscript(ParseNode lhs, const Typeset::Marker& right) alloc_except;
    ParseNode dualscript(ParseNode lhs) alloc_except;
    template<bool first_arg = false> ParseNode subExpr() alloc_except;
    ParseNode matrix() alloc_except;
    ParseNode cases() alloc_except;
    ParseNode squareRoot() alloc_except;
    ParseNode nRoot() alloc_except;
    ParseNode limit() alloc_except;
    ParseNode indefiniteIntegral() alloc_except;
    ParseNode definiteIntegral() alloc_except;
    ParseNode oneDim(Op type) alloc_except;
    ParseNode twoDims(Op type) alloc_except;
    ParseNode trig(Op type) alloc_except;
    ParseNode log() alloc_except;
    ParseNode oneArgRequireParen(Op type) alloc_except;
    ParseNode oneArg(Op type) alloc_except;
    ParseNode twoArgs(Op type) alloc_except;
    ParseNode arg() alloc_except;
    ParseNode big(Op type) alloc_except;
    ParseNode oneArgConstruct(Op type) alloc_except;
    ParseNode error(ErrorCode code) alloc_except;
    ParseNode error(ErrorCode code, Typeset::Selection c) alloc_except;
    void advance() noexcept;
    bool match(ForscapeTokenType type) noexcept;
    bool peek(ForscapeTokenType type) const noexcept;
    bool lookahead(ForscapeTokenType type) const noexcept;
    void require(ForscapeTokenType type) noexcept;
    void consume(ForscapeTokenType type) noexcept;
    void skipNewlines() noexcept;
    void skipNewline() noexcept;
    ForscapeTokenType currentType() const noexcept;
    ParseNode makeTerminalNode(size_t type) alloc_except;
    ParseNode terminalAndAdvance(size_t type) alloc_except;
    const Typeset::Selection selection() const noexcept;
    const Typeset::Selection selectionPrev() const noexcept;
    const Typeset::Marker& lMark() const noexcept;
    const Typeset::Marker& rMark() const noexcept;
    const Typeset::Marker& lMarkPrev() const noexcept;
    const Typeset::Marker& rMarkPrev() const noexcept;
    bool noErrors() const noexcept;
    void recover() noexcept;
    #ifndef FORSCAPE_TYPESET_HEADLESS
    void registerParseNodeRegion(ParseNode pn, size_t token_index) alloc_except;
    void registerParseNodeRegionToPatch(size_t token_index) alloc_except;
    void startPatch() alloc_except;
    void finishPatch(ParseNode pn) noexcept;
    #endif

    //EVENTUALLY: the parser should not assume there is a single Typeset::Model
    std::vector<std::pair<size_t, size_t>> token_map_stack;
    std::vector<size_t> token_stack_frames;
    const std::vector<Token>& tokens;
    ErrorStream& error_stream;
    Typeset::Model* model;
    size_t index = 0;
    bool parsing_dims = false;
    size_t loops = 0;
    ParseNode comment = NONE;
};

}

}

#endif // FORSCAPE_PARSER_H
