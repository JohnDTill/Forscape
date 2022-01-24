#include "hope_stack.h"

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

size_t Stack::size() const noexcept{
    return std::vector<Value>::size();
}

void Stack::clear() noexcept{
    std::vector<Value>::clear();

    #ifndef NDEBUG
    stack_names.clear();
    #endif
}


void Stack::push(const Value& value, std::string name){
    std::vector<Value>::push_back(value);
    #ifndef NDEBUG
    stack_names.push_back( name );
    #endif
}

void Stack::pop() noexcept{
    std::vector<Value>::pop_back();
    #ifndef NDEBUG
    stack_names.pop_back();
    #endif
}

Value& Stack::read(size_t offset, std::string intended) noexcept{
    assert(offset < size());

    #ifndef NDEBUG
    const std::string& actual = stack_names[offset];
    if(intended != actual){
        std::cout << "Tried to read " << intended << ", but read "
                  << actual << '\n' << std::endl;
        print();
    }
    assert(intended == actual);
    #endif

    return std::vector<Value>::operator[](offset);
}

Value& Stack::readReturn() noexcept{
    assert(empty() || stack_names.back() == "%RETURN");
    #ifndef NDEBUG
    stack_names.back() = "%CLEARED_RETURN";
    #endif
    return back();
}

void Stack::trim(size_t sze) noexcept{
    assert(empty() || stack_names.back() != "%RETURN");
    assert(sze <= size());

    if(size() > sze){
        resize(sze);
        #ifndef NDEBUG
        stack_names.resize(sze);
        #endif
    }
}

Value& Stack::back() noexcept{
    return std::vector<Value>::back();
}

Value& Stack::operator[](size_t index) noexcept{
    return std::vector<Value>::operator[](index);
}

#ifndef NDEBUG
void Stack::print(){
    assert(size() == stack_names.size());

    std::cout << "STACK {\n";
    for(size_t i = 0; i < size(); i++)
        std::cout << "    " << stack_names[i] << ",\n";
    std::cout << "}\n" << std::endl;
}
#endif

}

}
