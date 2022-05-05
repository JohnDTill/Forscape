#include "hope_interpreter.h"

#include <code_parsenode_ops.h>
#include "construct_codes.h"
#include "hope_symbol_link_pass.h"

#include <Eigen/Eigen>
#include <thread>

#ifdef HOPE_EIGEN_UNSUPPORTED
#include <unsupported/Eigen/MatrixFunctions>
#endif

#ifndef NDEBUG
#include <iostream>
#endif

namespace Hope {

namespace Code {

void Interpreter::run(const ParseTree& parse_tree, SymbolTable symbol_table, const InstantiationLookup& inst_lookup){
    assert(parse_tree.getOp(parse_tree.root) == OP_BLOCK);
    reset();

    this->parse_tree = parse_tree;
    this->inst_lookup = inst_lookup;
    SymbolTableLinker linker(symbol_table, this->parse_tree);
    linker.link();
    this->parse_tree.patchClones();

    blockStmt(this->parse_tree.root);
    status = FINISHED;
}

void Interpreter::runThread(const ParseTree& parse_tree, SymbolTable symbol_table, const InstantiationLookup& inst_lookup){
    status = NORMAL;
    std::thread(&Interpreter::run, this, parse_tree, symbol_table, inst_lookup).detach();
}

void Interpreter::stop(){
    error(USER_STOP, parse_tree.root);
}

void Interpreter::reset() noexcept {
    error_code = NO_ERROR_FOUND;
    error_node = ParseTree::EMPTY;
    directive = RUN;
    status = NORMAL;
    stack.clear();
    active_closure = nullptr;
}

Value Interpreter::error(ErrorCode code, ParseNode pn) noexcept {
    if(status < RUNTIME_ERROR){
        directive = STOP;
        error_code = code;
        error_node = pn;
    }
    status = RUNTIME_ERROR;

    return &error_code;
}

void Interpreter::interpretStmt(ParseNode pn){
    if(directive == STOP) status = RUNTIME_ERROR;

    switch (parse_tree.getOp(pn)) {
        case OP_EQUAL: assignStmt(pn); break;
        case OP_ASSIGN: assignStmt(pn); break;
        case OP_REASSIGN: reassign(parse_tree.lhs(pn), parse_tree.rhs(pn)); break;
        case OP_ELEMENTWISE_ASSIGNMENT: elementWiseAssignment(pn); break;
        case OP_PRINT: printStmt(pn); break;
        case OP_ASSERT: assertStmt(pn); break;
        case OP_IF: ifStmt(pn); break;
        case OP_IF_ELSE: ifElseStmt(pn); break;
        case OP_WHILE: whileStmt(pn); break;
        case OP_FOR: forStmt(pn); break;
        case OP_BLOCK: blockStmt(pn); break;
        case OP_ALGORITHM: algorithmStmt(pn); break;
        case OP_PROTOTYPE_ALG: stack.push(static_cast<void*>(nullptr), parse_tree.str(parse_tree.child(pn))); break;
        case OP_EXPR_STMT: callStmt(parse_tree.child(pn)); break;
        case OP_RETURN: returnStmt(pn); break;
        case OP_BREAK: status = static_cast<Status>(status | BREAK); break;
        case OP_CONTINUE: status = static_cast<Status>(status | CONTINUE); break;
        case OP_DO_NOTHING: break;
        default: error(UNRECOGNIZED_STMT, pn);
    }
}

void Interpreter::printStmt(ParseNode pn){
    for(size_t i = 0; i < parse_tree.getNumArgs(pn) && status == NORMAL; i++)
        printNode(parse_tree.arg(pn, i));
}

void Interpreter::assertStmt(ParseNode pn){
    ParseNode child = parse_tree.child(pn);
    if(!evaluateCondition(child)) error(ASSERT_FAIL, child);
}

void Interpreter::assignStmt(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    Value v = interpretExpr(rhs);

    if(parse_tree.getOp(lhs) == OP_READ_UPVALUE) readClosedVar(lhs) = v;
    else stack.push(v, parse_tree.str(lhs));
}

void Interpreter::whileStmt(ParseNode pn){
    while(status <= CONTINUE && evaluateCondition( parse_tree.arg(pn, 0) )){
        status = NORMAL;
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(status < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::forStmt(ParseNode pn){
    size_t stack_size = stack.size();
    interpretStmt(parse_tree.arg(pn, 0));

    while(status <= CONTINUE && evaluateCondition( parse_tree.arg(pn, 1) )){
        status = NORMAL;
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 3) );
        if(status < RETURN){
            stack.trim(stack_size);
            interpretStmt(parse_tree.arg(pn, 2));
        }
    }

    if(status < RETURN) stack.trim(stack_size);
}

void Interpreter::ifStmt(ParseNode pn){
    if(status == NORMAL && evaluateCondition( parse_tree.arg(pn, 0) )){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(status < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::ifElseStmt(ParseNode pn){
    if(status == NORMAL && evaluateCondition( parse_tree.arg(pn, 0) )){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 1) );
        if(status < RETURN) stack.trim(stack_size);
    }else if(status == NORMAL){
        size_t stack_size = stack.size();
        interpretStmt( parse_tree.arg(pn, 2) );
        if(status < RETURN) stack.trim(stack_size);
    }
}

void Interpreter::blockStmt(ParseNode pn){
    for(size_t i = 0; i < parse_tree.getNumArgs(pn) && status == NORMAL; i++)
        interpretStmt(parse_tree.arg(pn, i));
}

void Interpreter::algorithmStmt(ParseNode pn){
    Algorithm alg(pn);
    Closure* list;

    ParseNode name = alg.name(parse_tree);
    ParseNode captured = alg.valCap(parse_tree);
    ParseNode upvalues = alg.refCap(parse_tree);

    if(parse_tree.getFlag(pn) == NONE){
        stack.push(alg, parse_tree.str(name));
        list = &std::get<Algorithm>(stack.back()).closure;
    }else{
        Value& place = read(name);
        place = alg;
        list = &std::get<Algorithm>(place).closure;
    }

    initClosure(*list, captured, upvalues);
}

void Interpreter::initClosure(Closure& closure, ParseNode val_cap, ParseNode ref_cap){
    closure.clear();

    for(size_t i = 0; i < parse_tree.valListSize(val_cap); i++){
        ParseNode capture = parse_tree.arg(val_cap, i);
        Value* v = new Value(read(capture));
        std::shared_ptr<void> ptr(v);
        closure.push_back(ptr);
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(ref_cap); i++){
        ParseNode up = parse_tree.arg(ref_cap, i);

        switch (parse_tree.getOp(up)) {
            case OP_IDENTIFIER:{ //Place on heap
                Value* v = new Value();
                std::shared_ptr<void> ptr(v);
                closure.push_back(ptr);
                break;
            }
            case OP_READ_UPVALUE:{ //Get existing
                assert(active_closure);
                size_t index = parse_tree.getClosureIndex(up);
                assert(index < active_closure->size());
                closure.push_back( active_closure->at(index) );
                break;
            }
            default:
                assert(false);
        }
    }
}

void Interpreter::breakLocalClosureLinks(Closure& closure, ParseNode val_cap, ParseNode ref_cap){
    size_t val_cap_size = parse_tree.valListSize(val_cap);

    for(size_t i = 0; i < val_cap_size; i++){
        Value& val = conv(closure[i]);
        std::shared_ptr<void> ptr(new Value(val));
        closure[i] = ptr;
    }

    for(size_t i = 0; i < parse_tree.getNumArgs(ref_cap); i++){
        if(parse_tree.getOp(parse_tree.arg(ref_cap, i)) == OP_IDENTIFIER){
            size_t j = val_cap_size + i;
            Value& val = conv(closure[j]);
            std::shared_ptr<void> ptr(new Value(val));
            closure[j] = ptr;
        }
    }
}

void Interpreter::returnStmt(ParseNode pn){
    Value v = interpretExpr(parse_tree.child(pn));
    status = static_cast<Status>(status | RETURN);
    stack.push(v, "%RETURN");
}

Value Interpreter::implicitMult(ParseNode pn, size_t start){
    ParseNode lhs = parse_tree.arg(pn, start);
    Value vl = interpretExpr(lhs);
    size_t N = parse_tree.getNumArgs(pn);
    if(start == N-1) return vl;
    Value vr = implicitMult(pn, start+1);
    if(!isFunction(vl.index())) return binaryDispatch(OP_CALL, vl, vr, pn);

    size_t stack_size = stack.size();

    Value ans;
    switch (vl.index()) {
        case Lambda_index:{
            const Lambda& l = std::get<Lambda>(vl);
            ParseNode params = l.params(parse_tree);
            if(parse_tree.getNumArgs(params) != 1){
                error(INVALID_ARGS, lhs);
                return NIL;
            }else{
                stack.push(vr, parse_tree.str(parse_tree.child(params)));
            }
            ans = interpretExpr(l.expr(parse_tree));
            break;
        }
        case Algorithm_index:{
            const Algorithm& a = std::get<Algorithm>(vl);
            interpretStmt(a.body(parse_tree));
            ans = stack.back();
            break;
        }
        case RuntimeError: return &error_code;
        default:
            if(error_code != NO_ERROR_FOUND) return &error_code; //EVENTUALLY: delete this
            assert(false);
    }

    stack.trim(stack_size);

    return ans;
}

Value Interpreter::sum(ParseNode pn){
    return big(pn, OP_ADDITION);
}

Value Interpreter::prod(ParseNode pn){
    return big(pn, OP_MULTIPLICATION);
}

Value Interpreter::big(ParseNode pn, Op type){
    ParseNode assign = parse_tree.arg(pn, 0);
    ParseNode stop = parse_tree.arg(pn, 1);
    ParseNode body = parse_tree.arg(pn, 2);

    interpretStmt(assign);
    Value val_start = stack.back();
    if(val_start.index() != double_index){
        error(BIG_SYMBOL_ARG, assign);
        return NIL;
    }
    size_t start = std::get<double>(val_start);
    Value val_final = interpretExpr(stop);
    if(val_final.index() != double_index){
        error(BIG_SYMBOL_ARG, stop);
        return NIL;
    }
    size_t final = std::get<double>(val_final);
    if(final <= start){
        error(BIG_SYMBOL_RANGE, pn);
        return NIL;
    }

    Value v = interpretExpr(body);
    while(++start < final && status < RUNTIME_ERROR){
        std::get<double>(stack.back()) += 1;
        v = binaryDispatch(type, v, interpretExpr(body), pn);
    }

    stack.trim(stack.size()-1);

    return v;
}

Value Interpreter::cases(ParseNode pn){
    for(size_t i = 0; i < parse_tree.getNumArgs(pn) && status < RUNTIME_ERROR; i+=2)
        if(evaluateCondition(parse_tree.arg(pn, i+1)))
            return interpretExpr(parse_tree.arg(pn, i));

    error(EMPTY_CASES, pn);
    return NIL;
}

bool Interpreter::evaluateCondition(ParseNode pn){
    Value v = interpretExpr(pn);
    if(error_code != NO_ERROR_FOUND) return false;
    assert(v.index() == bool_index);

    return std::get<bool>(v);
}

void Interpreter::reassign(ParseNode lhs, ParseNode rhs){
    switch (parse_tree.getOp(lhs)) {
        case OP_IDENTIFIER:{
            Value v_rhs = interpretExpr(rhs);
            Value& v_lhs = readLocal(lhs);
            if(v_rhs.index() != v_lhs.index()) error(DIMENSION_MISMATCH, rhs);
            else if(v_rhs.index() == MatrixXd_index){
                const Eigen::MatrixXd& m_lhs = std::get<Eigen::MatrixXd>(v_lhs);
                const Eigen::MatrixXd& m_rhs = std::get<Eigen::MatrixXd>(v_rhs);
                if(m_lhs.rows() != m_rhs.rows() || m_lhs.cols() != m_rhs.cols()){
                    error(DIMENSION_MISMATCH, rhs);
                    break;
                }
            }

            v_lhs = v_rhs;
            break;
        }

        case OP_READ_GLOBAL:{
            Value v_rhs = interpretExpr(rhs);
            Value& v_lhs = readGlobal(lhs);
            if(v_rhs.index() != v_lhs.index()) error(DIMENSION_MISMATCH, rhs);
            else if(v_rhs.index() == MatrixXd_index){
                const Eigen::MatrixXd& m_lhs = std::get<Eigen::MatrixXd>(v_lhs);
                const Eigen::MatrixXd& m_rhs = std::get<Eigen::MatrixXd>(v_rhs);
                if(m_lhs.rows() != m_rhs.rows() || m_lhs.cols() != m_rhs.cols()){
                    error(DIMENSION_MISMATCH, rhs);
                    break;
                }
            }

            v_lhs = v_rhs;
            break;
        }

        case OP_READ_UPVALUE:{
            Value v_rhs = interpretExpr(rhs);
            Value& v_lhs = readClosedVar(lhs);
            if(v_rhs.index() != v_lhs.index()) error(DIMENSION_MISMATCH, rhs);
            else if(v_rhs.index() == MatrixXd_index){
                const Eigen::MatrixXd& m_lhs = std::get<Eigen::MatrixXd>(v_lhs);
                const Eigen::MatrixXd& m_rhs = std::get<Eigen::MatrixXd>(v_rhs);
                if(m_lhs.rows() != m_rhs.rows() || m_lhs.cols() != m_rhs.cols()){
                    error(DIMENSION_MISMATCH, rhs);
                    break;
                }
            }

            v_lhs = v_rhs;
            break;
        }

        case OP_SUBSCRIPT_ACCESS:
            reassignSubscript(lhs, rhs);
            break;

        default:
            assert(false);
    }
}

void Interpreter::reassignSubscript(ParseNode lhs, ParseNode rhs){
    size_t num_indices = parse_tree.getNumArgs(lhs)-1;
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
            for(size_t i = 1; i <= num_indices && status == NORMAL; i++)
                readSubscript(parse_tree.arg(lhs, i), 1);
            read(lvalue_node) = rvalue;
            return;

        case MatrixXd_index:{
            Eigen::MatrixXd& lmat = std::get<Eigen::MatrixXd>(lvalue);

            Eigen::MatrixXd rmat;
            if(rvalue.index() == double_index){
                rmat.resize(1,1);
                rmat(0,0) = std::get<double>(rvalue);
            }else{
                assert(rvalue.index() == MatrixXd_index);
                rmat = std::get<Eigen::MatrixXd>(rvalue);
            }

            if(num_indices == 1){
                if(lmat.rows() > 1 && lmat.cols() > 1){
                    error(TYPE_ERROR, rhs);
                    return;
                }

                Eigen::Map<Eigen::VectorXd> vec(lmat.data(), lmat.size());
                ParseNode index_node = parse_tree.arg(lhs, 1);
                Slice s = readSubscript(index_node, lmat.size());
                if(status != NORMAL) return;
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
                if(status != NORMAL) return;
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
            assert(false);
    }
}

void Interpreter::elementWiseAssignment(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    ParseNode rhs = parse_tree.rhs(pn);

    size_t num_subscripts = parse_tree.getNumArgs(lhs)-1;
    ParseNode lvalue_node = parse_tree.arg(lhs, 0);
    Value& lvalue = read(lvalue_node);

    if(lvalue.index() == double_index){
        bool use_first = parse_tree.getOp( parse_tree.arg(lhs, 1) ) != OP_SLICE;
        bool use_second = num_subscripts>1 &&
                          parse_tree.getOp( parse_tree.arg(lhs, 2) ) != OP_SLICE;

        if(use_first) stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 1)));
        if(use_second) stack.push(0.0, parse_tree.str(parse_tree.arg(lhs, 2)));
        Value rvalue = interpretExpr(rhs);
        stack.pop();
        if(use_first & use_second) stack.pop();
        read(lvalue_node) = rvalue;
        return;
    }

    assert(lvalue.index() == MatrixXd_index);

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

    Op type_row = parse_tree.getOp( parse_tree.arg(lhs, 1) );
    Op type_col = parse_tree.getOp( parse_tree.arg(lhs, 2) );

    if(type_row == OP_SLICE){
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
    }else if(type_col == OP_SLICE){
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
    switch (parse_tree.getOp(pn)) {
        case OP_IDENTIFIER: return readLocal(pn);
        case OP_READ_GLOBAL: return readGlobal(pn);
        case OP_READ_UPVALUE: return readClosedVar(pn);
        default:
            error(NON_LVALUE, pn);
            stack.push(NIL, "%ERROR");
            return stack.back();
    }
}

Value& Interpreter::readLocal(ParseNode pn) {
    assert(parse_tree.getOp(pn) == OP_IDENTIFIER);
    size_t stack_offset = parse_tree.getStackOffset(pn);

    return stack.read(stack.size()-1-stack_offset, parse_tree.str(pn));
}

Value& Interpreter::readGlobal(ParseNode pn){
    assert(parse_tree.getOp(pn) == OP_READ_GLOBAL);
    size_t stack_offset = parse_tree.getGlobalIndex(pn);

    return stack.read(stack_offset, parse_tree.str(pn));
}

Value& Interpreter::readClosedVar(ParseNode pn){
    size_t upvalue_offset = parse_tree.getClosureIndex(pn);
    assert(upvalue_offset < active_closure->size());
    return conv(active_closure->at(upvalue_offset));
}

Value Interpreter::matrix(ParseNode pn){
    size_t nargs = parse_tree.getNumArgs(pn);
    if(nargs == 1) return interpretExpr(parse_tree.arg(pn, 0));
    size_t typeset_rows = parse_tree.getFlag(pn);
    size_t typeset_cols = nargs/typeset_rows;
    assert(typeset_cols*typeset_rows == nargs);

    //EVENTUALLY: break nested allocation
    std::vector<size_t> elem_cols; elem_cols.resize(typeset_cols);
    std::vector<size_t> elem_rows; elem_rows.resize(typeset_rows);
    std::vector<Value> elements; elements.resize(nargs);

    size_t curr = 0;
    for(size_t i = 0; i < typeset_rows; i++){
        for(size_t j = 0; j < typeset_cols; j++){
            const ParseNode& arg = parse_tree.arg(pn, curr);
            elements[curr] = interpretExpr(arg);
            const Value& e = elements[curr++];

            if(e.index() == double_index){
                if(i==0) elem_cols[j] = 1;
                else if(elem_cols[j] != 1) return error(ErrorCode::DIMENSION_MISMATCH, pn);
                if(j==0) elem_rows[i] = 1;
                else if(elem_rows[i] != 1) return error(ErrorCode::DIMENSION_MISMATCH, pn);
            }else{
                if(error_code != NO_ERROR_FOUND) return &error_code;
                if(error_code != NO_ERROR_FOUND) return &error_code;
                assert(e.index() == MatrixXd_index);
                const Eigen::MatrixXd& e_mat = std::get<Eigen::MatrixXd>(e);
                if(i==0) elem_cols[j] = e_mat.cols();
                else if(elem_cols[j] != e_mat.cols()) return error(ErrorCode::DIMENSION_MISMATCH, pn);
                if(j==0) elem_rows[i] = e_mat.rows();
                else if(elem_rows[i] != e_mat.rows()) return error(ErrorCode::DIMENSION_MISMATCH, pn);
            }
        }
    }

    size_t rows = 0;
    for(auto count : elem_rows) rows += count;
    size_t cols = 0;
    for(auto count : elem_cols) cols += count;

    Eigen::MatrixXd mat(rows, cols);

    size_t row = 0;
    for(size_t i = 0; i < typeset_rows; i++){
        size_t col = 0;
        for(size_t j = 0; j < typeset_cols; j++){
            const Value& e = elements[j + i*typeset_cols];
            if(e.index() == double_index){
                mat(row, col) = std::get<double>(e);
            }else{
                if(error_code != NO_ERROR_FOUND) return &error_code;
                assert(e.index() == MatrixXd_index);
                const Eigen::MatrixXd& e_mat = std::get<Eigen::MatrixXd>(e);
                mat.block(row, col, e_mat.rows(), e_mat.cols()) = e_mat;
            }

            col += elem_cols[j];
        }

        row += elem_rows[i];
    }

    return mat;
}

Value Interpreter::str(ParseNode pn) const{
    std::string str = parse_tree.str(pn);
    return str.substr(1, str.size()-2);
}

Value Interpreter::anonFun(ParseNode pn){
    Lambda l(pn);

    ParseNode ref_list = l.refCap(parse_tree);

    Closure* list = &l.closure;

    initClosure(*list, ParseTree::EMPTY, ref_list);

    return l;
}

Value Interpreter::call(ParseNode call) {
    Value v = interpretExpr( parse_tree.arg(call, 0) );
    size_t nargs = parse_tree.getNumArgs(call)-1;

    switch (v.index()) {
        case Lambda_index:{
            Lambda& f = std::get<Lambda>(v);
            return innerCall(call, f.closure, f.def, true, true);
        }

        case Algorithm_index:{
            Algorithm& alg = std::get<Algorithm>(v);
            return innerCall(call, alg.closure, alg.def, true, false);
        }

        case Unitialized_index:
            error(USE_BEFORE_DEFINE, parse_tree.arg(call, 0));
            return NIL;

        case double_index:
        case MatrixXd_index:
            assert(nargs == 1);
            return binaryDispatch(call);

        default:
            assert(false);
            return NIL;
    }
}

void Interpreter::callStmt(ParseNode pn){
    Value v = interpretExpr( parse_tree.arg(pn, 0) );

    switch (v.index()) {
        case Lambda_index:
            break; //No side effects, optimise away EVENTUALLY

        case Algorithm_index:{
            Algorithm& alg = std::get<Algorithm>(v);
            innerCall(pn, alg.closure, alg.def, false, false);
            break;
        }

        default:
            assert(false);
    }
}

Value Interpreter::innerCall(ParseNode call, Closure& closure, ParseNode fn, bool expect, bool is_lambda){
    assert(parse_tree.getOp(call) == OP_CALL);

    auto inst_result = inst_lookup.find(std::make_pair(fn, call));
    assert(inst_result != inst_lookup.end());
    ParseNode inst_fn = inst_result->second;

    ParseNode val_cap = parse_tree.valCapList(inst_fn);
    ParseNode ref_cap = parse_tree.refCapList(inst_fn);
    ParseNode params = parse_tree.paramList(inst_fn);
    ParseNode body = parse_tree.body(inst_fn);

    size_t nargs = parse_tree.getNumArgs(call)-1;
    size_t nparams = parse_tree.getNumArgs(params);
    if(nargs > nparams) return error(INVALID_ARGS, call);
    std::vector<std::pair<Value, std::string>> stack_vals; //EVENTUALLY: break nested allocation
    std::vector<std::pair<ParseNode, Value>> closure_vals;
    for(size_t i = 0; (i < nargs) & (status == NORMAL); i++){
        ParseNode param = parse_tree.arg(params, i);
        if(parse_tree.getOp(param) == OP_EQUAL) param = parse_tree.lhs(param);
        Value v = interpretExpr(parse_tree.arg(call, i+1));
        if(parse_tree.getOp(param) == OP_READ_UPVALUE){
            closure_vals.push_back({param, v});
        }else{
            stack_vals.push_back({v, parse_tree.str(param)});
        }
    }

    breakLocalClosureLinks(closure, val_cap, ref_cap);
    frames.push_back(stack.size());
    Closure* old = active_closure;
    active_closure = &closure;

    for(size_t i = nargs; (i < nparams)  & (status == NORMAL); i++){
        ParseNode defnode = parse_tree.arg(params, i);
        assert(parse_tree.getOp(defnode) == OP_EQUAL);
        ParseNode param = parse_tree.lhs(defnode);
        Value v = interpretExpr(parse_tree.rhs(defnode));
        if(parse_tree.getOp(param) == OP_READ_UPVALUE){
            closure_vals.push_back({param, v});
        }else{
            stack_vals.push_back({v, parse_tree.str(param)});
        }
    }

    for(const auto& entry : stack_vals)
        stack.push(entry.first, entry.second);
    for(const auto& entry : closure_vals)
        readClosedVar(entry.first) = entry.second;

    Value ans;

    if(is_lambda){
        ans = interpretExpr(body);
    }else{
        interpretStmt(body);

        if(status != RETURN){
            ans = expect ? error(NO_RETURN, call) : NIL;
        }else{
            ans = stack.readReturn();
            status = NORMAL;
        }
    }

    if(error_code != NO_ERROR_FOUND) return &error_code;

    stack.trim(frames.back());
    frames.pop_back();
    active_closure = old;
    return ans;
}

Value Interpreter::elementAccess(ParseNode pn){
    Value lhs = interpretExpr( parse_tree.arg(pn, 0) );
    size_t num_indices = parse_tree.getNumArgs(pn)-1;

    if(lhs.index() == double_index){
        for(size_t i = 1; i <= num_indices && status == NORMAL; i++)
            readSubscript(parse_tree.arg(pn, i), 1);
        return lhs;
    }else{
        assert(lhs.index() == MatrixXd_index);

        const Eigen::MatrixXd& mat = std::get<Eigen::MatrixXd>(lhs);
        if(num_indices == 1){
            if(mat.rows() > 1 && mat.cols() > 1){
                error(INDEX_OUT_OF_RANGE, pn);
                return NIL;
            }
            Eigen::Map<const Eigen::VectorXd> vec(mat.data(), mat.size());
            ParseNode index_node = parse_tree.arg(pn, 1);
            Slice s = readSubscript(index_node, mat.size());
            if(status != NORMAL) return NIL;
            const auto& v = vec(s);
            if(v.size() == 1) return v(0);
            else if(mat.rows() == 1) return v.eval().transpose();
            else return v;
        }else if(num_indices == 2){
            ParseNode row_node = parse_tree.arg(pn, 1);
            Slice rows = readSubscript(row_node, mat.rows());
            ParseNode col_node = parse_tree.arg(pn, 2);
            Slice cols = readSubscript(col_node, mat.cols());
            if(status != NORMAL) return NIL;
            const auto& m = mat(rows, cols);
            return m.size() > 1 ? Value(m) : m(0,0);
        }else{
            error(INDEX_OUT_OF_RANGE, pn);
            return NIL;
        }
    }
}

Interpreter::Slice Interpreter::readSubscript(ParseNode pn, Eigen::Index sze){
    size_t nargs = parse_tree.getNumArgs(pn);

    if(parse_tree.getOp(pn) != OP_SLICE){
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
        Eigen::Index f = parse_tree.getOp(first) != OP_SLICE_ALL ? readIndex(first, sze) : 0;
        Eigen::Index l = parse_tree.getOp(last) != OP_SLICE_ALL ? readIndex(last, sze) : sze-1;
        Eigen::Index diff = l-f;
        if(diff < 0){
            error(NONTERMINATING_SLICE, pn);
            return Slice(INVALID, INVALID, INVALID);
        }else{
            Eigen::Index slice_sze = 1+diff/s;
            return Slice(f, slice_sze, s);
        }
    }else{
        Eigen::Index f = parse_tree.getOp(first) != OP_SLICE_ALL ? readIndex(first, sze) : sze-1;
        Eigen::Index l = parse_tree.getOp(last) != OP_SLICE_ALL ? readIndex(last, sze) : 0;
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
        error(DIMENSION_MISMATCH, pn);
        return INVALID;
    }else{
        return std::get<double>(v);
    }
}

void Interpreter::printNode(const ParseNode& pn){
    Value val = interpretExpr(pn);
    if(status != NORMAL) return;

    std::string str;

    switch (val.index()) {
        case double_index: str = formatted( std::get<double>(val) ); break;
        case bool_index: str = std::get<bool>(val) ? "true" : "false"; break;

        case string_index:
            str = std::get<std::string>(val);
            removeEscapes(str);
            break;

        case MatrixXd_index:{
            auto mat = std::get<Eigen::MatrixXd>(val);
            str += OPEN;
            str += MATRIX;
            str += mat.rows();
            str += mat.cols();
            for(Eigen::Index i = 0; i < mat.rows(); i++)
                for(Eigen::Index j = 0; j < mat.cols(); j++)
                    str += formatted(mat(i,j)) + CLOSE;
            break;
        }

        case Lambda_index:{
            const Lambda& lambda = std::get<Lambda>(val);
            str = parse_tree.str(lambda.params(parse_tree)) + " ↦ " + parse_tree.str(lambda.expr(parse_tree));
            break;
        }

        default: assert(false);
    }

    for(const char& ch : str) message_queue.enqueue(ch);
    message_queue.enqueue('\0');
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
        assert(a.size() == 6);
        return Eigen::MatrixXd({{    0, -a(2),  a(1), a(3)},
                                { a(2),     0, -a(0), a(4)},
                                {-a(1),  a(0),     0, a(5)},
                                {    0,     0,     0,    1}});
    }
}

Eigen::MatrixXd Interpreter::invHat(const Eigen::MatrixXd& a){
    if(a.size() == 9){
        assert(a.rows() == 3);
        return Eigen::Vector3d(a(2,1), a(0,2), a(1,0));
    }else{
        assert(a.rows() == 4 && a.cols() == 4);
        return Eigen::Matrix<double, 6, 1>(a(2,1), a(0,2), a(1,0), a(0,3), a(1,3), a(2,3));
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
    Value elem = interpretExpr(parse_tree.unitVectorElem(pn));
    assert(elem.index() == double_index);

    Value rows = interpretExpr(parse_tree.unitVectorRows(pn));
    assert(rows.index() == double_index);

    Value cols = interpretExpr(parse_tree.unitVectorCols(pn));
    assert(cols.index() == double_index);

    Eigen::Index r = std::get<double>(rows);
    Eigen::Index c = std::get<double>(cols);

    assert(r >= 1);
    assert(c >= 1);

    if(r > 1 && c > 1){
        error(DIMENSION_MISMATCH, pn);
        return NIL;
    }

    Eigen::Index e = std::get<double>(elem);
    assert(e >= 0);
    if(e >= r*c){
        error(DIMENSION_MISMATCH, pn);
        return NIL;
    }

    if(r > 1) return Eigen::VectorXd::Unit(r, e);
    else if(c > 1) return Eigen::RowVectorXd::Unit(c, e);
    else return 1.0f;
}

Value Interpreter::finiteDiff(ParseNode pn){
    ParseNode val_pn = parse_tree.arg<2>(pn);
    Value val = interpretExpr(val_pn);
    stack.push(val, parse_tree.str(val_pn));
    ParseNode expr = parse_tree.arg<0>(pn);
    Value f = interpretExpr(expr);

    static constexpr double INCR = 1e-9;

    Value ans;

    if(val.index() == double_index){
        std::get<double>(stack.back()) += INCR;
        Value f_incr = interpretExpr(expr);
        if(f.index() == double_index){
            ans = (std::get<double>(f_incr) - std::get<double>(f)) / INCR;
        }else{
            assert(f.index() == MatrixXd_index);
            ans = (std::get<Eigen::MatrixXd>(f_incr) - std::get<Eigen::MatrixXd>(f)) / INCR;
        }
    }else{
        assert(val.index() == MatrixXd_index);
        const Eigen::MatrixXd& v = std::get<Eigen::MatrixXd>(val);
        if(v.cols() != 1) return error(DIMENSION_MISMATCH, val_pn);

        if(f.index() == double_index){
            Eigen::MatrixXd a(1, v.rows());
            for(Eigen::Index i = 0; i < v.rows(); i++){
                double orig = std::get<Eigen::MatrixXd>(stack.back())(i);
                std::get<Eigen::MatrixXd>(stack.back())(i) += INCR;
                Value f_incr = interpretExpr(expr);
                a(i) = (std::get<double>(f_incr) - std::get<double>(f)) / INCR;
                std::get<Eigen::MatrixXd>(stack.back())(i) = orig;
            }
            ans = a;
        }else{
            assert(f.index() == MatrixXd_index);
            const Eigen::MatrixXd& m = std::get<Eigen::MatrixXd>(f);
            if(m.cols() != 1) return error(DIMENSION_MISMATCH, expr);
            Eigen::MatrixXd a(m.rows(), v.rows());
            for(Eigen::Index i = 0; i < v.rows(); i++){
                double orig = std::get<Eigen::MatrixXd>(stack.back())(i);
                std::get<Eigen::MatrixXd>(stack.back())(i) += INCR;
                Value f_incr = interpretExpr(expr);
                a.col(i) = (std::get<Eigen::MatrixXd>(f_incr) - std::get<Eigen::MatrixXd>(f)) / INCR;
                std::get<Eigen::MatrixXd>(stack.back())(i) = orig;
            }
            ans = a;
        }
    }

    stack.pop();

    return ans;
}

double Interpreter::pNorm(const Eigen::MatrixXd& a, double b) noexcept{
    //EVENTUALLY: eigen has a templated version of lpNorm<b>() for codegen

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

std::string Interpreter::formatted(double num){
    std::string str = std::to_string(num);

    auto dec = str.find('.');
    if(dec != std::string::npos){
        // Remove trailing zeroes
        str = str.substr(0, str.find_last_not_of('0')+1);
        // If the decimal point is now the last character, remove that as well
        if(str.find('.') == str.size()-1)
            str = str.substr(0, str.size()-1);
    }

    bool is_negative_zero = (str.front() == '-' && str.size() == 2 && str[1] == '0');
    if(is_negative_zero) return "0";

    return str;
}

Value Interpreter::factorial(ParseNode pn) {
    Value v = interpretExpr(parse_tree.child(pn));
    if(v.index() != double_index) return error(TYPE_ERROR, pn);

    if(std::get<double>(v) < 0) return error(EXPECT_POSITIVE_INT, pn);
    size_t a = std::get<double>(v);

    static constexpr bool bit64 = (8*sizeof(size_t) == 64);
    static constexpr bool bit32 = (8*sizeof(size_t) == 32);
    if(bit64 && a > 20) return error(CALC_OVERFLOW, pn);
    else if(bit32 && a > 12) return error(CALC_OVERFLOW, pn);

    size_t ans = 1;
    while(a > 1) ans *= a--;
    return static_cast<double>(ans);
}

Value Interpreter::binomial(ParseNode pn){
    ParseNode lhs = parse_tree.lhs(pn);
    Value vl = interpretExpr(lhs);
    if(vl.index() != double_index) return error(TYPE_ERROR, lhs);
    if(std::get<double>(vl) < 1) return error(EXPECT_NATURAL_NUMBER, lhs);
    size_t n = std::get<double>(vl);

    ParseNode rhs = parse_tree.rhs(pn);
    Value vr = interpretExpr(rhs);
    if(vr.index() != double_index) return error(TYPE_ERROR, rhs);
    if(std::get<double>(vr) < 0) return error(EXPECT_POSITIVE_INT, rhs);
    size_t k = std::get<double>(vr);
    if(k > n) return error(BINOM_K_EXCEED_N, pn);

    //EVENTUALLY: handle overflow
    double res = 1;
    for (int i = 1; i <= k; ++i)
        res = res * (n - k + i) / i;

    return res;
}

}

}
