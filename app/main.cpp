#include "mainwindow.h"

#include <typeset_themes.h>
#include <QApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QTranslator>

#ifdef _MSC_VER
#pragma comment(linker, "/ENTRY:wmainCRTStartup")
int wmain(int argc, wchar_t* argv[]){
#else
int main(int argc, char* argv[]){
#endif
    const bool file_supplied = (argc == 2);

    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, nullptr);

    //EVENTUALLY: get translations working
    QTranslator translator;
    if(translator.load(QLocale::system(), "qtbase", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        a.installTranslator(&translator);

    Forscape::Typeset::setPreset(Forscape::Typeset::PRESET_DEFAULT);
    MainWindow w;
    if(file_supplied){
        #ifdef _MSC_VER
        QString supplied_path = QString::fromWCharArray(argv[1]);
        #else
        QString supplied_path(argv[1]);
        #endif
        QString abs_path = QFileInfo(supplied_path).absoluteFilePath();
        w.openProject(abs_path);
    }
    w.show();
    auto code = a.exec();

    return code;
}