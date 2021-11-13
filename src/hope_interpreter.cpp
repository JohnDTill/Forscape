#include "hope_interpreter.h"

#include <hope_parsenodetype.h>
#include <typeset_line.h>
#include <typeset_model.h>
#include <typeset_text.h>
#include <typeset_command_line.h>
#include <typeset_all_constructs.h>

#include <Eigen/Eigen>

#ifdef HOPE_EIGEN_UNSUPPORTED
#include <unsupported/Eigen/MatrixFunctions>
#endif

#ifndef NDEBUG
#include <iostream>
#endif

#include <QCoreApplication> //DO THIS - this is a daft until threading

namespace Hope {

namespace Code {

Interpreter::Interpreter(const ParseTree& parse_tree, Typeset::Model* model, Typeset::View* view, Typeset::View* console)
    : parse_tree(parse_tree), errors(model->errors), view(view), console(console) {
}

Typeset::Model* Interpreter::interpret(ParseNode root){
    stack.clear();

    output = Typeset::Model::fromSerial("");
    output->is_output = true;
    exit_mode = KEEP_GOING;
    active_closure = nullptr;
    if(console) console->setModel(output);

    assert(parse_tree.getEnum(root) == PN_BLOCK);
    blockStmt(root);

    if(!errors.empty()){
        output->appendLine();
        Error::writeErrors(errors, output, view);
    }
    output->calculateSizes();
    output->updateLayout();
    if(console) console->updateModel();

    return output;
}

void Interpreter::stop(){
    exit_mode = ERROR;
    output->appendLine();
    output->lastText()->str = "Stopped by user";
}

void Interpreter::interpretStmt(ParseNode pn){
    QCoreApplication::processEvents(); //DO THIS - this is a daft until threading

    switch (parse_tree.getEnum(pn)) {
        case PN_EQUAL: assignStmt(pn); break;
        case PN_ASSIGN: assignStmt(pn); break;
        case PN_REASSIGN: reassign(parse_tree.lhs(pn), parse_tree.rhs(pn)); break;
        case PN_ELEMENTWISE_ASSIGNMENT: elementWiseAssignment(pn); break;
        case PN_PRINT: printStmt(pn); break;
        case PN_ASSERT: assertStmt(pn); break;
        case PN_IF: ifStmt(pn); break;
        case PN_IF_ELSE: ifElseStmt(pn); break;
        case PN_WHILE: whileStmt(pn); break;
        case PN_FOR: forStmt(pn); break;
        case PN_BLOCK: blockStmt(pn); break;
        case PN_ALGORITHM: case PN_ALGORITHM_UP: algorithmStmt(pn); break;
        case PN_PROTOTYPE_ALG: stack.push(static_cast<void*>(nullptr), parse_tree.str(parse_tree.child(pn))); break;
        case PN_EXPR_STMT: callStmt(parse_tree.child(pn)); break;
        case PN_RETURN: returnStmt(pn); break;
        case PN_BREAK: exit_mode = static_cast<ExitMode>(exit_mode | BREAK); break;
        case PN_CONTINUE: exit_mode = static_cast<ExitMode>(exit_mode | CONTINUE); break;
        default: error(UNRECOGNIZED_STMT, pn);
    }
}

void Interpreter::printStmt(ParseNode pn){
    bool at_bottom = console && console->scrolledToBottom();

    for(size_t i = 0; i < parse_tree.numArgs(pn) && exit_mode == KEEP_GOING; i++)
        printNode(parse_tree.arg(pn, i));

    if(console){
        console->updateHighlighting();
        console->updateModel();

        if(at_bottom) console->scrollToBottom();
    }
}

void Interpreter::assertStmt(ParseNode pn){
    ParseNode child = parse_tree.child(pn);
    Value v = interpretExpr(child);
    if(v.index() != bool_index || !std::get<bool>(v))
        error(ASSERT_FAIL, child);
}

void Interpreter::assignStmt(ParseNode pn){
    ParseNode rhs = parse_tree.rhs(pn);

    Value v = interpretExpr(rhs);
    stack.push(v, parse_tree.str(parse_tree.lhs(pn)));
}

void Interpreter::whileStmt(ParseNode pn){
    while(exit_mode <= CONTINUE && evaluateCondition( parse_tree.arg(pn, 0) )){
        exit_mode = KEEP_GOING;
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(exit_mode < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::forStmt(ParseNode pn){
    size_t stack_size = stack.size();
    interpretStmt(parse_tree.arg(pn, 0));

    while(exit_mode <= CONTINUE && evaluateCondition( parse_tree.arg(pn, 1) )){
        exit_mode = KEEP_GOING;
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 3) );
        if(exit_mode < RETURN){
            stack.trim(stack_size);
            interpretStmt(parse_tree.arg(pn, 2));
        }
    }

    if(exit_mode < RETURN) stack.trim(stack_size);
}

void Interpreter::ifStmt(ParseNode pn){
    if(exit_mode == KEEP_GOING && evaluateCondition( parse_tree.arg(pn, 0) )){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(exit_mode < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::ifElseStmt(ParseNode pn){
    if(exit_mode == KEEP_GOING && evaluateCondition( parse_tree.arg(pn, 0) )){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(exit_mode < RETURN) stack.trim(stack_size);
    }else if(exit_mode == KEEP_GOING){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 2) );
        if(exit_mode < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::blockStmt(ParseNode pn){
    for(size_t i = 0; i < parse_tree.numArgs(pn) && exit_mode == KEEP_GOING; i++)
        interpretStmt(parse_tree.arg(pn, i));
}

void Interpreter::algorithmStmt(ParseNode pn){
    ParseNode name = parse_tree.arg(pn, 0);
    ParseNode captured = parse_tree.arg(pn, 1);
    ParseNode upvalues = parse_tree.arg(pn, 2);
    ParseNode params = parse_tree.arg(pn, 3);
    ParseNode body = parse_tree.arg(pn, 4);

    Algorithm alg(name, params, body);
    Closure* list;

    if(parse_tree.stackOffset(name) == std::numeric_limits<size_t>::max()){
        stack.push(alg, parse_tree.str(name));
        list = &std::get<Algorithm>(stack.back()).captured;
    }else{
        Value& place = readLocal(name);
        place = alg;
        list = &std::get<Algorithm>(place).captured;
    }

    if(captured != ParseTree::EMPTY){
        size_t N = parse_tree.numArgs(captured);

        for(size_t i = 0; i < N; i++){
            ParseNode capture = parse_tree.arg(captured, i);
            Value* v = new Value(read(capture));
            std::shared_ptr<void> ptr(v);
            list->push_back(ptr);
        }
    }

    for(size_t i = 0; i < parse_tree.numArgs(upvalues); i++){
        ParseNode up = parse_tree.arg(upvalues, i);
        switch (parse_tree.getEnum(up)) {
            case PN_IDENTIFIER:{ //Place on heap
                Value* v = new Value(readLocal(up));
                std::shared_ptr<void> ptr(v);
                list->push_back(ptr);
                assert(active_closure);
                active_closure->push_back(ptr);
                break;
            }
            case PN_READ_UPVALUE:{ //Get existing
                assert(active_closure);
                size_t index = parse_tree.upvalueOffset(up);
                list->push_back( active_closure->at(index) );
                break;
            }
            default:
                assert(false);
        }
    }
}

void Interpreter::returnStmt(ParseNode pn){
    Value v = interpretExpr(parse_tree.child(pn));
    exit_mode = static_cast<ExitMode>(exit_mode | RETURN);
    stack.push(v, "%RETURN");
}

Value Interpreter::implicitMult(ParseNode pn, size_t start){
    ParseNode lhs = parse_tree.arg(pn, start);
    Value vl = interpretExpr(lhs);
    size_t N = parse_tree.numArgs(pn);
    if(start == N-1) return vl;
    Value vr = implicitMult(pn, start+1);
    if(!isFunction(vl.index())) return binaryDispatch(PN_CALL, vl, vr, pn);

    size_t stack_size = stack.size();

    Value ans;
    switch (vl.index()) {
        case Lambda_index:{
            const Lambda& l = std::get<Lambda>(vl);
            ParseNode params = l.params;
            if(parse_tree.numArgs(params) != 1){
                return error(INVALID_ARGS, lhs);
            }else{
                stack.push(vr, parse_tree.str(parse_tree.child(params)));
            }
            ans = interpretExpr(l.expr);
            break;
        }
        case Algorithm_index:{
            const Algorithm& a = std::get<Algorithm>(vl);
            interpretStmt(a.body);
            ans = stack.back();
            break;
        }
        default:
            assert(false);
    }

    stack.trim(stack_size);

    return ans;
}

Value Interpreter::sum(ParseNode pn){
    return big(pn, PN_ADDITION);
}

Value Interpreter::prod(ParseNode pn){
    return big(pn, PN_MULTIPLICATION);
}

Value Interpreter::big(ParseNode pn, ParseNodeType type){
    ParseNode assign = parse_tree.arg(pn, 0);
    ParseNode stop = parse_tree.arg(pn, 1);
    ParseNode body = parse_tree.arg(pn, 2);

    interpretStmt(assign);
    Value val_start = stack.back();
    if(val_start.index() != double_index) return error(BIG_SYMBOL_ARG, assign);
    size_t start = std::get<double>(val_start);
    Value val_final = interpretExpr(stop);
    if(val_final.index() != double_index) return error(BIG_SYMBOL_ARG, stop);
    size_t final = std::get<double>(val_final);
    if(final <= start) return error(BIG_SYMBOL_RANGE, pn);

    Value v = interpretExpr(body);
    while(++start < final && exit_mode < ERROR){
        std::get<double>(stack.back()) += 1;
        v = binaryDispatch(type, v, interpretExpr(body), pn);
    }

    stack.trim(stack.size()-1);

    return v;
}

Value Interpreter::cases(ParseNode pn){
    for(size_t i = 0; i < parse_tree.numArgs(pn) && exit_mode < ERROR; i+=2){
        ParseNode condition = parse_tree.arg(pn, i+1);
        Value v = interpretExpr(condition);
        if(v.index() != bool_index) return error(TYPE_ERROR, condition);
        else if(std::get<bool>(v)) return interpretExpr(parse_tree.arg(pn, i));
    }

    return error(EMPTY_CASES, pn);
}

bool Interpreter::evaluateCondition(ParseNode pn){
    Value v = interpretExpr(pn);
    if(v.index() != bool_index){
        error(TYPE_ERROR, pn);
        return false;
    }else{
        return std::get<bool>(v);
    }
}

void Interpreter::reassign(ParseNode lhs, ParseNode rhs){
    switch (parse_tree.getEnum(lhs)) {
        case PN_IDENTIFIER:{
            Value v = interpretExpr(rhs);
            readLocal(lhs) = v;
            break;
        }

        case PN_READ_GLOBAL:{
            Value v = interpretExpr(rhs);
            readGlobal(lhs) = v;
            break;
        }

        case PN_READ_UPVALUE:{
            Value v = interpretExpr(rhs);
            readUpvalue(lhs) = v;
            break;
        }

        case PN_SUBSCRIPT_ACCESS:
            reassignSubscript(lhs, rhs);
            break;

        default: error(NON_LVALUE, lhs);
    }
}

void Interpreter::reassignSubscript(ParseNode lhs, ParseNode rhs){
    size_t num_indices = parse_tree.numArgs(lhs)-1;
    Value rvalue = interpretExpr(rhs);
    ParseNode lvalue_node = parse_tree.arg(lhs, 0);
    Value& lvalue = read(lvalue_node);
    //VERY IMPORTANT TO READ AFTER STACK MODIFYING OPS, NOT BEFORE!
    //Not a great way to check except making your own variant class

    assert(valid(lvalue));

    switch (lvalue.index()) {
        case double_index:
            if(rvalue.index() != double_index){
                error(TYPE_ERROR, rhs);
                return;
            }
            for(size_t i = 1; i <= num_indices && errors.empty(); i++)
                readSubscript(parse_tree.arg(lhs, i), 1);
            read(lvalue_node) = rvalue;
            return;

        case MatrixXd_index:{
            Eigen::MatrixXd& lmat = std::get<Eigen::MatrixXd>(lvalue);

            Eigen::MatrixXd rmat;
            if(rvalue.index() == double_index){
                rmat.resize(1,1);
                rmat(0,0) = std::get<double>(rvalue);
            }else if(rvalue.index() == MatrixXd_index){
                rmat = std::get<Eigen::MatrixXd>(rvalue);
            }else{
                error(TYPE_ERROR, rhs);
                return;
            }

            if(num_indices == 1){
                if(lmat.rows() > 1 && lmat.cols() > 1){
                    error(TYPE_ERROR, rhs);
                    return;
                }

                Eigen::Map<Eigen::VectorXd> vec(lmat.data(), lmat.size());
                ParseNode index_node = parse_tree.arg(lhs, 1);
                Slice s = readSubscript(index_node, lmat.size());
                if(!errors.empty()) return;
                auto v = vec(s);

                if(rmat.cols() == v.cols() && rmat.rows() == v.rows())
                    v = rmat;
                else
                    error(DIMENSION_MISMATCH, rhs);
            }else if(num_indices == 2){
                ParseNode row_node = parse_tree.arg(lhs, 1);
                Slice rows = readSubscript(row_node, lmat.rows());
                ParseNode col_node = parse_tree.arg(lhs, 2);
                Slice cols = readSubscript(col_node, lmat.cols());
                if(!errors.empty()) return;
                auto m = lmat(rows, cols);
                if(rmat.cols() == m.cols() && rmat.rows() == m.rows())
                    m = rmat;
                else
                    error(DIMENSION_MISMATCH, rhs);
            }else{
                error(INDEX_OUT_OF_RANGE, lhs);
            }
            return;
        }

        default:
            error(TYPE_ERROR, lvalue_node);
    }
}

void Interpreter::elementWiseAssignment(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    size_t num_subscripts = parse_tree.numArgs(lhs)-1;
    ParseNode lvalue_node = parse_tree.arg(lhs, 0);
    Value& lvalue = read(lvalue_node);

    if(lvalue.index() == double_index){
        bool use_first = parse_tree.getEnum( parse_tree.arg(lhs, 1) ) != PN_SLICE;
        bool use_second = num_subscripts>1 &&
                          parse_tree.getEnum( parse_tree.arg(lhs, 2) ) != PN_SLICE;

        if(use_first) stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 1)));
        if(use_second) stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 2)));
        Value rvalue = interpretExpr(rhs);
        stack.pop();
        if(use_first & use_second) stack.pop();
        read(lvalue_node) = rvalue;
        return;
    }else if(lvalue.index() != MatrixXd_index){
        error(TYPE_ERROR, lvalue_node);
        return;
    }

