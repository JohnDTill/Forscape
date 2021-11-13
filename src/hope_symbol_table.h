#ifndef HOPE_SYMBOL_TABLE_H
#define HOPE_SYMBOL_TABLE_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include "typeset_selection.h"
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

class ParseTree;
typedef std::unordered_map<Typeset::Marker, std::vector<Typeset::Selection>*> IdMap;

class SymbolTableBuilder{
public:
    SymbolTableBuilder(ParseTree& parse_tree, Typeset::Model* model);
    void resolveSymbols();
    IdMap doc_map;

private:
    static const std::unordered_map<std::string_view, ParseNodeType> predef;

    struct Symbol{
        std::vector<Typeset::Selection>* occurences;
        size_t lexical_depth;
        size_t closure_depth;
        size_t stack_index;
        bool is_const;
        bool is_used = false;
        bool used_as_value = false;
        bool is_reassigned = false;
        bool is_enclosed = false;
        bool is_prototype = false;
        bool is_captured = false;
        bool is_ewise_index = false;

        Symbol()
            : lexical_depth(0) {}

        Symbol(Typeset::Selection first_occurence,
                   size_t lexical_depth,
                   size_t closure_depth,
                   size_t stack_index,
                   bool is_const)
            : lexical_depth(lexical_depth),
              closure_depth(closure_depth),
              stack_index(stack_index),
              is_const(is_const) {
            occurences = new std::vector<Typeset::Selection>();
            occurences->push_back(first_occurence);
        }

        size_t closureIndex() const noexcept{
            assert(closure_depth != 0);
            return closure_depth - 1;
        }
    };

    typedef std::vector<Symbol> Id;
    typedef size_t IdIndex;

    void increaseLexicalDepth();
    void decreaseLexicalDepth();
    void increaseClosureDepth(ParseNode pn);
    void decreaseClosureDepth();

    static constexpr size_t GLOBAL_DEPTH = 0;
    size_t lexical_depth = GLOBAL_DEPTH;

    struct Closure {
        ParseNode fn;
        std::unordered_map<IdIndex, size_t> upvalue_indices;
        size_t num_upvalues = 0;
        std::vector<std::pair<IdIndex, bool> > upvalues;
        std::vector<size_t> captured;

        Closure(){}
        Closure(ParseNode fn) : fn(fn) {}
    };

    size_t getUpvalueIndex(IdIndex value, size_t closure_index);

    std::vector<Closure> closures;
    size_t closureDepth() const noexcept { return closures.size(); }
    size_t closureIndex() const noexcept {
        assert(!closures.empty());
        return closures.size()-1;
    }

    std::vector<Id> ids;
    std::vector<IdIndex> stack;
    std::unordered_map<Typeset::Selection, IdIndex> map;
    std::vector<size_t> stack_sizes;
    std::vector<Error>& errors;

    void finalize(const Symbol& sym_info);

    void makeEntry(const Typeset::Selection& c, bool immutable);
    void appendEntry(size_t index, const Typeset::Selection& c, bool immutable);

    void resolveStmt(ParseNode pn);
    void resolveExpr(ParseNode pn);
    void resolveEquality(ParseNode pn);
    void resolveAssignment(ParseNode pn);
    void resolveAssignmentId(ParseNode pn);
    void resolveAssignmentSubscript(ParseNode pn, ParseNode lhs, ParseNode rhs);
    bool resolvePotentialIdSub(ParseNode pn);
    void resolveReference(ParseNode pn);
    void resolveReference(ParseNode pn, const Typeset::Selection& c, IdIndex id_index);
    void resolveConditional1(ParseNode pn);
    void resolveConditional2(ParseNode pn);
    void resolveFor(ParseNode pn);
    void resolveBody(ParseNode pn);
    void resolveBlock(ParseNode pn);
    void resolveDefault(ParseNode pn);
    void resolveLambda(ParseNode pn);
    void resolveAlgorithm(ParseNode pn);
    void resolvePrototype(ParseNode pn);
    void resolveCall(ParseNode pn);
    void resolveSubscript(ParseNode pn);
    void resolveBig(ParseNode pn);
    bool defineLocalScope(ParseNode pn, bool immutable = true);

    ParseTree& parse_tree;
};

}

}

#endif // HOPE_SYMBOL_TABLE_H
