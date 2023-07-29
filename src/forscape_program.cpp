#include "forscape_program.h"

#include "forscape_message.h"
#include "forscape_serial.h"
#include "typeset_model.h"
#include <fstream>
#include <sstream>

namespace Forscape {

Program* Program::singleton_instance = new Program();

Program* Program::instance() noexcept {
    return singleton_instance;
}

bool Program::noErrors() const noexcept {
    return error_stream.noErrors();
}

void Program::clear() noexcept {
    source_files.clear();
    pending_project_browser_updates.clear();
    settings.reset();
    error_stream.reset();
}

void Program::setProgramEntryPoint(std::filesystem::path path, Typeset::Model* model) {
    assert(path.empty() || path == std::filesystem::canonical(path));
    all_files.clear();
    all_files.push_back(model);
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

    //The path is required to be lexically normal, which is the same as canonical except
    //that symlinks are not resolved, which requires reading from disk. We only pay this
    //cost after confirming the file is not loaded and does exist.
    assert(path == path.lexically_normal());
    std::filesystem::path canonical_path = std::filesystem::canonical(path);
    auto canonical_result = result;
    const bool followed_symlink = (canonical_path != path);
    if(followed_symlink){
        canonical_result = source_files.insert({canonical_path, FILE_NOT_FOUND_PTR});
        if(!canonical_result.second){
            //This file was already loaded
            result.first->second = canonical_result.first->second;
            return reinterpret_cast<ptr_or_code>(canonical_result.first->second);
        }
    }

    std::stringstream buffer;
    buffer << in.rdbuf();

    std::string src = buffer.str();
    for(size_t i = 1; i < src.size(); i++)
        if(src[i] == '\r' && src[i-1] != OPEN)
            src[i] = '\0';
    src.erase( std::remove(src.begin(), src.end(), '\0'), src.end() );

    if(!Forscape::isValidSerial(src)) return FILE_CORRUPTED;

    Typeset::Model* model = Typeset::Model::fromSerial(src);
    all_files.push_back(model);
    model->path = canonical_path;
    entry->second = model;
    canonical_result.first->second = model;
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
    for(auto& entry : all_files) delete entry;
    all_files.clear();
    source_files.clear();
}

const std::vector<Typeset::Model*>& Program::allFiles() const noexcept {
    return all_files;
}

const std::vector<Typeset::Model*>& Program::getPendingProjectBrowserUpdates() const noexcept {
    return pending_project_browser_updates;
}

void Program::clearPendingProjectBrowserUpdates() noexcept {
    pending_project_browser_updates.clear();
}

void Program::reset() noexcept {
    for(const auto& entry : all_files){
        entry->is_imported = false;

        for(Code::Symbol& symbol : entry->symbol_builder.symbol_table.symbols){
            symbol.last_external_usage = symbol.lastUsage();
            symbol.type = NONE;
        }
    }

    program_entry_point->is_imported = true;
}

void Program::runStaticPass() {
    static bool running = false;
    if(running) return;
    running = true;

    error_stream.reset();
    settings.reset(); //EVENTUALLY: this should be an assert rather than an action
    program_entry_point->performSemanticFormatting();
    static_pass.resolve(program_entry_point);
    running = false;
}

static bool notPiFile(const std::string& str) noexcept {
    if(str.size() < 3) return true;
    return std::string_view(str.data() + str.size() - 3) != ".π";
}

void Program::getFileSuggestions(std::vector<std::string>& suggestions, Typeset::Model* active) const {
    const std::string model_filename = active->path.filename().u8string();

    for(const std::filesystem::path& path_entry : project_path){
        if(std::filesystem::exists(path_entry)){
            for(auto const& dir_entry : std::filesystem::directory_iterator{path_entry}){
                const std::string candidate = dir_entry.path().filename().u8string();
                if(notPiFile(candidate) && candidate != model_filename) continue;
                suggestions.push_back(candidate);
            }
        }
    }
}

void Program::getFileSuggestions(std::vector<std::string>& suggestions, std::string_view input, Typeset::Model* active) const {
    const std::filesystem::path input_as_path(input);

    for(const std::filesystem::path& path_entry : project_path){
        const std::string input_filename = std::filesystem::is_directory(path_entry / input_as_path) ? "" : input_as_path.filename().u8string();
        const std::filesystem::path dir_of_input = (path_entry / input_as_path).parent_path();
        if(!std::filesystem::exists(dir_of_input)) continue;

        const std::filesystem::path model_rel_path = std::filesystem::relative(active->path, path_entry);

        for(const auto& dir_entry : std::filesystem::directory_iterator{dir_of_input}){
            const std::filesystem::path candidate_path = dir_entry.path();
            const std::string candidate_filename = candidate_path.filename().u8string();

            if(candidate_filename.size() < input_filename.size() ||
               notPiFile(candidate_filename) ||
               std::string_view(candidate_filename.data(), input_filename.size()) != input_filename) continue;

            std::filesystem::path rel_path = std::filesystem::relative(candidate_path, path_entry);
            if(rel_path != model_rel_path) suggestions.push_back(rel_path.u8string());
        }
    }
}

Program::ptr_or_code Program::openFromRelativePathSpecifiedExtension(std::filesystem::path rel_path){
    for(const std::filesystem::path& path_entry : project_path){
        std::filesystem::path abs_path = (path_entry / rel_path).lexically_normal();
        if(Program::ptr_or_code model = openFromAbsolutePath(abs_path)) return model;
    }

    return FILE_NOT_FOUND;
}

Program::ptr_or_code Program::openFromRelativePathAutoExtension(std::filesystem::path rel_path){
    for(const std::filesystem::path& path_entry : project_path){
        std::filesystem::path abs_path = (path_entry / rel_path).lexically_normal();

        //This seems like pointlessly much interaction
        for(const std::string_view& extension : extensions){
            abs_path.replace_extension(std::filesystem::u8path(extension));
            if(Program::ptr_or_code model = openFromAbsolutePath(abs_path)) return model;
        }
    }

    return FILE_NOT_FOUND;
}

std::string Program::run(){
    assert(error_stream.noErrors());

    interpreter.run(
        parse_tree,
        static_pass.instantiation_lookup,
        static_pass.number_switch,
        static_pass.string_switch);

    std::string str;

    InterpreterOutput* msg;
    while(interpreter.message_queue.try_dequeue(msg))
        switch(msg->getType()){
            case Forscape::InterpreterOutput::Print:
                str += static_cast<PrintMessage*>(msg)->msg;
                delete msg;
                break;
            case Forscape::InterpreterOutput::CreatePlot:
                //EVENTUALLY: maybe do something here?
                delete msg;
                break;
            case Forscape::InterpreterOutput::AddDiscreteSeries:{
                delete msg;
                break;
            }
            default: assert(false);
        }

    if(interpreter.error_code != Code::ErrorCode::NO_ERROR_FOUND){
        const Typeset::Selection& sel = program_entry_point->parser.parse_tree.getSelection(interpreter.error_node);
        const std::string msg(getMessage(interpreter.error_code));

        str += "\nLine " + sel.getStartLineAsString() + " - " + msg;
        error_stream.fail(sel, msg, interpreter.error_code);
    }

    return str;
}

void Program::runThread(){
    assert(error_stream.noErrors());
    interpreter.runThread(
        parse_tree,
        static_pass.instantiation_lookup,
        static_pass.number_switch,
        static_pass.string_switch);
}

void Program::stop(){
    interpreter.stop();
}

}
