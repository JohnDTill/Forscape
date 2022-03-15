#include "mainwindow.h"

#include <hope_logging.h>
#include <QApplication>

int main(int argc, char* argv[]){
    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    Hope::initLogging();
    Hope::logger->info("/* APP_SESSION_START */");
    MainWindow w;

    w.show();
    w.resize(w.width()+1, w.height()+1);
    auto code = a.exec();
    Hope::logger->info("/* APP_SESSION_END */");

    return code;
}
