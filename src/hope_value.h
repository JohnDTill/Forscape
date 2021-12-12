#ifndef HOPE_VALUE_H
#define HOPE_VALUE_H

#include "hope_error.h"
#include "hope_parse_tree.h"
#include <memory>
#include <variant>
#include <vector>
//#include <Eigen/src/Core/Matrix.h>
#include <Eigen/Eigen>

namespace Hope {

namespace Code {

typedef std::vector<std::shared_ptr<void>> Closure;

struct Lambda{
    Closure closure;
    ParseNode def;
    ParseNode captured(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 0);
    }
    ParseNode upvalues(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 1);
    }
    ParseNode params(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 2);
    }
    ParseNode expr(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 3);
    }

    Lambda(ParseNode def)
        : def(def) {}
};

struct Algorithm{
    Closure closure;
    ParseNode def;

    ParseNode name(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 0);
    }
    ParseNode captured(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 1);
    }
    ParseNode upvalues(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 2);
    }
    ParseNode params(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 3);
    }
    ParseNode body(const ParseTree& parse_tree) const noexcept{
        return parse_tree.arg(def, 4);
    }

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
