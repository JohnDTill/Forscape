#include "forscape_program.h"

#include "forscape_serial.h"
#include "typeset_model.h"
#include <fstream>
#include <sstream>

namespace Forscape {

Program* Program::singleton_instance = new Program();

Program* Program::instance() noexcept {
    return singleton_instance;
}

void Program::setProgramEntryPoint(std::filesystem::path path, Typeset::Model* model) {
    source_files.clear();
    source_files[path] = model;
}

Program::ptr_or_code Program::openFromAbsolutePath(std::filesystem::path path){
    #define FILE_NOT_FOUND_PTR reinterpret_cast<Typeset::Model*>(FILE_NOT_FOUND)

    auto result = source_files.insert({path, FILE_NOT_FOUND_PTR});
    auto& entry = result.first;
    if(!result.second) return entry->second == FILE_NOT_FOUND_PTR ? FILE_NOT_FOUND : FILE_ALREADY_OPEN;

    std::ifstream in(path);
    if(!in.is_open()) return FILE_NOT_FOUND;

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    if(!Forscape::isValidSerial(src)) return FILE_CORRUPTED;

    Typeset::Model* model = Typeset::Model::fromSerial(src);
    entry->second = model;

    return reinterpret_cast<size_t>(model);
}

Program::ptr_or_code Program::openFromRelativePath(std::string_view file_name){
    //DO THIS: the search should start from the directory of the calling file

    std::filesystem::path rel_path = std::filesystem::u8path(file_name);
    Program::ptr_or_code result = rel_path.has_extension() ?
        openFromRelativePathSpecifiedExtension(rel_path) :
        openFromRelativePathAutoExtension(rel_path);

    return result;
}

void Program::freeFileMemory() noexcept {
    for(auto& entry : source_files)
        if(reinterpret_cast<size_t>(entry.second) > FILE_NOT_FOUND)
            delete entry.second;
    source_files.clear();
}

Program::ptr_or_code Program::openFromRelativePathSpecifiedExtension(std::filesystem::path rel_path){
    for(std::filesystem::path path_entry : project_path){
        path_entry /= rel_path;
        if(Program::ptr_or_code model = openFromAbsolutePath(path_entry)) return model;
    }

    return FILE_NOT_FOUND;
}

Program::ptr_or_code Program::openFromRelativePathAutoExtension(std::filesystem::path rel_path){
    for(std::filesystem::path path_entry : project_path){
        path_entry /= rel_path;

        //This seems like pointlessly much interaction
        for(const std::string_view& extension : extensions){
            path_entry.replace_extension(std::filesystem::u8path(extension));
            if(Program::ptr_or_code model = openFromAbsolutePath(path_entry)) return model;
        }
    }

    return FILE_NOT_FOUND;
}

}
