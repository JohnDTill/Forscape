#ifndef TYPESET_MATRIX_H
#define TYPESET_MATRIX_H

#include "typeset_command.h"
#include "typeset_construct.h"
#include "typeset_controller.h"
#include "typeset_model.h"
#include "typeset_subphrase.h"

#ifndef FORSCAPE_TYPESET_HEADLESS
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIntValidator>
#include <QLineEdit>
#endif

#ifndef NDEBUG
#include <iostream>
#endif

namespace Forscape {

namespace Typeset {

class Matrix final : public Construct { 
public:
    uint16_t rows;
    uint16_t cols;

    static constexpr double ELEMENT_HPADDING = 5;
    static constexpr double ELEMENT_VPADDING = 4;
    static constexpr double MATRIX_LPADDING = 2;
    static constexpr double MATRIX_RPADDING = 3;
    static constexpr double BRACKET_WIDTH = 2;
    static constexpr double BRACKET_HOFFSET = 3;
    static constexpr double BRACKET_TOP_OFFSET = 2;
    static constexpr double BRACKET_BOT_OFFSET = 0;
    static constexpr double BRACKET_VSHIFT = 1;

    std::vector<double> W; //EVENTUALLY: can eliminate these altogether when dimensions are in parent frame
    std::vector<double> U;
    std::vector<double> D;

    Matrix(uint16_t rows, uint16_t cols)
        : rows(rows), cols(cols) {
        setupNAargs(rows*cols);

        W.resize(cols);
        U.resize(rows);
        D.resize(rows);
    }

    virtual void writeArgs(std::string& out, size_t& curr) const noexcept override {
        out[curr++] = static_cast<uint8_t>(rows);
        out[curr++] = static_cast<uint8_t>(cols);
    }

    virtual size_t dims() const noexcept override { return 2; }
    virtual char constructCode() const noexcept override { return MATRIX; }

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id >= cols ? arg(caller->id - cols)->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id + cols < numArgs() ? arg(caller->id + cols)->textLeftOf(x) : next();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        std::fill(W.begin(), W.begin()+cols, 0);
        std::fill(U.begin(), U.begin()+rows, 0);
        std::fill(D.begin(), D.begin()+rows, 0);

        for(size_t i = 0; i < rows; i++){
            for(size_t j = 0; j < cols; j++){
                Subphrase* e = arg(j+i*cols);
                W[j] = std::max( W[j], e->width );
                U[i] = std::max( U[i], e->above_center );
                D[i] = std::max( D[i], e->under_center );
            }
        }

        width = MATRIX_LPADDING + MATRIX_RPADDING + 2*BRACKET_HOFFSET + (cols-1)*ELEMENT_HPADDING;
        for(size_t i = 0; i < cols; i++)
            width += W[i];

        double height = BRACKET_TOP_OFFSET + BRACKET_BOT_OFFSET + (rows-1)*ELEMENT_VPADDING;
        for(size_t i = 0; i < rows; i++)
            height += U[i] + D[i];
        above_center = under_center = height/2;
    }

    virtual void updateChildPositions() noexcept override {
        double yc = y + BRACKET_TOP_OFFSET;
        for(uint16_t i = 0; i < rows; i++){
            double xc = x + MATRIX_LPADDING + BRACKET_HOFFSET;
            for(uint16_t j = 0; j < cols; j++){
                Subphrase* e = arg(j+i*cols);
                e->x = xc + (W[j]-e->width)/2;
                e->y = yc + U[i]-e->above_center;
                xc += W[j]+ELEMENT_HPADDING;
            }
            yc += U[i]+D[i]+ELEMENT_VPADDING;
        }
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.setTypeIfAppropriate(SEM_DEFAULT);

        double h = height();

        painter.drawLine(x+MATRIX_LPADDING, y+BRACKET_VSHIFT, BRACKET_WIDTH, 0);
        painter.drawLine(x+MATRIX_LPADDING, y+BRACKET_VSHIFT, 0, h);
        painter.drawLine(x+MATRIX_LPADDING, y+BRACKET_VSHIFT+h, BRACKET_WIDTH, 0);

        painter.drawLine(x+width-BRACKET_WIDTH-MATRIX_RPADDING, y+BRACKET_VSHIFT, BRACKET_WIDTH, 0);
        painter.drawLine(x+width-MATRIX_RPADDING, y+BRACKET_VSHIFT, 0, h);
        painter.drawLine(x+width-BRACKET_WIDTH-MATRIX_RPADDING, y+BRACKET_VSHIFT+h, BRACKET_WIDTH, 0);
    }

