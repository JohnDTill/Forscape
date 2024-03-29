#ifndef FORSCAPE_PROGRAM_H
#define FORSCAPE_PROGRAM_H

#include "forscape_common.h"
#include "forscape_dynamic_settings.h"
#include "forscape_error.h"
#include <filesystem>
#include <vector>

#include "forscape_interpreter.h"

namespace Forscape {

class Program {
public:
    static Program* instance() noexcept;
    bool noErrors() const noexcept;
    void clear() noexcept;
    void setProgramEntryPoint(std::filesystem::path path, Typeset::Model* model);
    typedef size_t ptr_or_code;
    ptr_or_code openFromAbsolutePath(const std::filesystem::path& path);
    static constexpr size_t FILE_NOT_FOUND = 0;
    static constexpr size_t FILE_CORRUPTED = 1;
    ptr_or_code openFromRelativePath(std::string_view file_name, std::filesystem::path dir);
    ptr_or_code openFromRelativePath(std::string_view file_name);
    void freeFileMemory() noexcept;
    const std::vector<Typeset::Model*>& allFiles() const noexcept;
    const std::vector<Typeset::Model*>& getPendingProjectBrowserUpdates() const noexcept;
    void clearPendingProjectBrowserUpdates() noexcept;
    void reset() noexcept;
    void runStaticPass();
    void getFileSuggestions(std::vector<std::string>& suggestions, Typeset::Model* active) const;
    void getFileSuggestions(std::vector<std::string>& suggestions, std::string_view input, Typeset::Model* active) const;
    void removeFile(Typeset::Model* model) noexcept;
    std::string run();
    void runThread();
    void stop();

    Code::Settings settings;
    Code::ErrorStream error_stream;
    Code::ParseTree parse_tree;
    Code::StaticPass static_pass = Code::StaticPass(parse_tree, error_stream);
    Code::Interpreter interpreter;

    FORSCAPE_UNORDERED_MAP<std::filesystem::path, Typeset::Model*> source_files; //May contain multiple entries per model
    Typeset::Model* program_entry_point = nullptr;

    //DESIGN QUAGMIRE (AST): define difference between model parse_tree and program parse_tree

private:
    static Program* singleton_instance;
    Program() = default;
    ptr_or_code openFromRelativePathSpecifiedExtension(std::filesystem::path file_name);
    ptr_or_code openFromRelativePathAutoExtension(std::filesystem::path file_name);

    std::vector<Typeset::Model*> all_files;
    std::vector<Typeset::Model*> pending_project_browser_updates;
    std::vector<std::filesystem::path> project_path = {
        std::filesystem::path(), //Placeholder for searching file's directory
        std::filesystem::current_path(), //Placeholder for base project directory
    };
    static constexpr std::string_view extensions[] = {".π"};

    std::filesystem::path& fileDirectory() noexcept { return project_path[0]; }
    std::filesystem::path& projectDirectory() noexcept { return project_path[1]; }
};

}

#endif // FORSCAPE_PROGRAM_H
