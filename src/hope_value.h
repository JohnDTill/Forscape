#ifndef HOPE_VALUE_H
#define HOPE_VALUE_H

#include "hope_error.h"
#include <memory>
#include <variant>
#include <vector>
//#include <Eigen/src/Core/Matrix.h>
#include <Eigen/Eigen>

namespace Hope {

namespace Code {

class ParseTree;
typedef std::vector<std::shared_ptr<void>> Closure;
typedef size_t ParseNode;

struct Lambda{
    Closure closure;
    ParseNode def;
    ParseNode valCap(const ParseTree& parse_tree) const noexcept;
    ParseNode refCap(const ParseTree& parse_tree) const noexcept;
    ParseNode params(const ParseTree& parse_tree) const noexcept;
    ParseNode expr(const ParseTree& parse_tree) const noexcept;

    Lambda(ParseNode def)
        : def(def) {}
};

struct Algorithm{
    Closure closure;
    ParseNode def;

    ParseNode name(const ParseTree& parse_tree) const noexcept;
    ParseNode valCap(const ParseTree& parse_tree) const noexcept;
    ParseNode refCap(const ParseTree& parse_tree) const noexcept;
    ParseNode params(const ParseTree& parse_tree) const noexcept;
    ParseNode body(const ParseTree& parse_tree) const noexcept;

    Algorithm(ParseNode def)
        : def(def){}
};

typedef std::variant<
    Code::Error*,
    double,
    Eigen::MatrixXd,
    std::string, //This is the biggest member
    bool,
    Lambda,
    Algorithm,
    void*
> Value;

constexpr size_t ValueSize = sizeof(Value);

static const Value NIL = static_cast<void*>(nullptr);

inline Value& conv(const std::shared_ptr<void>& ptr){
    return *static_cast<Value*>(ptr.get());
}

static constexpr size_t RuntimeError = 0;
static constexpr size_t double_index = 1;
static constexpr size_t MatrixXd_index = 2;
static constexpr size_t string_index = 3;
static constexpr size_t bool_index = 4;
static constexpr size_t Lambda_index = 5;
static constexpr size_t Algorithm_index = 6;
static constexpr size_t Unitialized_index = 7;

inline bool isFunction(size_t index) noexcept{
    return index >= Lambda_index;
}

#ifndef NDEBUG
inline bool valid(const Value& v){ return v.index() <= Unitialized_index; }
#endif

}

}

#endif // HOPE_VALUE_H
