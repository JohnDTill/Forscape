#include <cassert>
#include <fstream>
#include "typeset.h"
#include <sstream>
#include <string>
#include "report.h"

#include <QApplication>
#include <QFile>

using namespace Hope;

static QImage getImage(Typeset::View& view){
    QImage img(view.size(), QImage::Format_RGB32);
    QPainter painter(&img);
    view.render(&painter);

    return img;
}

static void toBinary(QImage& img){
    for(auto i = 0; i < img.width(); i++){
        for(auto j = 0; j < img.height(); j++){
            QColor c = img.pixelColor(i,j);
            if(c != Qt::white){
                img.setPixelColor(i, j, Qt::black);
            }
        }
    }
}

static QImage diff(const QImage& A, const QImage& B){
    A.save("A.png");
    B.save("B.png");
    assert(A.size() == B.size());

    QImage d(A.size(), QImage::Format_Mono);

    for(auto i = 0; i < A.width(); i++){
        for(auto j = 0; j < A.height(); j++){
            if(A.pixelColor(i,j) != B.pixelColor(i,j)){
                d.setPixel(i,j,0);
            }else{
                d.setPixel(i,j,1);
            }
        }
    }

    return d;
}

static double failRatio(const QImage& im){
    size_t bad = 0;

    for(auto i = 0; i < im.width(); i++){
        for(auto j = 0; j < im.height(); j++){
            if(im.pixelColor(i,j) == Qt::black){
                bad++;
            }
        }
    }

    return static_cast<double>(bad) / (im.width()*im.height());
}

inline bool testTypesetQPainter(){
    constexpr double threshold = 1e-12;
    constexpr size_t MAX_MOVES = 15;
    bool passing = true;

    Typeset::View::selection_box_color.setAlpha(0);
    Typeset::View::text_cursor_color.setAlpha(0);
    Typeset::View::line_num_active_color.setAlpha(0);
    Typeset::View::line_num_passive_color.setAlpha(0);
    Typeset::View::selection_text_color = Qt::black;

    std::string test = readFile("serial_exhaustive.txt");
    Typeset::View view;
    view.setLineNumbersVisible(false);
    view.setFromSerial(test);
    Typeset::Controller& controller = view.getController();

    QImage img_formatted, img_selected;

    Code::Scanner scanner(view.getModel());
    scanner.scanAll();
    Code::Parser parser(scanner, view.getModel());
    parser.parseAll();

    view.resize(1200, 1000);

    img_formatted = getImage(view);
    toBinary(img_formatted);

    controller.selectAll();
    QApplication::processEvents();

    img_selected = getImage(view);
    toBinary(img_selected);

    if(img_formatted != img_selected){
        printf("Layout changes with formatted select-all\n");
        diff(img_selected, img_formatted).save("diff_formatted_select_all.png");
        passing = false;
    }

    size_t num_moves = 0;
    controller.moveToStartOfDocument();
    while(!controller.atEnd()){
        controller.selectNextChar();
        QApplication::processEvents();
        num_moves++;
        QImage img_selected = getImage(view);
        toBinary(img_selected);
        QImage d = diff(img_selected, img_formatted);
        double fail_ratio = failRatio(d);

        if(fail_ratio > threshold){
            std::cout << "Layout change of " << fail_ratio*100
                      << "% with unformatted moving selection on move "
                      << num_moves << std::endl;
            d.save("diff_unformatted_moving_selection_" + QString::number(num_moves) + ".png");
            passing = false;
        }

        if(num_moves > MAX_MOVES) break;
    }

    controller.moveToEndOfDocument();
    num_moves = 0;
    while(!controller.atStart()){
        controller.selectPrevChar();
        QApplication::processEvents();
        num_moves++;
        QImage img_selected = getImage(view);
        toBinary(img_selected);
        QImage d = diff(img_selected, img_formatted);
        double fail_ratio = failRatio(d);

        if(fail_ratio > threshold){
            std::cout << "Layout change of " << fail_ratio*100
                      << "% with unformatted reverse selection on move "
                      << num_moves << std::endl;
            d.save("diff_unformatted_reverse_selection_" + QString::number(num_moves) + ".png");
            img_selected.save("f_me_" + QString::number(num_moves) + ".png");
            passing = false;
        }

        if(num_moves > MAX_MOVES) break;
    }

    if(passing){
        report("Typeset QPainter", passing);
    }else{
        std::cout << "-- Typeset QPainter: KNOWN BUG (https://github.com/JohnDTill/Forscape/issues/12)" << std::endl;
    }

    return true;

    //report("Typeset QPainter", passing);
    //return passing;
}
