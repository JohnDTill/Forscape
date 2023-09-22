#include "projectbrowser.h"

#include "mainwindow.h"

#include <forscape_program.h>
#include <typeset_model.h>
#include <qt_compatability.h>

#include <QAction>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>

//PROJECT BROWSER UPDATE: switch to QTreeView/QFileSystemModel, or continue using QTreeWidget w/ std::filesystem?

//PROJECT BROWSER UPDATE: a program should not be singleton
//PROJECT BROWSER UPDATE: need assurance that the viewing history remains valid as files are deleted, renamed, w/e
//PROJECT BROWSER UPDATE: the project browser should probably have an undo stack
//PROJECT BROWSER UPDATE: finish implementing interaction actions
//PROJECT BROWSER UPDATE: show/hide unused files which are in a directory, but not imported anywhere
//PROJECT BROWSER UPDATE: fix save-as to rename files, projects
//PROJECT BROWSER UPDATE: allow multiple open projects, with one active project, and ability to close
//PROJECT BROWSER UPDATE: Parse file-level descriptions for use in tooltips, with full paths in tooltips
//PROJECT BROWSER UPDATE: scan for external changes
//PROJECT BROWSER UPDATE: limit external dependence on the project browser
//PROJECT BROWSER UPDATE: should support drag and drop

Q_DECLARE_METATYPE(Forscape::Typeset::Model*); //EVENTUALLY: this is only for compability with old versions

namespace Forscape {

static QIcon main_icon;
static QIcon file_icon;
static QIcon folder_icon;

class ProjectBrowser::FileEntry : public QTreeWidgetItem {
public:
    Forscape::Typeset::Model* model = nullptr;
    std::filesystem::path path;
    FileEntry(QTreeWidgetItem* parent);
    FileEntry();
    void setModel(Forscape::Typeset::Model& m) noexcept;
    bool isSavedToDisk() const noexcept;
    const std::filesystem::path& getPath() const noexcept;
    void deleteFile() noexcept;
};

ProjectBrowser::FileEntry::FileEntry(QTreeWidgetItem* parent) : QTreeWidgetItem(parent) {
    setIcon(0, file_icon);
}

ProjectBrowser::FileEntry::FileEntry() : QTreeWidgetItem() {
    setIcon(0, file_icon);
}

void ProjectBrowser::FileEntry::setModel(Forscape::Typeset::Model& m) noexcept {
    model = &m;
    path = m.path;
    setText(0, isSavedToDisk() ? toQString(path.filename()) : "untitled");
    if(isSavedToDisk()) setToolTip(0, toQString(path));
}

bool ProjectBrowser::FileEntry::isSavedToDisk() const noexcept {
    return !model->notOnDisk();
}

const std::filesystem::path& ProjectBrowser::FileEntry::getPath() const noexcept {
    if(model) assert(path == model->path);
    return path;
}

void ProjectBrowser::FileEntry::deleteFile() noexcept {
    if(isSavedToDisk()) std::filesystem::remove(path);
    delete model;
    delete this;
}

static bool itemIsFile(const QTreeWidgetItem* item) noexcept {
    return item->childCount() == 0;
}

static bool itemIsDirectory(const QTreeWidgetItem* item) noexcept {
    return item->childCount() > 0;
}

class ProjectBrowser::DirectoryEntry : public QTreeWidgetItem {
private:
    std::filesystem::path path;

public:
    DirectoryEntry() : QTreeWidgetItem() {
        setIcon(0, folder_icon);
        setExpanded(true);
    }

    void setPath(const std::filesystem::path& p) {
        path = p;
        setText(0, toQString(p.filename()));
        setToolTip(0, toQString(path));
    }

