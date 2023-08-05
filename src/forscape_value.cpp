#include "forscape_value.h"

#include "forscape_parse_tree.h"

namespace Forscape {

namespace Code {

ParseNode Lambda::valCap(const ParseTree& parse_tree) const noexcept {
    return parse_tree.valCapList(def);
}

ParseNode Lambda::refCap(const ParseTree& parse_tree) const noexcept {
    return parse_tree.refCapList(def);
}

ParseNode Lambda::params(const ParseTree& parse_tree) const noexcept {
    return parse_tree.paramList(def);
}

ParseNode Lambda::expr(const ParseTree& parse_tree) const noexcept {
    return parse_tree.body(def);
}

ParseNode Algorithm::name(const ParseTree& parse_tree) const noexcept {
    return parse_tree.algName(def);
}

ParseNode Algorithm::valCap(const ParseTree& parse_tree) const noexcept {
    return parse_tree.valCapList(def);
}

ParseNode Algorithm::refCap(const ParseTree& parse_tree) const noexcept {
    return parse_tree.refCapList(def);
}

ParseNode Algorithm::params(const ParseTree& parse_tree) const noexcept {
    return parse_tree.paramList(def);
}

ParseNode Algorithm::body(const ParseTree& parse_tree) const noexcept {
    return parse_tree.body(def);
}

}

}