    Eigen::MatrixXd lmat = std::get<Eigen::MatrixXd>(lvalue);

    if(num_subscripts == 1){
        if((lmat.rows() > 1) & (lmat.cols() > 1)){
            error(DIMENSION_MISMATCH, lhs);
            return;
        }

        stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 1)));
        for(Eigen::Index i = 0; i < lmat.size(); i++){
            stack.back() = static_cast<double>(i);
            Value rvalue = interpretExpr(rhs);
            if(rvalue.index() != double_index){
                error(TYPE_ERROR, rhs);
                return;
            }
            lmat(i) = std::get<double>(rvalue);
        }
        stack.pop();

        read(lvalue_node) = lmat;
        return;
    }

    ParseNodeType type_row = parse_tree.getEnum( parse_tree.arg(lhs, 1) );
    ParseNodeType type_col = parse_tree.getEnum( parse_tree.arg(lhs, 2) );

    if(type_row == PN_SLICE){
        stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 2)));
        if(lmat.rows() > 1){
            for(Eigen::Index i = 0; i < lmat.cols(); i++){
                stack.back() = static_cast<double>(i);
                Value rvalue = interpretExpr(rhs);
                if(rvalue.index() != MatrixXd_index){
                    error(TYPE_ERROR, rhs);
                    return;
                }
                const Eigen::MatrixXd& rmat = std::get<Eigen::MatrixXd>(rvalue);
                if(rmat.cols() != 1 || rmat.rows() != lmat.rows()){
                    error(DIMENSION_MISMATCH, rhs);
                    return;
                }
                lmat.col(i) = rmat;
            }
        }else{
            for(Eigen::Index i = 0; i < lmat.cols(); i++){
                stack.back() = static_cast<double>(i);
                Value rvalue = interpretExpr(rhs);
                if(rvalue.index() != double_index){
                    error(TYPE_ERROR, rhs);
                    return;
                }
                lmat(i) = std::get<double>(rvalue);
            }
        }
        stack.pop();
        read(lvalue_node) = lmat;
    }else if(type_col == PN_SLICE){
        stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 1)));
        if(lmat.cols() > 1){
            for(Eigen::Index i = 0; i < lmat.rows(); i++){
                stack.back() = static_cast<double>(i);
                Value rvalue = interpretExpr(rhs);
                if(rvalue.index() != MatrixXd_index){
                    error(TYPE_ERROR, rhs);
                    return;
                }
                const Eigen::MatrixXd& rmat = std::get<Eigen::MatrixXd>(rvalue);
                if(rmat.rows() != 1 || rmat.cols() != lmat.cols()){
                    error(DIMENSION_MISMATCH, rhs);
                    return;
                }
                lmat.row(i) = rmat;
            }
        }else{
            for(Eigen::Index i = 0; i < lmat.rows(); i++){
                stack.back() = static_cast<double>(i);
                Value rvalue = interpretExpr(rhs);
                if(rvalue.index() != double_index){
                    error(TYPE_ERROR, rhs);
                    return;
                }
                lmat(i) = std::get<double>(rvalue);
            }
        }
        stack.pop();
        read(lvalue_node) = lmat;
    }else{
        stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 1)));
        stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 2)));
        for(Eigen::Index i = 0; i < lmat.rows(); i++){
            stack[stack.size()-2] = static_cast<double>(i);
            for(Eigen::Index j = 0; j < lmat.cols(); j++){
                stack.back() = static_cast<double>(j);
                Value rvalue = interpretExpr(rhs);
                if(rvalue.index() != double_index){
                    error(TYPE_ERROR, rhs);
                    return;
                }
                lmat(i,j) = std::get<double>(rvalue);
            }
        }
        stack.pop();
        stack.pop();
        read(lvalue_node) = lmat;
    }
}

