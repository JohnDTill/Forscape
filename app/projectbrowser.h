#ifndef PROJECTBROWSER_H
#define PROJECTBROWSER_H

#include <QTreeWidget>
#include <filesystem>
#include <forscape_common.h>

class MainWindow;

namespace Forscape {

class ProjectBrowser : public QTreeWidget {
    Q_OBJECT

public:
    ProjectBrowser(QWidget* parent, MainWindow* main_window);
    void setProject(Forscape::Typeset::Model* model);
    void addProject(const std::filesystem::path& entry_point_path);
    void addUnsavedFile(Forscape::Typeset::Model* model);
    void saveModel(Forscape::Typeset::Model* saved_model, const std::filesystem::path& std_path);
    void updateProjectBrowser();
    void addProjectEntry(Forscape::Typeset::Model* model);
    void populateWithNewProject(Forscape::Typeset::Model* model);
    void setCurrentlyViewed(Forscape::Typeset::Model* model);
    void clear() noexcept;

private slots:
    void onFileClicked(QTreeWidgetItem* item, int column);
    void onFileClicked();
    void onShowInExplorer();
    void onDeleteFile();
    void onRightClick(const QPoint& pos);
    void onDirectoryRenamed();
    void onFileRenamed();
    void expandDirectory();
    void onCopyFilePath();
    void onCopyFileName();
    void onCopyDirPath();
    void onCopyDirName();

private:
    class FileEntry;
    class DirectoryEntry;
    FileEntry& getSelectedFileEntry() const noexcept;
    DirectoryEntry& getSelectedDirectory() const noexcept;
    void linkFileToAncestor(FileEntry* file_item, const std::filesystem::path file_path);
    void linkItemToExistingAncestor(QTreeWidgetItem* item, std::filesystem::path path);

    FORSCAPE_UNORDERED_MAP<std::filesystem::path, QTreeWidgetItem*> files_in_filesystem;
    FORSCAPE_UNORDERED_MAP<Typeset::Model*, QTreeWidgetItem*> files_in_memory;
    FileEntry* currently_viewed_file = nullptr; //Used to give affordance
    MainWindow* main_window;
};

}

#endif // PROJECTBROWSER_H
