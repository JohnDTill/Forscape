#ifndef HOPE_COMMON_H
#define HOPE_COMMON_H

#include <cstddef>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace Hope {

template<typename T, typename IN_TYPE>
inline constexpr T debug_cast(IN_TYPE in) noexcept {
    assert(dynamic_cast<T>(in));
    return static_cast<T>(in);
}

typedef size_t ParseNode;
extern inline constexpr size_t NONE = std::numeric_limits<size_t>::max();
extern inline constexpr size_t UNKNOWN_SIZE = 0;

using std::unordered_map;
template<class Key, class T> using static_map = const unordered_map<Key, T>;

using std::unordered_set;
template<class T> using static_set = const unordered_set<T>;

}

#endif // HOPE_COMMON_H