    static std::vector<ContextAction> actions;

    virtual const std::vector<ContextAction>& getContextActions(Subphrase* child) const noexcept override {
        if(child == nullptr) return no_actions;

        actions[4].enabled = (rows > 1);
        actions[5].enabled = (cols > 1);

        return actions;
    }

    void insertRow(size_t row, const std::vector<Subphrase*>& inserted){
        size_t start = row*cols;

        //Shift everything beyond this row down
        size_t old_size = numArgs();
        args.resize(old_size + cols);
        for(size_t i = old_size-1; i >= start; i--){
            args[i+cols] = args[i];
            args[i]->id += cols;
            if(i==0) break;
        }

        //Initialize the new row
        for(size_t i = start; i < start+cols; i++){
            args[i] = inserted[i-start];
            args[i]->id = i;
        }

        rows++;

        if(rows > U.size()){
            U.resize(rows);
            D.resize(rows);
        }
    }

    void removeRow(size_t row) noexcept {
        size_t start = (row+1)*cols;

        //Move subsequent elements up
        for(size_t i = start; i < numArgs(); i++){
            args[i-cols] = args[i];
            args[i]->id = i-cols;
        }

        args.resize(numArgs() - cols);
        rows--;
    }

    void insertCol(size_t col, const std::vector<Subphrase*>& inserted){
        args.resize(numArgs() + rows);

        //Move the old data
        for(size_t i = rows-1; i < rows; i--){
            for(size_t j = cols-1; j >= col; j--){
                size_t index = i*cols + j;
                args[index+i+1] = args[index];
                args[index]->id = index+i+1;
                if(j==0) break;
            }
            for(size_t j = col-1; j < col; j--){
                size_t index = i*cols + j;
                args[index+i] = args[index];
                args[index]->id = index+i;
            }
        }

        cols++;

        //Initialize the new column
        for(size_t i = col; i < numArgs(); i += cols){
            args[i] = inserted[i/cols];
            args[i]->id = i;
        }

        if(cols > W.size()) W.resize(cols);
    }

    void removeCol(size_t col){
        //Move subsequent elements left
        for(size_t i = 0; i < rows; i++){
            for(size_t j = 0; j < col; j++){
                size_t new_index = i*cols+j - i;
                size_t old_index = i*cols+j;
                args[new_index] = args[old_index];
                args[new_index]->id = new_index;
            }

            for(size_t j = col+1; j < cols; j++){
                size_t new_index = i*cols+j - i - 1;
                size_t old_index = i*cols+j;
                args[new_index] = args[old_index];
                args[new_index]->id = new_index;
            }
        }

        args.resize(numArgs() - rows);

        cols--;
    }

    template<bool is_insert>
    class CmdRow : public Command{
    private:
        bool owning;
        Matrix& mat;
        const size_t row;
        std::vector<Subphrase*> data;

        public:
            CmdRow(Matrix& mat, size_t row)
                : mat(mat), row(row){

                if(is_insert){
                    for(size_t i = 0; i < mat.cols; i++){
                        data.push_back( new Subphrase );
                        data.back()->setParent(&mat);
                    }
                }else{
                    for(size_t i = 0; i < mat.cols; i++){
                        data.push_back( mat.arg(mat.cols*row+i) );
                    }
                }
            }

