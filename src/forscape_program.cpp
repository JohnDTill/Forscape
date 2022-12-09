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
    auto lookup = source_files.find(path);
    if(lookup != source_files.end()) return FILE_ALREADY_OPEN;

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
    source_files[path] = model; //DO THIS: how to avoid 2 map operations?

    return reinterpret_cast<size_t>(model);
}

Program::ptr_or_code Program::openFromRelativePath(std::string_view file_name){
    std::filesystem::path rel_path(file_name);
    Program::ptr_or_code result = rel_path.has_extension() ?
        openFromRelativePathSpecifiedExtension(rel_path) :
        openFromRelativePathAutoExtension(rel_path);

    return result;
}

void Program::freeFileMemory() noexcept {
    for(auto& entry : source_files)
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
            path_entry.replace_extension(extension);
            if(Program::ptr_or_code model = openFromAbsolutePath(path_entry)) return model;
        }
    }

    return FILE_NOT_FOUND;
}

}