Value& Interpreter::read(ParseNode pn){
    switch (parse_tree.getEnum(pn)) {
        case PN_IDENTIFIER: return readLocal(pn);
        case PN_READ_GLOBAL: return readGlobal(pn);
        case PN_READ_UPVALUE: return readUpvalue(pn);
        default:
            stack.push(error(NON_LVALUE, pn), "%ERROR");
            return stack.back();
    }
}

Value& Interpreter::readLocal(ParseNode pn) {
    assert(parse_tree.getEnum(pn) == PN_IDENTIFIER);
    size_t stack_offset = parse_tree.stackOffset(pn);

    return stack.read(stack.size()-1-stack_offset, parse_tree.str(pn));
}

Value& Interpreter::readGlobal(ParseNode pn){
    assert(parse_tree.getEnum(pn) == PN_READ_GLOBAL);
    size_t stack_offset = parse_tree.stackOffset(pn);

    return stack.read(stack_offset, parse_tree.str(pn));
}

Value& Interpreter::readUpvalue(ParseNode pn){
    size_t upvalue_offset = parse_tree.upvalueOffset(pn);
    return conv(active_closure->at(upvalue_offset));
}

Value Interpreter::error(ErrorCode code, ParseNode pn) {
    if(errors.empty()){
        exit_mode = ERROR;
        Typeset::Selection c = parse_tree.getSelection(pn);
        errors.push_back(Error(c, code));
    }

    return &errors.back();
}

