#include "mainwindow.h"

#include <typeset_themes.h>
#include <QApplication>
#include <QFileInfo>

#ifdef _WIN32
#pragma comment(linker, "/ENTRY:wmainCRTStartup")
int wmain(int argc, wchar_t* argv[]){
#else
int main(int argc, char* argv[]){
#endif
    const bool file_supplied = (argc == 2);

    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QApplication a(argc, nullptr);
    Forscape::Typeset::setPreset(Forscape::Typeset::PRESET_DEFAULT);
    MainWindow w;
    if(file_supplied){
        #ifdef _WIN32
        QString supplied_path = QString::fromWCharArray(argv[1]);
        #else
        QString supplied_path(argv[1]);
        #endif
        QString abs_path = QFileInfo(supplied_path).absoluteFilePath();
        w.open(abs_path);
    }
    w.show();
    auto code = a.exec();

    return code;
}
