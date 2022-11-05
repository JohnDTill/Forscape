#include "mainwindow.h"

#include <hope_logging.h>
#include <typeset_themes.h>
#include <QApplication>

int main(int argc, char* argv[]){
    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QApplication a(argc, argv);
    Hope::initLogging();
    Hope::logger->info("/* APP_SESSION_START */");
    Hope::Typeset::setPreset(Hope::Typeset::PRESET_DEFAULT);
    MainWindow w;
    w.show();
    auto code = a.exec();
    Hope::logger->info("/* APP_SESSION_END */");

    return code;
}