Value Interpreter::matrix(ParseNode pn){
    if(parse_tree.numArgs(pn) == 1) return interpretExpr(parse_tree.arg(pn, 0));

    Typeset::Matrix* tm = static_cast<Typeset::Matrix*>(
                parse_tree.getLeft(pn).text->nextConstructAsserted()
                );

    Eigen::MatrixXd mat(tm->rows, tm->cols);

    size_t curr = 0;
    for(size_t i = 0; i < tm->rows; i++){
        for(size_t j = 0; j < tm->cols; j++){
            const ParseNode& arg = parse_tree.arg(pn, curr++);
            Value e = interpretExpr(arg);
            if(e.index() != double_index){
                return error(NON_NUMERIC_TYPE, arg);
            }
            mat(i,j) = std::get<double>(e);
        }
    }

    return mat;
}

Value Interpreter::str(ParseNode pn) const{
    std::string str = parse_tree.str(pn);
    return str.substr(1, str.size()-2);
}

Value Interpreter::anonFun(ParseNode pn){
    ParseNode upvalues = parse_tree.arg(pn, 0);
    ParseNode args = parse_tree.arg(pn, 1);
    ParseNode expr = parse_tree.arg(pn, 2);

    Lambda l(args, expr);

    Closure* list = &l.captured;
    for(size_t i = 0; i < parse_tree.numArgs(upvalues); i++){
        ParseNode up = parse_tree.arg(upvalues, i);
        switch (parse_tree.getEnum(up)) {
            case PN_IDENTIFIER:{ //Place on heap
                Value* v = new Value(readLocal(up));
                list->push_back(std::shared_ptr<void>(v));
                break;
            }
            case PN_READ_UPVALUE:{ //Get existing
                assert(active_closure);
                size_t index = parse_tree.upvalueOffset(up);
                list->push_back( active_closure->at(index) );
                break;
            }
            default:
                assert(false);
        }
    }

    return l;
}

