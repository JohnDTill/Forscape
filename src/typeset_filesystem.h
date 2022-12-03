#ifndef TYPESET_FILESYSTEM_H
#define TYPESET_FILESYSTEM_H

#include <string_view>

namespace Hope {

namespace Typeset {

class Model;

class Filesystem {
public:
    static Model* open(std::string_view file_name);
};

}

}

#endif // TYPESET_FILESYSTEM_H
