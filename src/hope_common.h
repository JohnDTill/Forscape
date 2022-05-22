#ifndef HOPE_COMMON_H
#define HOPE_COMMON_H

#include <cstddef>
#include <limits>
#include <parallel_hashmap/phmap.h>
#include <unordered_set>

#define HOPE_UNORDERED_MAP phmap::flat_hash_map
#define HOPE_STATIC_MAP const phmap::flat_hash_map
#define HOPE_UNORDERED_SET std::unordered_set
#define HOPE_STATIC_SET const std::unordered_set

namespace Hope {

template<typename T, typename IN_TYPE>
inline constexpr T debug_cast(IN_TYPE in) noexcept {
    assert(dynamic_cast<T>(in));
    return static_cast<T>(in);
}

typedef size_t ParseNode;
extern inline constexpr size_t NONE = std::numeric_limits<size_t>::max();
extern inline constexpr size_t UNKNOWN_SIZE = 0;
extern inline constexpr double STALE = std::numeric_limits<double>::quiet_NaN();

#ifndef NDEBUG
#define DEBUG_INIT_NONE =NONE
#define DEBUG_INIT_NULLPTR =nullptr
extern inline constexpr size_t UNITIALISED = NONE-1;
#define DEBUG_INIT_UNITIALISED
#else
#define DEBUG_INIT_NONE
#define DEBUG_INIT_NULLPTR
#define DEBUG_INIT_UNITIALISED
#endif

}

#endif // HOPE_COMMON_H
