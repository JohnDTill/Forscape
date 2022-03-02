#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[]){
    QCoreApplication::setApplicationName("Forscape");
    QCoreApplication::setOrganizationName("AutoMath");
    QCoreApplication::setOrganizationDomain("https://github.com/JohnDTill/Forscape");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;

    w.show();
    w.resize(w.width()+1, w.height()+1);
    return a.exec();
}