Value Interpreter::call(ParseNode call) {
    Value v = interpretExpr( parse_tree.arg(call, 0) );
    size_t nargs = parse_tree.numArgs(call)-1;

    switch (v.index()) {
        case Lambda_index:{
            Lambda& f = std::get<Lambda>(v);
            size_t nparams = parse_tree.numArgs(f.params);
            if(nparams != nargs) return error(INVALID_ARGS, call);
            size_t stack_size = stack.size();
            for(size_t i = 0; i < nargs && exit_mode == KEEP_GOING; i++){
                ParseNode param = parse_tree.arg(f.params, i);
                if(parse_tree.getEnum(param) == PN_EQUAL) param = parse_tree.lhs(param);
                Value v = interpretExpr(parse_tree.arg(call, i+1));
                stack.push(v, parse_tree.str(param));
            }
            Closure* old = active_closure;
            active_closure = &f.captured;
            Value ans = interpretExpr(f.expr);
            stack.trim(stack_size);
            active_closure = old;
            return ans;
        }

        case Algorithm_index:{
            Algorithm& alg = std::get<Algorithm>(v);

            frames.push_back(stack.size());
            size_t nargs = parse_tree.numArgs(call)-1;
            size_t nparams = parse_tree.numArgs(alg.params);
            ParseNode params = alg.params;
            if(nargs > nparams){
                frames.pop_back();
                return error(INVALID_ARGS, call);
            }
            for(size_t i = 0; (i < nargs) & (exit_mode == KEEP_GOING); i++){
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getEnum(param) == PN_EQUAL) param = parse_tree.lhs(param);
                Value v = interpretExpr(parse_tree.arg(call, i+1));
                stack.push(v, parse_tree.str(param));
            }
            for(size_t i = nargs; (i < nparams)  & (exit_mode == KEEP_GOING); i++){
                ParseNode defnode = parse_tree.arg(params, i);
                if(parse_tree.getEnum(defnode) != PN_EQUAL){
                    stack.trim(frames.back());
                    frames.pop_back();

                    return error(INVALID_ARGS, call);
                }
                ParseNode param = parse_tree.arg(params, i);
                if(parse_tree.getEnum(param) == PN_EQUAL) param = parse_tree.lhs(param);
                Value v = interpretExpr(parse_tree.rhs(defnode));
                stack.push(v, parse_tree.str(param));
            }
            Closure* old = active_closure;
            active_closure = &alg.captured;
            interpretStmt(alg.body);
            active_closure = old;

            if(exit_mode != RETURN){
                stack.trim(frames.back());
                frames.pop_back();
                return error(NO_RETURN, call);
            }else{
                exit_mode = KEEP_GOING;
                Value ans = stack.readReturn();
                stack.trim(frames.back());
                frames.pop_back();
                return ans;
            }
        }

        case Unitialized_index:
            return error(USE_BEFORE_DEFINE, parse_tree.arg(call, 0));

        case double_index:
        case MatrixXd_index:
            if(nargs != 1) return error(NOT_CALLABLE, call);
            else return binaryDispatch(call);

        default: return error(NOT_CALLABLE, call);
    }
}

