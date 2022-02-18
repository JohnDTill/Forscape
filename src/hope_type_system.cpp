#include "hope_type_system.h"

#include <cassert>
#include <string_view>


#include <iostream>

namespace Hope {

namespace Code {

constexpr bool TypeSystem::isAbstractFunctionGroup(size_t type) noexcept {
    return type < FAILURE;
}

static constexpr std::string_view type_strs[] = {
    "Failure",
    "Void",
    "Boolean",
    "String",
    "Numeric",
    "Unknown",
};

std::string TypeSystem::typeString(Type t) const{
    if(isAbstractFunctionGroup(t)){
        return abstractFunctionSetString(t);
    }else{
        return std::string(type_strs[t - FAILURE]);
    }
}

TypeSystem::TypeSystem(){}

void TypeSystem::reset() noexcept{
    clear();
    memoized_abstract_function_groups.clear();
}

Type TypeSystem::makeFunctionSet(ParseNode pn) noexcept{
    std::vector<ParseNode> possible_fns = {pn};

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(1);
        push_back(pn);
    }

    return lookup.first->second;
}

Type TypeSystem::functionSetUnion(Type a, Type b){
    assert(v(a) == TYPE_FUNCTION_SET);
    assert(v(b) == TYPE_FUNCTION_SET);

    if(a == b) return a;

    std::vector<ParseNode> possible_fns;

    size_t last_a = last(a);
    size_t last_b = last(b);
    size_t index_a = first(a);
    size_t index_b = first(b);

    for(;;){
        ParseNode fn_a = v(index_a);
        ParseNode fn_b = v(index_b);

        if(fn_a <= fn_b){
            possible_fns.push_back(fn_a);
            index_a++;
            index_b += (fn_a == fn_b);
        }else{
            possible_fns.push_back(fn_b);
            index_b++;
        }

        if(index_a > last_a){
            while(index_b <= last_b) possible_fns.push_back(v(index_b++));
            break;
        }else if(index_b > last_b){
            while(index_a <= last_a) possible_fns.push_back(v(index_a++));
            break;
        }
    }

    auto lookup = memoized_abstract_function_groups.insert({possible_fns, size()});
    if(lookup.second){
        push_back(TYPE_FUNCTION_SET);
        push_back(possible_fns.size());
        insert(end(), possible_fns.begin(), possible_fns.end());
    }

    return lookup.first->second;
}

size_t TypeSystem::numElements(size_t index) const noexcept{
    assert(v(index) == TYPE_FUNCTION_SET);

    return v(index+1);
}

TypeSystem::ParseNode TypeSystem::arg(size_t index, size_t n) const noexcept{
    assert(n < numElements(index));
    return v(index + 2 + n);
}

size_t TypeSystem::v(size_t index) const noexcept{
    return operator[](index);
}

size_t TypeSystem::first(size_t index) const noexcept{
    return index+2;
}

size_t TypeSystem::last(size_t index) const noexcept{
    index++;
    return index + v(index);
}

std::string TypeSystem::abstractFunctionSetString(Type t) const{
    assert(v(t) == TYPE_FUNCTION_SET);

    size_t index = first(t);
    std::string str = "{" + std::to_string( v(index++) );
    while(index <= last(t))
        str += ", " + std::to_string(v(index++));
    str += "} : AbstractFunctionSet";

    return str;
}


}

}
