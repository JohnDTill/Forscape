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

void Program::clear() noexcept {
    source_files.clear();
    pending_project_browser_updates.clear();
    errors.clear();
    warnings.clear();
}

void Program::setProgramEntryPoint(std::filesystem::path path, Typeset::Model* model) {
    assert(path == std::filesystem::canonical(path));
    source_files.clear();
    source_files[path] = model;
    projectDirectory() = path.parent_path();
    program_entry_point = model;
}

Program::ptr_or_code Program::openFromAbsolutePath(const std::filesystem::path& path){
    #define FILE_NOT_FOUND_PTR reinterpret_cast<Typeset::Model*>(FILE_NOT_FOUND)

    auto result = source_files.insert({path, FILE_NOT_FOUND_PTR});
    auto& entry = result.first;
    if(!result.second) return reinterpret_cast<ptr_or_code>(entry->second);

    std::ifstream in(path);
    if(!in.is_open()) return FILE_NOT_FOUND;

    assert(path == std::filesystem::weakly_canonical(path));

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    if(!Forscape::isValidSerial(src)) return FILE_CORRUPTED;

    Typeset::Model* model = Typeset::Model::fromSerial(src);
    model->path = path;
    entry->second = model;
    pending_project_browser_updates.push_back(model);

    return reinterpret_cast<size_t>(model);
}

Program::ptr_or_code Program::openFromRelativePath(std::string_view file_name, std::filesystem::path dir) {
    fileDirectory() = dir;
    return openFromRelativePath(file_name);
}

Program::ptr_or_code Program::openFromRelativePath(std::string_view file_name){
    std::filesystem::path rel_path = std::filesystem::u8path(file_name);
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

const std::vector<Typeset::Model*>& Program::getPendingProjectBrowserUpdates() const noexcept {
    return pending_project_browser_updates;
}

void Program::clearPendingProjectBrowserUpdates() noexcept {
    pending_project_browser_updates.clear();
}

void Program::resetModels() noexcept {
    for(auto& entry : source_files) entry.second->is_parsed = false;
}

Program::ptr_or_code Program::openFromRelativePathSpecifiedExtension(std::filesystem::path rel_path){
    for(const std::filesystem::path& path_entry : project_path){
        std::filesystem::path abs_path = std::filesystem::weakly_canonical(path_entry / rel_path);
        if(Program::ptr_or_code model = openFromAbsolutePath(path_entry)) return model;
    }

    return FILE_NOT_FOUND;
}

Program::ptr_or_code Program::openFromRelativePathAutoExtension(std::filesystem::path rel_path){
    for(const std::filesystem::path& path_entry : project_path){
        std::filesystem::path abs_path = std::filesystem::weakly_canonical(path_entry / rel_path);

        //This seems like pointlessly much interaction
        for(const std::string_view& extension : extensions){
            abs_path.replace_extension(std::filesystem::u8path(extension));
            if(Program::ptr_or_code model = openFromAbsolutePath(abs_path)) return model;
        }
    }

    return FILE_NOT_FOUND;
}

}