void Interpreter::callStmt(ParseNode pn){
    if(parse_tree.getEnum(pn) != PN_CALL){
        error(UNUSED_EXPRESSION, pn);
        return;
    }

    Value v = interpretExpr( parse_tree.arg(pn, 0) );
    size_t nargs = parse_tree.numArgs(pn)-1;

    switch (v.index()) {
        case Lambda_index:
            error(UNUSED_EXPRESSION, pn); break;

        case Unitialized_index:
            error(USE_BEFORE_DEFINE, parse_tree.arg(pn, 0)); break;

        case double_index:
        case MatrixXd_index:
            if(nargs != 1) error(NOT_CALLABLE, pn);
            else error(UNUSED_EXPRESSION, pn);
            break;

        case Algorithm_index:{
            Algorithm& alg = std::get<Algorithm>(v);
            size_t stack_size = stack.size();
            callAlg(alg, pn);
            if(exit_mode == RETURN) exit_mode = KEEP_GOING;
            stack.trim(stack_size);
            break;
        }

        default: error(NOT_CALLABLE, pn);
    }
}

void Interpreter::callAlg(Algorithm& alg, ParseNode call){
    frames.push_back(stack.size());
    size_t nargs = parse_tree.numArgs(call)-1;
    size_t nparams = parse_tree.numArgs(alg.params);
    ParseNode params = alg.params;
    if(nargs > nparams){
        error(INVALID_ARGS, call);
        return;
    }
    for(size_t i = 0; (i < nargs) & (exit_mode == KEEP_GOING); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getEnum(param) == PN_EQUAL) param = parse_tree.lhs(param);
        Value v = interpretExpr(parse_tree.arg(call, i+1));
        stack.push(v, parse_tree.str(param));
    }
    for(size_t i = nargs; (i < nparams)  & (exit_mode == KEEP_GOING); i++){
        ParseNode defnode = parse_tree.arg(params, i);
        if(parse_tree.getEnum(defnode) != PN_EQUAL){
            error(INVALID_ARGS, call);
            break;
        }
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getEnum(param) == PN_EQUAL) param = parse_tree.lhs(param);
        Value v = interpretExpr(parse_tree.rhs(defnode));
        stack.push(v, parse_tree.str(param));
    }

    Closure* old = active_closure;
    active_closure = &alg.captured;
    interpretStmt(alg.body);
    active_closure = old;
    stack.trim(frames.back());
    frames.pop_back();
}