            ~CmdRow(){
                if(owning) for(Subphrase* s : data) delete s;
            }

        private:
            void insert(Controller& c){
                owning = false;
                mat.insertRow(row, data);
                c.setBothToBackOf(mat.arg(mat.cols*(row+1)-1)->back());
            }

            void remove(Controller& c) noexcept {
                owning = true;
                mat.removeRow(row);
                c.setBothToFrontOf(mat.arg(row > 0 ? mat.cols*(row-1) : 0)->front());
            }

            virtual void redo(Controller& c) override final{
                if(is_insert) insert(c);
                else remove(c);
            }

            virtual void undo(Controller& c) override final{
                if(is_insert) remove(c);
                else insert(c);
            }
    };

    template<bool is_insert>
    class CmdCol : public Command{
    private:
        bool owning;
        Matrix& mat;
        const size_t col;
        std::vector<Subphrase*> data;

    public:
        CmdCol(Matrix& mat, size_t col)
            : mat(mat), col(col){

            if(is_insert){
                for(size_t i = 0; i < mat.rows; i++){
                    data.push_back( new Subphrase );
                    data.back()->setParent(&mat);
                }
            }else{
                for(size_t i = 0; i < mat.rows; i++){
                    data.push_back( mat.arg(i*mat.cols + col) );
                }
            }
        }

        ~CmdCol(){
            if(owning) for(Subphrase* s : data) delete s;
        }

    private:
        void insert(Controller& c){
            owning = false;
            mat.insertCol(col, data);
            c.setBothToFrontOf(mat.arg(col)->front());
        }

        void remove(Controller& c) noexcept {
            owning = true;
            mat.removeCol(col);
            if(col > 0) c.setBothToBackOf(mat.arg(col-1)->back());
            else c.setBothToFrontOf(mat.arg(0)->front());
        }

    protected:
        virtual void redo(Controller& c) override final{
            if(is_insert) insert(c);
            else remove(c);
        }

        virtual void undo(Controller& c) override final{
            if(is_insert) remove(c);
            else insert(c);
        }
    };

    class SetSize : public Command {
    private:
        bool enacted;
        Matrix& mat;
        std::vector<Subphrase*> original_data;
        std::vector<Subphrase*> modified_data;
        std::vector<Subphrase*> original_data_not_shared;
        std::vector<Subphrase*> modified_data_not_shared;
        const size_t original_rows;
        const size_t original_cols;
        const size_t modified_rows;
        const size_t modified_cols;

    public:
        SetSize(Matrix& mat, size_t rows, size_t cols)
            : mat(mat), original_rows(mat.rows), original_cols(mat.cols), modified_rows(rows), modified_cols(cols) {
            original_data = mat.args;

            modified_data.reserve(modified_rows*modified_cols);
            for(size_t i = 0; i < modified_rows; i++){
                for(size_t j = 0; j < modified_cols; j++){
                    if(i < original_rows && j < original_cols){
                        modified_data.push_back(original_data[i*original_cols+j]);
                    }else{
                        Subphrase* p = new Subphrase;
                        p->setParent(&mat);
                        modified_data.push_back(p);
                        modified_data_not_shared.push_back(p);
                    }
                }
                for(size_t j = modified_cols; j < original_cols; j++)
                    original_data_not_shared.push_back(original_data[i*original_cols+j]);
            }
            if(original_rows > modified_rows){
                const size_t start_index = modified_rows*original_cols;
                original_data_not_shared.insert(
                    original_data_not_shared.end(), original_data.cbegin() + start_index, original_data.cend());
            }
        }

        ~SetSize(){
            for(Subphrase* p : (enacted ? original_data_not_shared : modified_data_not_shared)) delete p;
        }

