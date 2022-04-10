#ifndef HOPE_OPTIMISER_H
#define HOPE_OPTIMISER_H

#include <stddef.h>
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

class ParseTree;
typedef size_t ParseNode;

class Optimiser{
private:
    ParseTree& parse_tree;
    typedef std::vector<size_t> DimSignature;
    struct vectorOfIntHash{
        std::size_t operator()(const std::vector<size_t>& vec) const noexcept {
            std::size_t seed = vec.size();
            for(auto& i : vec) seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
    std::unordered_map<DimSignature, size_t, vectorOfIntHash> instantiated;

public:
    Optimiser(ParseTree& parse_tree);
    void optimise();

private:
    ParseNode visitNode(ParseNode pn);
    ParseNode visitDecimal(ParseNode pn);
    ParseNode visitLength(ParseNode pn);
    ParseNode visitMult(ParseNode pn);
    ParseNode visitPower(ParseNode pn);
    ParseNode visitUnaryMinus(ParseNode pn);
    ParseNode visitCall(ParseNode pn);
};

}

}

#endif // HOPE_OPTIMISER_H