Value Interpreter::elementAccess(ParseNode pn){
    Value lhs = interpretExpr( parse_tree.arg(pn, 0) );
    size_t num_indices = parse_tree.numArgs(pn)-1;

    switch (lhs.index()) {
        case double_index:
            for(size_t i = 1; i <= num_indices && errors.empty(); i++)
                readSubscript(parse_tree.arg(pn, i), 1);
            return lhs;

        case MatrixXd_index:{
            const Eigen::MatrixXd& mat = std::get<Eigen::MatrixXd>(lhs);
            if(num_indices == 1){
                if(mat.rows() > 1 && mat.cols() > 1) return error(INDEX_OUT_OF_RANGE, pn);
                Eigen::Map<const Eigen::VectorXd> vec(mat.data(), mat.size());
                ParseNode index_node = parse_tree.arg(pn, 1);
                Slice s = readSubscript(index_node, mat.size());
                if(!errors.empty()) return &errors[0];
                const auto& v = vec(s);
                if(v.size() == 1) return v(0);
                else if(mat.rows() == 1) return v.eval().transpose();
                else return v;
            }else if(num_indices == 2){
                ParseNode row_node = parse_tree.arg(pn, 1);
                Slice rows = readSubscript(row_node, mat.rows());
                ParseNode col_node = parse_tree.arg(pn, 2);
                Slice cols = readSubscript(col_node, mat.cols());
                if(!errors.empty()) return &errors[0];
                const auto& m = mat(rows, cols);
                return m.size() > 1 ? Value(m) : m(0,0);
            }else{
                return error(INDEX_OUT_OF_RANGE, pn);
            }
        }

        default: return error(TYPE_ERROR, pn);
    }
}

Interpreter::Slice Interpreter::readSubscript(ParseNode pn, Eigen::Index sze){
    size_t nargs = parse_tree.numArgs(pn);

    if(parse_tree.getEnum(pn) != PN_SLICE){
        Eigen::Index index = readIndex(pn, sze);
        return Slice(index, 1, Eigen::fix<1>);
    }else if(nargs == 1){
        return Slice(0, sze, Eigen::fix<1>);
    }

    ParseNode first = parse_tree.arg(pn, 0);
    ParseNode last = parse_tree.arg(pn, 1);

    Eigen::Index s = nargs == 3 ? readDouble(parse_tree.arg(pn, 2)) : 1;
    if(s == 0){
        error(INDEX_OUT_OF_RANGE, pn);
        return Slice(INVALID, INVALID, INVALID);
    }else if(s > 0){
        Eigen::Index f = parse_tree.getEnum(first) != PN_SLICE_ALL ? readIndex(first, sze) : 0;
        Eigen::Index l = parse_tree.getEnum(last) != PN_SLICE_ALL ? readIndex(last, sze) : sze-1;
        Eigen::Index diff = l-f;
        if(diff < 0){
            error(NONTERMINATING_SLICE, pn);
            return Slice(INVALID, INVALID, INVALID);
        }else{
            Eigen::Index slice_sze = 1+diff/s;
            return Slice(f, slice_sze, s);
        }
    }else{
        Eigen::Index f = parse_tree.getEnum(first) != PN_SLICE_ALL ? readIndex(first, sze) : sze-1;
        Eigen::Index l = parse_tree.getEnum(last) != PN_SLICE_ALL ? readIndex(last, sze) : 0;
        Eigen::Index diff = l-f;
        if(diff > 0){
            error(NONTERMINATING_SLICE, pn);
            return Slice(INVALID, INVALID, INVALID);
        }else{
            Eigen::Index slice_sze = 1+diff/s;
            return Slice(f, slice_sze, s);
        }
    }
}

Eigen::Index Interpreter::readIndex(ParseNode pn, Eigen::Index sze){
    Eigen::Index index = static_cast<Eigen::Index>(readDouble(pn));
    if((index >= sze) | (index < 0)){
        error(INDEX_OUT_OF_RANGE, pn);
        return INVALID;
    }

    return index;
}

double Interpreter::readDouble(ParseNode pn){
    Value v = interpretExpr(pn);
    if(v.index() != double_index){
        error(TYPE_ERROR, pn);
        return INVALID;
    }else{
        return std::get<double>(v);
    }
}

void Interpreter::printNode(const ParseNode& pn){
    Value v = interpretExpr(pn);
    if(exit_mode == KEEP_GOING) appendTo(output, v);
}

