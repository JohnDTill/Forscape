#include "projectbrowser.h"

#include "mainwindow.h"

#include <forscape_program.h>
#include <typeset_model.h>
#include <qt_compatability.h>

#include <QAction>
#include <QDir>
#include <QFileIconProvider>
#include <QMenu>
#include <QProcess>

//DO THIS: the project view is unintuitive. Allow multiple open projects?
//DO THIS: need assurance that the viewing history remains valid as files are deleted, renamed, w/e
//DO THIS: the project browser should probably have an undo stack

Q_DECLARE_METATYPE(Forscape::Typeset::Model*); //EVENTUALLY: this is only for compability with old versions

static QIcon main_icon;
static QIcon file_icon;
static QIcon folder_icon;

static Forscape::Typeset::Model* getModelForEntry(const QTreeWidgetItem* item) noexcept {
    assert(item->childCount() == 0);
    return item->data(0, Qt::UserRole).value<Forscape::Typeset::Model*>();
}

static bool itemIsFile(const QTreeWidgetItem* item) noexcept {
    return item->childCount() == 0;
}

static bool isSavedToDisk(const QTreeWidgetItem* item) noexcept {
    return !getModelForEntry(item)->path.empty();
}

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
    QTreeWidgetItem* main_file = new QTreeWidgetItem(root);
    main_file->setText(0, toQString(std_path.filename()));
    main_file->setIcon(0, main_icon);
    main_file->setData(0, Qt::UserRole, QVariant::fromValue(model));
    model->project_browser_entry = main_file;
    project_browser_entries[std_path] = main_file;
    const std::filesystem::path parent_path = std_path.parent_path();
    project_browser_entries[parent_path] = root;
    root->setData(0, Qt::UserRole, toQString(parent_path));
    currently_viewed_item = main_file;
    QFont normal_font = font();
    QFont bold_font = normal_font;
    bold_font.setBold(true);
    currently_viewed_item->setFont(0, bold_font);
}

void ProjectBrowser::addFile(Forscape::Typeset::Model* model) {
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    item->setText(0, "untitled");
    item->setIcon(0, file_icon);
    item->setData(0, Qt::UserRole, QVariant::fromValue(model));
    sortItems(0, Qt::SortOrder::AscendingOrder);
    model->project_browser_entry = item;
}

void ProjectBrowser::saveModel(Forscape::Typeset::Model* saved_model, const std::filesystem::path& std_path) {
    std::filesystem::path old_path = saved_model->path;
    const bool create_new_file = old_path.empty();
    const bool rename_file = saved_model->path != std_path && !create_new_file;

    if(rename_file){
        //EVENTUALLY: this is a hacky solution to keep the old file, which is likely referenced in code
        Forscape::Program::instance()->source_files.erase(old_path);
        project_browser_entries.erase(old_path);
        Forscape::Program::instance()->openFromAbsolutePath(old_path);
    }

    if(create_new_file || rename_file){
        saved_model->path = std_path;
        QTreeWidgetItem* item = saved_model->project_browser_entry;
        item->setText(0, toQString(std_path.filename()));
        auto result = project_browser_entries.insert({std_path, item});
        if(!result.second){
            //Saving over existing project file
            QTreeWidgetItem* overwritten = result.first->second;
            Forscape::Typeset::Model* overwritten_model = getModelForEntry(overwritten);
            delete overwritten_model;
            /* DO THIS
            modified_files.erase(overwritten_model);
            for(auto& entry : viewing_chain)
                if(entry.model == overwritten_model)
                    entry.model = saved_model;
            */
            removeItemWidget(overwritten, 0);
            delete overwritten;
            result.first->second = item;
        }else{
            //New file created
            invisibleRootItem()->removeChild(item);
            linkFileToAncestor(item, std_path);
        }
    }
    sortItems(0, Qt::SortOrder::AscendingOrder);
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
    assert(project_browser_entries.find(path) == project_browser_entries.end());

    QTreeWidgetItem* tree_item = new QTreeWidgetItem;
    model->project_browser_entry = tree_item;
    tree_item->setData(0, Qt::UserRole, QVariant::fromValue(model));
    tree_item->setText(0, toQString(path.filename()));
    tree_item->setIcon(0, file_icon);
    project_browser_entries[path] = tree_item;

    model->write_time = std::filesystem::file_time_type::clock::now();

    linkFileToAncestor(tree_item, path);
}

void ProjectBrowser::populateWithNewProject(Forscape::Typeset::Model* model) {
    clear();
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    item->setText(0, "untitled");
    item->setIcon(0, main_icon);
    item->setData(0, Qt::UserRole, QVariant::fromValue(model));
    sortItems(0, Qt::SortOrder::AscendingOrder);
    model->project_browser_entry = item;
    currently_viewed_item = item;
    item->setSelected(true);
    QFont f = font();
    f.setBold(true);
    item->setFont(0, f);
}

void ProjectBrowser::setCurrentlyViewed(Forscape::Typeset::Model* model) {
    QFont normal_font = font();
    QFont bold_font = normal_font;
    bold_font.setBold(true);

    //Remove highlighting on the old item
    currently_viewed_item->setFont(0, QFont());

    //Highlight the new item
    currently_viewed_item = model->project_browser_entry;
    currently_viewed_item->setFont(0, bold_font);
    setCurrentItem(currently_viewed_item);
}

