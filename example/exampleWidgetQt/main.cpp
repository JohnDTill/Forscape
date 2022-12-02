#include "mainwindow.h"

#include <typeset_themes.h>
#include <QApplication>

int main(int argc, char* argv[]){
    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    Hope::Typeset::setPreset(Hope::Typeset::PRESET_DEFAULT);
    MainWindow w;
    w.show();
    auto code = a.exec();

    return code;
}