void Interpreter::appendTo(Typeset::Model* m, const Value& val){
    switch (val.index()) {
        case RuntimeError:
            break;
        case double_index:
            m->lastText()->str += formatted( std::get<double>(val) );
            break;
        case string_index:{

            std::string str = std::get<std::string>(val);
            removeEscapes(str);
            Typeset::Controller(m).insertSerial(str);

            break;
        }
        case MatrixXd_index:{
            const Eigen::MatrixXd& mat = std::get<Eigen::MatrixXd>(val);
            Typeset::Construct* c = new Typeset::Matrix(mat.rows(), mat.cols());

            for(Eigen::Index i = 0; i < mat.rows(); i++){
                for(Eigen::Index j = 0; j < mat.cols(); j++){
                    c->arg(i*mat.cols()+j)->front()->str = formatted(mat(i,j));
                }
            }
            m->lastLine()->appendConstruct(c);

            break;
        }

        case bool_index:
            m->lastText()->str += std::get<bool>(val) ? "true" : "false";
            break;

        case Lambda_index:{
            const Lambda& lambda = std::get<Lambda>(val);
            m->lastText()->str += parse_tree.str(lambda.params);
            m->lastText()->str += " ↦ ";
            m->lastText()->str += parse_tree.str(lambda.expr);
            break;
        }

        default:
            assert(false);
    }
}

double Interpreter::dot(const Eigen::MatrixXd& a, const Eigen::MatrixXd& b) noexcept{
    assert(a.size() == b.size());
    assert(a.cols() == 1 || a.rows()==1);

    double ans = 0;
    for(Eigen::Index i = a.size(); i-->0;)
        ans += a(i)*b(i);
    return ans;
}

Eigen::MatrixXd Interpreter::hat(const Eigen::MatrixXd& a){
    if(a.size() == 3){
        return Eigen::MatrixXd({{    0, -a(2),  a(1)},
                                { a(2),     0, -a(0)},
                                {-a(1),  a(0),     0}});
    }else{
        return Eigen::MatrixXd({{    0, -a(2),  a(1), a(3)},
                                { a(2),     0, -a(0), a(4)},
                                {-a(1),  a(0),     0, a(5)},
                                {    0,     0,     0,    1}});
    }
}

Value Interpreter::pow(const Eigen::MatrixXd& a, double b, ParseNode pn){
    if(b == -1) return a.inverse();

    #ifdef HOPE_EIGEN_UNSUPPORTED
    return a.pow(b);
    #else
    return error(EIGEN_UNSUPPORTED, pn);
    #endif
}

Value Interpreter::unitVector(ParseNode pn){
    Value elem = interpretExpr(parse_tree.arg(pn, 0));
    if(elem.index() != double_index) return error(TYPE_ERROR, parse_tree.arg(pn, 0));

    Value rows = interpretExpr(parse_tree.arg(pn, 1));
    if(rows.index() != double_index) return error(TYPE_ERROR, parse_tree.arg(pn, 1));

    Value cols = interpretExpr(parse_tree.arg(pn, 2));
    if(cols.index() != double_index) return error(TYPE_ERROR, parse_tree.arg(pn, 2));

    Eigen::Index r = std::get<double>(rows);
    Eigen::Index c = std::get<double>(cols);

    if(r < 0) return error(TYPE_ERROR, parse_tree.arg(pn, 1));
    if(c < 0) return error(TYPE_ERROR, parse_tree.arg(pn, 2));
    if(r > 1 && c > 1) return error(DIMENSION_MISMATCH, pn);

    Eigen::Index e = std::get<double>(elem);
    if(e < 0) return error(TYPE_ERROR, parse_tree.arg(pn, 0));
    if(e >= r*c) return error(DIMENSION_MISMATCH, pn);

    if(r > 1) return Eigen::VectorXd::Unit(r, e);
    else if(c > 1) return Eigen::RowVectorXd::Unit(c, e);
    else return 1.0f;
}

double Interpreter::pNorm(const Eigen::MatrixXd& a, double b) noexcept{
    double sum = 0;
    for(Eigen::Index i = 0; i < a.size(); i++)
        sum += std::pow(std::abs(a(i)), b);

    return std::pow(sum, 1/b);
}

void Interpreter::removeEscapes(std::string& str) noexcept{
    size_t offset = 0;
    size_t i = 0;
    while(i < str.size()-1){
        str[i-offset] = str[i];

        if(str[i] == '\\'){
            switch (str[i+1]) {
                case 'n': str[i-offset] = '\n'; i++; offset++; break;
                case '"': str[i-offset] = '"'; i++; offset++; break;
            }
        }

        i++;
    }
    if(i < str.size()) str[i-offset] = str[i];
    str.resize(str.size()-offset);
}

}

}