    const std::filesystem::path& getPath() const noexcept {
        return path;
    }
};

ProjectBrowser::ProjectBrowser(QWidget* parent, MainWindow* main_window)
    : QTreeWidget(parent), main_window(main_window) {
    setHeaderHidden(true);
    setIndentation(10);
    setMinimumWidth(120);
    setRootIsDecorated(false); //Hide a universal root entry, so the user has the illusion of multiple "top-level" entries
    setContextMenuPolicy(Qt::CustomContextMenu); //Needed to enable context menu

    main_icon = QIcon(":/fonts/anchor.svg");
    file_icon = QIcon(":/fonts/pi_file.svg");
    folder_icon = QFileIconProvider().icon(QFileIconProvider::Folder);

    connect(this, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(onFileClicked(QTreeWidgetItem*, int)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onRightClick(const QPoint&)));
}

void ProjectBrowser::setProject(Forscape::Typeset::Model* model) {
    clear();
    const std::filesystem::path& std_path = model->path;
    QTreeWidgetItem* root = invisibleRootItem();
    FileEntry* main_file = new FileEntry(root);
    main_file->setIcon(0, main_icon);
    main_file->setModel(*model);
    files_in_filesystem[std_path] = main_file;
    files_in_memory[model] = main_file;
    const std::filesystem::path parent_path = std_path.parent_path();
    files_in_filesystem[parent_path] = root;
    root->setData(0, Qt::UserRole, toQString(parent_path));
    currently_viewed_file = main_file;
    QFont normal_font = font();
    QFont bold_font = normal_font;
    bold_font.setBold(true);
    currently_viewed_file->setFont(0, bold_font);
}

void ProjectBrowser::addUnsavedFile(Forscape::Typeset::Model* model) {
    FileEntry* item = new FileEntry(invisibleRootItem());
    files_in_memory[model] = item;
    item->setText(0, "untitled");
    item->setIcon(0, file_icon);
    item->model = model;
    sortItems(0, Qt::SortOrder::AscendingOrder);
}

void ProjectBrowser::saveModel(Forscape::Typeset::Model* saved_model, const std::filesystem::path& std_path) {
    std::filesystem::path old_path = saved_model->path;
    const bool create_new_file = old_path.empty();
    const bool rename_file = old_path != std_path && !create_new_file;

    if(create_new_file || rename_file){
        auto file_with_same_path = files_in_filesystem.find(std_path);
        if(file_with_same_path != files_in_filesystem.end()){
            Typeset::Model* cloned_model = Typeset::Model::fromSerial(saved_model->toSerial());
            cloned_model->write_time = std::filesystem::file_time_type::clock::now();
            cloned_model->path = std_path;

            //Saving over existing project file
            FileEntry* existing_file_entry = debug_cast<FileEntry*>(file_with_same_path->second);

            //The overwritten model is removed
            Forscape::Typeset::Model* overwritten_model = existing_file_entry->model;
            delete overwritten_model;
            bool success = files_in_memory.erase(overwritten_model);
            assert(success);

            //The previous file entry is repurposed
            existing_file_entry->model = cloned_model;
            const auto result = files_in_memory.insert({cloned_model, existing_file_entry});
            assert(result.second);

            main_window->viewModel(cloned_model);
            main_window->reparse();
        }else if(create_new_file){
            auto entry = files_in_memory.find(saved_model);
            assert(entry != files_in_memory.end());
            FileEntry* existing_file_entry = debug_cast<FileEntry*>(entry->second);
            files_in_filesystem[std_path] = existing_file_entry;
            saved_model->path = std_path;

            for(int i = 0; i < invisibleRootItem()->childCount(); i++)
                if(invisibleRootItem()->child(i)->childCount() > 0){
                    //Entry moves from unnamed column to parent under directory
                    invisibleRootItem()->removeChild(existing_file_entry);
                    linkFileToAncestor(existing_file_entry, std_path);
                }else{
                    existing_file_entry->setModel(*saved_model);
                }
        }else{
            //New file created
            Typeset::Model* cloned_model = Typeset::Model::fromSerial(saved_model->toSerial());
            cloned_model->write_time = std::filesystem::file_time_type::clock::now();
            cloned_model->path = std_path;
            addProjectEntry(cloned_model);

            main_window->viewModel(cloned_model);
            main_window->reparse();
        }
    }
}

void ProjectBrowser::updateProjectBrowser() {
    const auto& parsed_files = Forscape::Program::instance()->getPendingProjectBrowserUpdates();
    if(parsed_files.empty()) return;

    for(const auto& entry : parsed_files) addProjectEntry(entry);
    Forscape::Program::instance()->clearPendingProjectBrowserUpdates();
    sortByColumn(0, Qt::AscendingOrder);
}

void ProjectBrowser::addProjectEntry(Forscape::Typeset::Model* model) {
    std::filesystem::path path = model->path;
    assert(files_in_filesystem.find(path) == files_in_filesystem.end());

    FileEntry* tree_item = new FileEntry();
    tree_item->setModel(*model);
    files_in_filesystem[path] = tree_item;
    files_in_memory[model] = tree_item;
    model->write_time = std::filesystem::file_time_type::clock::now();

    linkFileToAncestor(tree_item, path);
}

void ProjectBrowser::populateWithNewProject(Forscape::Typeset::Model* model) {
    clear();
    FileEntry* item = new FileEntry(invisibleRootItem());
    files_in_memory[model] = item;
    item->setText(0, "untitled");
    item->setIcon(0, main_icon);
    item->model = model;
    sortItems(0, Qt::SortOrder::AscendingOrder);
    currently_viewed_file = item;
    setCurrentlyViewed(model);
    QFont f = font();
    f.setBold(true);
    item->setFont(0, f);
}

void ProjectBrowser::setCurrentlyViewed(Forscape::Typeset::Model* model) {
    QFont normal_font = font();
    QFont bold_font = normal_font;
    bold_font.setBold(true);

    //Remove highlighting on the old item
    if(currently_viewed_file) currently_viewed_file->setFont(0, QFont());

    //Highlight the new item
    const auto lookup = files_in_memory.find(model);
    assert(lookup != files_in_memory.end());
    currently_viewed_file = debug_cast<FileEntry*>(lookup->second);
    currently_viewed_file->setFont(0, bold_font);
    setCurrentItem(currently_viewed_file);
}

void ProjectBrowser::clear() noexcept {
    QTreeWidget::clear();
    files_in_filesystem.clear();
    files_in_memory.clear();

    setRootIsDecorated(false); //Hide a universal root entry, so the user has the illusion of multiple "top-level" entries
}

void ProjectBrowser::onFileClicked(QTreeWidgetItem* item, int column) {
    if(itemIsFile(item)) main_window->viewModel(debug_cast<FileEntry*>(item)->model);
}

void ProjectBrowser::onFileClicked() {
    onFileClicked(currentItem(), 0);
}

void ProjectBrowser::onShowInExplorer() {
    const QTreeWidgetItem* item = currentItem();

    QStringList args;

    if(itemIsFile(item)){
        const FileEntry& file_item = getSelectedFileEntry();
        assert(file_item.isSavedToDisk());
        args << "/select," << QDir::toNativeSeparators(toQString(file_item.path));
    }else{
        const DirectoryEntry& directory_item = getSelectedDirectory();
        args << QDir::toNativeSeparators(toQString(directory_item.getPath()));
    }

    QProcess* process = new QProcess(this);
    process->start("explorer.exe", args);
}

void ProjectBrowser::onDeleteFile() {
    FileEntry& item = getSelectedFileEntry();
    assert(item.model != Forscape::Program::instance()->program_entry_point);
    files_in_filesystem.erase(item.path);
    files_in_memory.erase(item.model);
    main_window->removeFile(item.model);
    item.deleteFile();

    //PROJECT BROWSER UPDATE: this has implications for the viewing buffer

    main_window->reparse();
}

void ProjectBrowser::onRightClick(const QPoint& pos) {
    QMenu menu(this);
    menu.setToolTipsVisible(true);

    QTreeWidgetItem* item = itemAt(pos);
    if(item == nullptr){
        QAction* new_file = menu.addAction(tr("Add New File"));
        new_file->setToolTip(tr("Create a new file for the project"));
        connect(new_file, SIGNAL(triggered(bool)), main_window, SLOT(on_actionNew_triggered()));

        QAction* hide = menu.addAction(tr("Hide project browser"));
        hide->setToolTip(tr("Remove the project browser from view"));
        connect(hide, SIGNAL(triggered(bool)), main_window, SLOT(hideProjectBrowser()));
    }else if(itemIsFile(item)){
        //Item is file
        FileEntry* file_item = debug_cast<FileEntry*>(item);
        QAction* open_file = menu.addAction(tr("Open File"));
        open_file->setToolTip(tr("Open the selected file in the editor"));
        connect(open_file, SIGNAL(triggered(bool)), this, SLOT(onFileClicked()));

        if(file_item->isSavedToDisk()){
            QAction* show_in_explorer = menu.addAction(tr("Show in Explorer"));
            show_in_explorer->setToolTip(tr("Show in the OS file browser"));
            connect(show_in_explorer, SIGNAL(triggered(bool)), this, SLOT(onShowInExplorer()));
        }

        auto m = file_item->model;
        if(m != Forscape::Program::instance()->program_entry_point){
            QAction* delete_file = menu.addAction(tr("Delete"));
            delete_file->setToolTip(tr("Erase this file"));
            connect(delete_file, SIGNAL(triggered(bool)), this, SLOT(onDeleteFile()));
        }

        QAction* rename_file = menu.addAction(tr("Rename"));
        rename_file->setToolTip(tr("Change the filename"));
        connect(rename_file, SIGNAL(triggered(bool)), this, SLOT(onFileRenamed()));

        menu.addSeparator();
        QAction* copy_name = menu.addAction(tr("Copy file name"));
        copy_name->setToolTip(tr("Copy the file name to the clipboard"));
        connect(copy_name, SIGNAL(triggered(bool)), this, SLOT(onCopyFileName()));

        if( file_item->isSavedToDisk() ){
            QAction* copy_full_path = menu.addAction(tr("Copy full path"));
            copy_full_path->setToolTip(tr("Copy the full path to the clipboard"));
            connect(copy_full_path, SIGNAL(triggered(bool)), this, SLOT(onCopyFilePath()));
        }
    }else{
        QAction* show_in_explorer = menu.addAction(tr("Show in Explorer"));
        show_in_explorer->setToolTip(tr("Show in the OS file browser"));
        connect(show_in_explorer, SIGNAL(triggered(bool)), this, SLOT(onShowInExplorer()));

        QAction* rename_folder = menu.addAction(tr("Rename"));
        rename_folder->setToolTip(tr("Change the directory name"));
        connect(rename_folder, SIGNAL(triggered(bool)), this, SLOT(onDirectoryRenamed()));

        QAction* expand = menu.addAction(item->isExpanded() ? tr("Collapse") : tr("Expand"));
        connect(expand, SIGNAL(triggered(bool)), this, SLOT(expandDirectory()));

        menu.addSeparator();
        QAction* copy_name = menu.addAction(tr("Copy directory name"));
        copy_name->setToolTip(tr("Copy the directory name to the clipboard"));
        connect(copy_name, SIGNAL(triggered(bool)), this, SLOT(onCopyDirName()));

        QAction* copy_full_path = menu.addAction(tr("Copy full path"));
        copy_full_path->setToolTip(tr("Copy the full path to the clipboard"));
        connect(copy_full_path, SIGNAL(triggered(bool)), this, SLOT(onCopyDirPath()));
    }

    menu.exec(mapToGlobal(pos));
}

void ProjectBrowser::onDirectoryRenamed() {
    DirectoryEntry& entry = getSelectedDirectory();
    const QString old_name = currentItem()->text(0);

    QInputDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(tr("Rename folder ") + '"' + old_name + '"');
    dialog.setLabelText(tr("New name:"));
    dialog.setInputMode(QInputDialog::TextInput);
    if(dialog.exec() != QDialog::Accepted) return;

    const std::filesystem::path old_path = entry.getPath();
    const std::filesystem::path new_path = old_path.parent_path() / toCppString(dialog.textValue());
    std::error_code rename_error;
    std::filesystem::rename(old_path, new_path, rename_error);

    if(rename_error){
        QMessageBox messageBox;
        messageBox.critical(nullptr, tr("Error"), tr("Failed to rename directory ") + '"' + old_name + '"');
        messageBox.setFixedSize(500,200);
    }else{
        entry.setPath(new_path);

        //PROJECT BROWSER UPDATE: update all the child paths
        //PROJECT BROWSER UPDATE: refactor paths in the code
        //std::string name = toCppString(text);
    }
}

void ProjectBrowser::onFileRenamed() {
    FileEntry& entry = getSelectedFileEntry();
    const QString old_name = currentItem()->text(0);

    QInputDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(tr("Rename file ") + '"' + old_name + '"');
    dialog.setLabelText(tr("New name:"));
    dialog.setInputMode(QInputDialog::TextInput);
    if(dialog.exec() != QDialog::Accepted) return;

    const QString text = dialog.textValue();
    const std::filesystem::path old_path = entry.getPath();
    std::filesystem::path new_path = old_path;
    new_path.replace_filename(toCppString(text));
    new_path.replace_extension(old_path.extension());
    std::error_code rename_error;
    std::filesystem::rename(old_path, new_path, rename_error);

    if(rename_error){
        QMessageBox messageBox;
        messageBox.critical(nullptr, tr("Error"), tr("Failed to rename file ") + '"' + old_name + '"');
        messageBox.setFixedSize(500,200);
    }else{
        entry.setText(0, text);
        entry.path = new_path;

        //PROJECT BROWSER UPDATE: update paths in the code
    }
}

void ProjectBrowser::expandDirectory() {
    assert(itemIsDirectory(currentItem()));
    currentItem()->setExpanded(!currentItem()->isExpanded());
}

void ProjectBrowser::onCopyFilePath() {
    const FileEntry& entry = getSelectedFileEntry();
    if(!entry.isSavedToDisk()) return;
    QGuiApplication::clipboard()->setText(toQString(entry.getPath()));
}

void ProjectBrowser::onCopyFileName() {
    const FileEntry& entry = getSelectedFileEntry();
    QGuiApplication::clipboard()->setText(entry.text(0));
}

void ProjectBrowser::onCopyDirPath() {
    const DirectoryEntry& entry = getSelectedDirectory();
    QGuiApplication::clipboard()->setText(toQString(entry.getPath()));
}

void ProjectBrowser::onCopyDirName() {
    const DirectoryEntry& entry = getSelectedDirectory();
    QGuiApplication::clipboard()->setText(entry.text(0));
}

ProjectBrowser::FileEntry& ProjectBrowser::getSelectedFileEntry() const noexcept {
    return *debug_cast<ProjectBrowser::FileEntry*>(currentItem());
}

ProjectBrowser::DirectoryEntry& ProjectBrowser::getSelectedDirectory() const noexcept {
    return *debug_cast<ProjectBrowser::DirectoryEntry*>(currentItem());
}

void ProjectBrowser::linkFileToAncestor(FileEntry* file_item, const std::filesystem::path file_path) {
    assert(std::filesystem::is_regular_file(file_path));

    QTreeWidgetItem* root = invisibleRootItem();
    QString stale_root_path_str = root->data(0, Qt::UserRole).toString();
    std::filesystem::path stale_root_path = toCppPath(stale_root_path_str);
    std::filesystem::path folder_path = file_path.parent_path();

    //Link the file to it's folder
    auto result = files_in_filesystem.find(folder_path);
    if(result != files_in_filesystem.end()){
        //Folder already exists
        QTreeWidgetItem* preexisting_folder_entry = result->second;
        preexisting_folder_entry->addChild(file_item);
        return;
    }

    QList<QTreeWidgetItem*> taken_children;

    //Find common ancestor of root directory and folder
    if(stale_root_path_str.isEmpty()){
        //Browser already showing different drives, do nothing
    }else if(stale_root_path.root_name() != file_path.root_name()){
        //These files come from different drives; change root to nothing since no common ancestor
        files_in_filesystem.erase(stale_root_path);
        root->setData(0, Qt::UserRole, QString());
        taken_children = root->takeChildren();
    }else{
        //Check if the root needs to change
        auto root_path_str = stale_root_path.u8string();
        auto folder_path_str = folder_path.u8string();
        const size_t root_path_size = root_path_str.size();

        size_t uncommon_index = std::min(root_path_size, folder_path_str.size());
        for(size_t i = 0; i < uncommon_index; i++)
            if(root_path_str[i] != folder_path_str[i]){
                uncommon_index = i;
                break;
            }

        //Root needs to move up
        if(uncommon_index < root_path_size){
            std::filesystem::path new_root_path = stale_root_path.parent_path();
            while(new_root_path.u8string().size() > uncommon_index){
                assert(new_root_path.has_parent_path());
                new_root_path = new_root_path.parent_path();
            }

            taken_children = root->takeChildren();
            files_in_filesystem.erase(stale_root_path);
            files_in_filesystem[new_root_path] = root;
            root->setData(0, Qt::UserRole, toQString(new_root_path));
        }
    }

    linkItemToExistingAncestor(file_item, file_path);

    if(!taken_children.empty()){
        linkItemToExistingAncestor(taken_children.back(), stale_root_path / "fake");
        taken_children.pop_back();
        QTreeWidgetItem* entry_for_stale_root = files_in_filesystem[stale_root_path];
        entry_for_stale_root->addChildren(taken_children);
        setCurrentItem(currently_viewed_file);
    }
}

void ProjectBrowser::linkItemToExistingAncestor(QTreeWidgetItem* item, std::filesystem::path path) {
    path = path.parent_path();
    auto parent_result = files_in_filesystem.insert({path, nullptr});
    while(parent_result.second){
        DirectoryEntry* new_item = new DirectoryEntry;
        setRootIsDecorated(true); //Effectively adding padding for expand/collapse arrows
        new_item->addChild(item);
        item = new_item;
        new_item->setPath(path);
        parent_result.first->second = item;

        auto parent_path = path.parent_path();
        if(parent_path != path){ //has_parent_path() lies, causing an infinite loop
            path = parent_path;
            parent_result = files_in_filesystem.insert({path, nullptr});
        }else{
            invisibleRootItem()->addChild(item);
            item->setText(0, toQString(*path.begin()));
            return;
        }
    }

    parent_result.first->second->addChild(item);
}

}