void ProjectBrowser::clear() noexcept {
    QTreeWidget::clear();
    project_browser_entries.clear();
}

void ProjectBrowser::onFileClicked(QTreeWidgetItem* item, int column) {
    if(itemIsFile(item)) main_window->viewModel(getModelForEntry(item));
}

void ProjectBrowser::onFileClicked() {
    onFileClicked(currentItem(), 0);
}

void ProjectBrowser::onShowInExplorer() {
    const QTreeWidgetItem* item = currentItem();

    QStringList args;

    if(item->childCount() == 0){
        assert(isSavedToDisk(item));
        auto m = getModelForEntry(item);
        args << "/select," << QDir::toNativeSeparators(toQString(m->path));
    }else{
        QString q_path = item->data(0, Qt::UserRole).toString();
        args << QDir::toNativeSeparators(q_path);
    }

    QProcess* process = new QProcess(this);
    process->start("explorer.exe", args);
}

void ProjectBrowser::onDeleteFile() {
    QTreeWidgetItem* item = currentItem();
    auto m = getModelForEntry(item);
    assert(itemIsFile(item));
    if(!m->notOnDisk()) std::filesystem::remove(m->path);

    assert(m != Forscape::Program::instance()->program_entry_point);
    delete m;
    delete item;

    //DO THIS: this has implications for the viewing buffer

    //editor->updateModel(); //DO THIS: update the model now that imports may be broken
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
    }else if(item->childCount() == 0){
        //Item is file
        QAction* open_file = menu.addAction(tr("Open File"));
        open_file->setToolTip(tr("Open the selected file in the editor"));
        connect(open_file, SIGNAL(triggered(bool)), this, SLOT(onFileClicked()));

        if(isSavedToDisk(item)){
            QAction* show_in_explorer = menu.addAction(tr("Show in Explorer"));
            show_in_explorer->setToolTip(tr("Show in the OS file browser"));
            connect(show_in_explorer, SIGNAL(triggered(bool)), this, SLOT(onShowInExplorer()));
        }

        auto m = getModelForEntry(item);
        if(m != Forscape::Program::instance()->program_entry_point){
            QAction* delete_file = menu.addAction(tr("Delete"));
            delete_file->setToolTip(tr("Erase this file"));
            connect(delete_file, SIGNAL(triggered(bool)), this, SLOT(onDeleteFile()));
        }

        QAction* rename_file = menu.addAction(tr("Rename"));
        rename_file->setToolTip("Change the filename");
        //DO THIS
    }else{
        QAction* show_in_explorer = menu.addAction(tr("Show in Explorer"));
        show_in_explorer->setToolTip(tr("Show in the OS file browser"));
        connect(show_in_explorer, SIGNAL(triggered(bool)), this, SLOT(onShowInExplorer()));

        QAction* rename_folder = menu.addAction(tr("Rename"));
        rename_folder->setToolTip("Change the directory name");
        //DO THIS
    }

    menu.exec(mapToGlobal(pos));
}

void ProjectBrowser::linkFileToAncestor(QTreeWidgetItem* file_item, const std::filesystem::path file_path) {
    assert(std::filesystem::is_regular_file(file_path));

    QTreeWidgetItem* root = invisibleRootItem();
    QString stale_root_path_str = root->data(0, Qt::UserRole).toString();
    std::filesystem::path stale_root_path = toCppPath(stale_root_path_str);
    std::filesystem::path folder_path = file_path.parent_path();

    //Link the file to it's folder
    auto result = project_browser_entries.find(folder_path);
    if(result != project_browser_entries.end()){
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
        project_browser_entries.erase(stale_root_path);
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
            project_browser_entries.erase(stale_root_path);
            project_browser_entries[new_root_path] = root;
            root->setData(0, Qt::UserRole, toQString(new_root_path));
        }
    }

    linkItemToExistingAncestor(file_item, file_path);

    if(!taken_children.empty()){
        linkItemToExistingAncestor(taken_children.back(), stale_root_path / "fake");
        taken_children.pop_back();
        QTreeWidgetItem* entry_for_stale_root = project_browser_entries[stale_root_path];
        entry_for_stale_root->addChildren(taken_children);
        setCurrentItem(currently_viewed_item);
    }
}

void ProjectBrowser::linkItemToExistingAncestor(QTreeWidgetItem* item, std::filesystem::path path) {
    path = path.parent_path();
    auto parent_result = project_browser_entries.insert({path, nullptr});
    while(parent_result.second){
        QTreeWidgetItem* new_item = new QTreeWidgetItem;
        new_item->addChild(item);
        item = new_item;
        item->setText(0, toQString(path.filename()));
        item->setData(0, Qt::UserRole, toQString(path));
        item->setIcon(0, folder_icon);
        item->setExpanded(true);
        parent_result.first->second = item;

        auto parent_path = path.parent_path();
        if(parent_path != path){ //has_parent_path() lies, causing an infinite loop
            path = parent_path;
            parent_result = project_browser_entries.insert({path, nullptr});
        }else{
            invisibleRootItem()->addChild(item);
            item->setText(0, toQString(*path.begin()));
            return;
        }
    }

    parent_result.first->second->addChild(item);
}
