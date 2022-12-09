#ifndef FORSCAPE_PROGRAM_H
#define FORSCAPE_PROGRAM_H

#include "forscape_common.h"
#include <filesystem>
#include <vector>

namespace Forscape {

class Program {
public:
    static Program* instance() noexcept;
    void setProgramEntryPoint(std::filesystem::path path, Typeset::Model* model);
    typedef size_t ptr_or_code;
    ptr_or_code openFromAbsolutePath(std::filesystem::path path);
    static constexpr size_t FILE_NOT_FOUND = 0;
    static constexpr size_t FILE_ALREADY_OPEN = 1;
    static constexpr size_t FILE_CORRUPTED = 2;
    ptr_or_code openFromRelativePath(std::string_view file_name);
    void freeFileMemory() noexcept;

private:
    static Program* singleton_instance;
    Program() = default;
    ptr_or_code openFromRelativePathSpecifiedExtension(std::string_view file_name);
    ptr_or_code openFromRelativePathAutoExtension(std::string_view file_name);

    FORSCAPE_UNORDERED_MAP<std::filesystem::path, Typeset::Model*> source_files;
    std::vector<std::filesystem::path> project_path = {
        std::filesystem::current_path(),
        //DO THIS: stop hardcoding these, make an "addpath" mechanism or similar
        std::filesystem::current_path() / "../test/interpreter_scripts/in",
        std::filesystem::current_path() / "../../test/interpreter_scripts/in",};
    static constexpr std::string_view extensions[] = {".txt"};
};

}

#endif // FORSCAPE_PROGRAM_H
