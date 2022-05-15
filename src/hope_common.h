#ifndef HOPE_COMMON_H
#define HOPE_COMMON_H

#include <cstddef>
#include <limits>

template<typename T, typename IN_TYPE>
inline constexpr T debug_cast(IN_TYPE in) noexcept {
    assert(dynamic_cast<T>(in));
    return static_cast<T>(in);
}

typedef size_t ParseNode;
extern inline constexpr size_t NONE = std::numeric_limits<size_t>::max();

#endif // HOPE_COMMON_H
