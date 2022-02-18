#ifndef HOPE_TYPE_SYSTEM_H
#define HOPE_TYPE_SYSTEM_H

#include <string>
#include <unordered_map>
#include <vector>

namespace Hope {

namespace Code {

typedef size_t Type;

class TypeSystem : private std::vector<size_t> {

public:
    typedef size_t ParseNode;
    static constexpr Type UNKNOWN = std::numeric_limits<size_t>::max();
    static constexpr Type NUMERIC = UNKNOWN-1;
    static constexpr Type STRING = UNKNOWN-2;
    static constexpr Type BOOLEAN = UNKNOWN-3;
    static constexpr Type VOID = UNKNOWN-4;
    static constexpr Type FAILURE = UNKNOWN-5;
    static constexpr bool isAbstractFunctionGroup(size_t type) noexcept;

    std::string typeString(Type t) const;

    TypeSystem();
    void reset() noexcept;
    Type makeFunctionSet(ParseNode fn) noexcept;
    Type functionSetUnion(Type a, Type b);
    size_t numElements(size_t index) const noexcept;
    ParseNode arg(size_t index, size_t n) const noexcept;

    struct vectorOfIntHash{
        std::size_t operator()(const std::vector<size_t>& vec) const noexcept {
            std::size_t seed = vec.size();
            for(auto& i : vec) seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

private:
    typedef size_t ParentType;
    static constexpr ParentType TYPE_FUNCTION_SET = 0;
    size_t v(size_t index) const noexcept;
    size_t first(size_t index) const noexcept;
    size_t last(size_t index) const noexcept;
    std::string abstractFunctionSetString(Type t) const;

    std::unordered_map<std::vector<ParseNode>, Type, vectorOfIntHash> memoized_abstract_function_groups;
};

}

}

#endif // HOPE_TYPE_SYSTEM_H
