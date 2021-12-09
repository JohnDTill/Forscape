#ifndef QGRAPHVIZCALL_H
#define QGRAPHVIZCALL_H

#include <QDebug>
#include <QFile>
#include <QGraphicsView>
#include <QProcess>
#include <QWheelEvent>
#include <QtSvg/QGraphicsSvgItem>

namespace QGraphvizCall{

static bool createSvg(const QString& source){
    QFile dot_file("temp.dot");
    if(!dot_file.open(QFile::WriteOnly)){
        qDebug() << "Failed to open \"temp.dot\" in showDot()";
        return false;
    }
    dot_file.write(source.toUtf8());
    dot_file.close();

    QProcess process;
    #ifndef NDEBUG
    process.start("dot", {"-V"});
    process.waitForFinished();
    if(process.error() == QProcess::FailedToStart){
        qDebug() << "\"dot\" command not found."
                    "Graphviz is not installed or is not on the PATH.";
        return false;
    }
    #endif

    process.start("dot", {"-Tsvg", "temp.dot", "-o", "temp.svg"});
    process.waitForFinished();
    QFile::remove("temp.dot");
    if(!QFile::exists("temp.svg")){
        qDebug() << "QGraphvizCaller: Bad DOT code:" << process.readAllStandardError();
        return false;
    }
    process.close();

    return true;
}

inline QGraphicsScene* getGraphvizScene(const QString& source){
    if(!createSvg(source)) return nullptr;

    QGraphicsSvgItem* item = new QGraphicsSvgItem("temp.svg");
    QFile::remove("temp.svg");

    QGraphicsScene* scene = new QGraphicsScene();
    scene->addItem(item);

    return scene;
}

class GraphVizView : public QGraphicsView{
public:
    inline GraphVizView(QWidget* parent = nullptr) : QGraphicsView(parent){
        scale(2,2);
        setMinimumSize(QSize(300,200));
        setDragMode(QGraphicsView::DragMode::ScrollHandDrag);
        setScene(new QGraphicsScene(this));
    }

    inline GraphVizView(QGraphicsScene* scene) : QGraphicsView(scene){
        scale(2,2);
        setMinimumSize(QSize(300,200));
        setDragMode(QGraphicsView::DragMode::ScrollHandDrag);
    }
	
    inline void update(const QString& source){
        scene()->clear();

        if(!createSvg(source)) return;

        QGraphicsSvgItem* item = new QGraphicsSvgItem("temp.svg");
        QFile::remove("temp.svg");

        scene()->addItem(item);
        scene()->setSceneRect(item->boundingRect());
    }
	
private:
    inline virtual void wheelEvent(QWheelEvent* event) override{
        if( event->modifiers() == Qt::ControlModifier ){
            constexpr qreal scale_factor = 0.85;
            if(event->angleDelta().y() > 0) scale(1/scale_factor, 1/scale_factor);
            else scale(scale_factor, scale_factor);
        }else{
            QGraphicsView::wheelEvent(event);
        }
    }
};

inline GraphVizView* show(const QString& source){
    if(QGraphicsScene* scene = getGraphvizScene(source)){
        GraphVizView* view = new GraphVizView(scene);

        view->setScene(scene);
        view->setWindowTitle("Graphviz Output");
        view->show();

        return view;
    }

    return nullptr;
}

}

#endif
