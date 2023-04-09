#ifndef FORSCAPE_COMMON_H
#define FORSCAPE_COMMON_H

#include <cstddef>
#include <limits>
#include <parallel_hashmap/phmap.h>
#include <unordered_set>

#define FORSCAPE_UNORDERED_MAP phmap::flat_hash_map
#define FORSCAPE_STATIC_MAP const phmap::flat_hash_map
#define FORSCAPE_UNORDERED_SET std::unordered_set
#define FORSCAPE_STATIC_SET const std::unordered_set

namespace Forscape {

template<typename T, typename IN_TYPE>
inline constexpr T debug_cast(IN_TYPE in) noexcept {
    assert(dynamic_cast<T>(in));
    return static_cast<T>(in);
}

typedef size_t ParseNode;
typedef size_t AstNode;
class Program;
extern inline constexpr size_t NONE = std::numeric_limits<size_t>::max();
extern inline constexpr size_t UNKNOWN_SIZE = 0;
extern inline constexpr double STALE = std::numeric_limits<double>::quiet_NaN();

#ifndef NDEBUG
#define DEBUG_INIT_NONE =NONE
#define DEBUG_INIT_NULLPTR =nullptr
#define DEBUG_INIT_UNITIALISED(type) =std::numeric_limits<type>::max()-1
#define DEBUG_INIT_STALE =STALE
#else
#define DEBUG_INIT_NONE
#define DEBUG_INIT_NULLPTR
#define DEBUG_INIT_UNITIALISED(type)
#define DEBUG_INIT_STALE
#endif

namespace Code {
class AbstractSyntaxTree;
struct Error;
class ParseTree;
}

namespace Typeset {
class Controller;
class Model;
class Selection;
}

}

#endif // FORSCAPE_COMMON_H
