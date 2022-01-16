#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[]){
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;

    //Wait for any warning popups to be awknowledged
    do{
        QGuiApplication::processEvents();
    } while(QGuiApplication::allWindows().size());

    w.show();
    w.resize(w.width()+1, w.height()+1);
    return a.exec();
}
