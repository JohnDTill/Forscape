#include "typeset_filesystem.h"

#include <hope_serial.h>
#include <typeset_model.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Hope {

namespace Typeset {

static std::vector<std::filesystem::path> path =
    {std::filesystem::current_path(), std::filesystem::current_path() / "../../test/interpreter_scripts/in"};
static constexpr std::string_view extensions[] = {".txt"};

static Model* tryToOpenFile(const std::string& file_path) {
    std::ifstream in(file_path);
    if(!in.is_open()) return nullptr;

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    assert(Hope::isValidSerial(src)); //DO THIS: allow failure

    return Model::fromSerial(src);
}

Model* Filesystem::open(std::string_view file_name) {
    const bool auto_extension = (file_name.find('.') == std::string_view::npos);

    for(std::filesystem::path path_entry : path){
        path_entry /= file_name;

        if(auto_extension){
            for(const std::string_view& extension : extensions){
                path_entry.replace_extension(extension);
                if(Typeset::Model* model = tryToOpenFile(path_entry.string())) return model;
            }
        }else{
            if(Typeset::Model* model = tryToOpenFile(path_entry.string())) return model;
        }
    }

    return nullptr;
}

}

}
