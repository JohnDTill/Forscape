#ifndef HOPE_TYPE_SYSTEM_H
#define HOPE_TYPE_SYSTEM_H

#include <cassert>
#include <unordered_set>
#include <vector>

class TypeSystem : private std::vector<size_t> {

typedef size_t ParentType;
static constexpr ParentType NO_TYPE = 0;
static constexpr ParentType PRIMITIVE = 1;
static constexpr ParentType FUNCTION = 2;
static constexpr ParentType MULTIPLE_POSSIBILITIES = 3;
typedef size_t TypeId;

ParentType parentType(TypeId type_id) const noexcept{
    return operator[](type_id);
}

bool hasMultiplePossibilities(TypeId type_id) const noexcept{
    return parentType(type_id) == MULTIPLE_POSSIBILITIES;
}

constexpr bool isNary(ParentType parent_type) const noexcept{
    return parent_type >= FUNCTION;
}

size_t startIndex(TypeId type_id) const noexcept{
    assert(isNary(parentType(type_id)));
    return type_id+2;
}

size_t endIndex(TypeId type_id) const noexcept{
    assert(isNary(parentType(type_id)));
    return operator[](type_id+1);
}

size_t searchPossibility(TypeId super, TypeId sub) const noexcept{
    assert(hasMultiplePossibilities(super));
    assert(!hasMultiplePossibilities(sub));
    for(size_t i = startIndex(super); i < endIndex(super); i++)
        if(operator[](i) == sub) return sub;
    return NO_TYPE;
}

size_t possibilityIntersection(TypeId a, TypeId b){
    assert(hasMultiplePossibilities(a));
    assert(hasMultiplePossibilities(b));
    //DO THIS
}

TypeId typeIntersection(TypeId a, TypeId b){
    if(a==b) return a;
    if(a==NO_TYPE || b==NO_TYPE) return NO_TYPE;

    bool mult_a = hasMultiplePossibilities(a);
    bool mult_b = hasMultiplePossibilities(b);

    if(mult_a && mult_b) return possibilityIntersection(a, b);
    else if(mult_a) return searchPossibility(a, b);
    else if(mult_b) return searchPossibility(b, a);
    else return NO_TYPE;
}

};

#endif // HOPE_TYPE_SYSTEM_H
