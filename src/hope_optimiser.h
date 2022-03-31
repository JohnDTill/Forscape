#ifndef HOPE_OPTIMISER_H
#define HOPE_OPTIMISER_H

namespace Hope {

namespace Code {

class ParseTree;
typedef size_t ParseNode;

class Optimiser{
private:
    ParseTree& parse_tree;

public:
    Optimiser(ParseTree& parse_tree);
    void optimise();

private:
    ParseNode visitNode(ParseNode pn);
};

}

}

#endif // HOPE_OPTIMISER_H