    protected:
        virtual void redo(Controller& c) override final{
            enacted = true;
            mat.rows = modified_rows;
            mat.cols = modified_cols;
            mat.args = modified_data;
            for(size_t i = mat.rows*mat.cols; i-->0;) mat.arg(i)->id = i;

            mat.W.resize(mat.cols);
            mat.U.resize(mat.rows);
            mat.D.resize(mat.rows);

            c.setBothToFrontOf(mat.next());
        }

        virtual void undo(Controller& c) override final{
            enacted = false;
            mat.rows = original_rows;
            mat.cols = original_cols;
            mat.args = original_data;
            for(size_t i = mat.rows*mat.cols; i-->0;) mat.arg(i)->id = i;

            mat.W.resize(mat.cols);
            mat.U.resize(mat.rows);
            mat.D.resize(mat.rows);

            c.setBothToFrontOf(mat.next());
        }
    };

    template<bool insert, bool offset>
    static void rowCmd(Construct* con, Controller& c, Subphrase* child){
        assert(dynamic_cast<Matrix*>(con));
        assert(child != nullptr);
        Matrix* m = static_cast<Matrix*>(con);
        size_t clicked_row = child->id / m->cols;
        Command* cmd = new CmdRow<insert>(*m, clicked_row+offset);
        c.getModel()->mutate(cmd, c);
    }

    template<bool insert, bool offset>
    static void colCmd(Construct* con, Controller& c, Subphrase* child){
        assert(dynamic_cast<Matrix*>(con));
        assert(child != nullptr);
        Matrix* m = static_cast<Matrix*>(con);
        size_t clicked_col = child->id % m->cols;
        Command* cmd = new CmdCol<insert>(*m, clicked_col+offset);
        c.getModel()->mutate(cmd, c);
    }

    static void promptSize(Construct* con, Controller& c, Subphrase*){
        assert(dynamic_cast<Matrix*>(con));
        Matrix* m = static_cast<Matrix*>(con);

        QDialog dialog(QApplication::focusWidget());
        dialog.setWindowTitle("Matrix size");
        dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        QFormLayout form(&dialog);

        QLineEdit* rowEdit = new QLineEdit(&dialog);
        rowEdit->setText(QString::number(m->rows));
        rowEdit->setValidator(new QIntValidator(1, 255, rowEdit));
        form.addRow("Rows", rowEdit);

        QLineEdit* colEdit = new QLineEdit(&dialog);
        colEdit->setText(QString::number(m->cols));
        colEdit->setValidator(new QIntValidator(1, 255, colEdit));
        form.addRow("Cols", colEdit);

        QDialogButtonBox buttonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
        form.addRow(&buttonBox);

        QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
        QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

        const auto code = dialog.exec();
        switch (code) {
            case QDialogButtonBox::Cancel:
            case QDialogButtonBox::StandardButton::NoButton:
                return;
            case 1: break;
            default: assert(false);
        }

        bool success;
        uint rows = rowEdit->text().toUInt(&success);
        if(!success || rows == 0 || rows > 255) return;
        uint cols = colEdit->text().toUInt(&success);
        if(!success || cols == 0 || cols > 255) return;
        if(rows == m->rows && cols ==m->cols) return;

        Command* cmd = new SetSize(*m, rows, cols);
        c.getModel()->mutate(cmd, c);
    }
    #endif
};

#ifndef FORSCAPE_TYPESET_HEADLESS
inline std::vector<Construct::ContextAction> Matrix::actions {
    ContextAction("Create row below", rowCmd<true, true>),
    ContextAction("Create row above", rowCmd<true, false>),
    ContextAction("Create col right", colCmd<true, true>),
    ContextAction("Create col left", colCmd<true, false>),
    ContextAction("Delete row", rowCmd<false, false>),
    ContextAction("Delete col", colCmd<false, false>),
    ContextAction("Set size...", promptSize),
};
#endif

}

}

#endif // TYPESET_MATRIX_H
