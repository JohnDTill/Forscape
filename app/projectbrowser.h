#ifndef PROJECTBROWSER_H
#define PROJECTBROWSER_H

#include <filesystem>
#include <forscape_common.h>
#include <QTreeWidget>

class MainWindow;

namespace Forscape {

class ProjectBrowser : public QTreeWidget {
    Q_OBJECT

public:
    ProjectBrowser(QWidget* parent, MainWindow* main_window);
    void setProject(Forscape::Typeset::Model* model);
    void addProject(const std::filesystem::path& entry_point_path);
    void addFile(const std::filesystem::path& file_path);
    void addFile(Forscape::Typeset::Model* model);
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

private:
    class FileEntry;
    class DirectoryEntry;
    FileEntry& getSelectedFileEntry() const noexcept;
    DirectoryEntry& getSelectedDirectory() const noexcept;
    void linkFileToAncestor(FileEntry* file_item, const std::filesystem::path file_path);
    void linkItemToExistingAncestor(QTreeWidgetItem* item, std::filesystem::path path);

    FORSCAPE_UNORDERED_MAP<std::filesystem::path, QTreeWidgetItem*> entries;
    QTreeWidgetItem* currently_viewed_item; //Used to give affordance
    MainWindow* main_window;
};

}

#endif // PROJECTBROWSER_H
