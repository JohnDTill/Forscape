#ifndef FORSCAPE_STACK_H
#define FORSCAPE_STACK_H

#include "forscape_value.h"
#include <vector>

#ifndef NDEBUG
#define DEBUG_STACK_NAME , std::string name
#define DEBUG_STACK_ARG(x) , x
#else
#define DEBUG_STACK_NAME
#define DEBUG_STACK_ARG(x)
#endif

namespace Forscape {

namespace Code {

class Stack : private std::vector<Value> {
public:
    size_t size() const noexcept;
    void clear() noexcept;
    void push(const Value& value  DEBUG_STACK_NAME); //EVENTUALLY: running out of memory in the user program is a real possibility
    void pop() noexcept;
    Value& read(size_t offset  DEBUG_STACK_NAME) noexcept;
    Value& readReturn() noexcept;
    void trim(size_t sze) noexcept;
    Value& back() noexcept;
    Value& operator[](size_t index) noexcept;

#ifndef NDEBUG
    void print();
    FORSCAPE_UNORDERED_MAP<std::string, std::unordered_set<std::string>> aliases;

private:
    std::vector<std::string> stack_names;
#endif
};

}

}

#endif // FORSCAPE_STACK_H
