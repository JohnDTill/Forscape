#ifndef PROJECTBROWSER_H
#define PROJECTBROWSER_H

#include <filesystem>
#include <QTreeWidget>

class ProjectBrowser : public QTreeWidget {

public:
    ProjectBrowser(QWidget* parent = nullptr);
    void addProject(const std::filesystem::path& entry_point_path);
    void addFile(const std::filesystem::path& file_path);

};

#endif // PROJECTBROWSER_H
