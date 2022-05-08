#ifndef HOPE_STACK_H
#define HOPE_STACK_H

#include "hope_value.h"
#include <vector>

namespace Hope {

namespace Code {

class Stack : private std::vector<Value> {
public:
    size_t size() const noexcept;
    void clear() noexcept;
    void push(const Value& value, std::string name); //EVENTUALLY: running out of memory in the user program is a real possibility
    void pop() noexcept;
    Value& read(size_t offset, std::string name) noexcept;
    Value& readReturn() noexcept;
    void trim(size_t sze) noexcept;
    Value& back() noexcept;
    Value& operator[](size_t index) noexcept;

#ifndef NDEBUG
    void print();

private:
    std::vector<std::string> stack_names;
#endif
};

}

}

#endif // HOPE_STACK_H
