from utils import cpp, table_reader
import re


def main():
    rules = table_reader.csv_to_list_of_tuples(
        csv_filepath="interpreter_dispatch.csv",
        tuple_name="Rule",
    )
    all_ops = set()
    unary_ops = set()
    binary_ops = set()

    nullary_rules = []
    unary_rules = []
    binary_rules = []

    for rule in rules:
        if not rule.a:
            nullary_rules.append(rule)
        else:
            all_ops.add(rule.a)
            if not rule.b:
                ops = rule.op.split("|")
                for op in ops:
                    unary_ops.add(op)
                unary_rules.append(rule)
            else:
                ops = rule.op.split("|")
                for op in ops:
                    if op != "CALL":
                        binary_ops.add(op)
                all_ops.add(rule.b)
                binary_rules.append(rule)
    all_ops.remove("ANY")
    all_ops.remove("NOT_a")

    with open("../src/generated/hope_interpreter_gen.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("#include \"hope_interpreter.h\"\n\n")
        codegen_file.write("#include <math.h>\n")
        codegen_file.write("#include <unsupported/Eigen/MatrixFunctions>\n\n")
        codegen_file.write("using namespace Eigen;\n")
        codegen_file.write("using namespace std;\n\n")
        codegen_file.write("namespace Hope {\n\n")
        codegen_file.write("namespace Code {\n\n")

        codegen_file.write("Value Interpreter::interpretExpr(ParseNode pn) {\n"
                           "    if(error_code != NO_ERROR_FOUND) return &error_code;\n"
                           "    switch( parse_tree.getOp(pn) ){\n")
        for rule in nullary_rules:
            ops = rule.op.split('|')
            if len(ops) == 1:
                codegen_file.write(f"        case OP_{ops[0]}: return {rule.impl};\n")
            else:
                for op in ops:
                    codegen_file.write(f"        case OP_{op}:\n")
                codegen_file.write(f"            return {rule.impl};\n")
        codegen_file.write("\n")
        for op in unary_ops:
            codegen_file.write(f"        case OP_{op}:\n")
        codegen_file.write("            return unaryDispatch(pn);\n\n")
        for op in binary_ops:
            codegen_file.write(f"        case OP_{op}:\n")
        codegen_file.write("            return binaryDispatch(pn);\n\n")
        codegen_file.write("        default: return error(UNRECOGNIZED_EXPR, pn);\n"
                           "    }\n"
                           "}\n\n")

        codegen_file.write("static constexpr uint16_t unaryCode(Op t, size_t value_op){\n"
                           "    assert((t & (value_op << 8)) == 0);\n"
                           "    assert((t | (value_op << 8)) <= std::numeric_limits<uint16_t>::max());\n"
                           "    return static_cast<uint16_t>(t | (value_op << 8));\n"
                           "}\n\n")

        codegen_file.write("Value Interpreter::unaryDispatch(ParseNode pn) {\n"
                           "    Value child = interpretExpr( parse_tree.child(pn) );\n\n"
                           "    switch( unaryCode(parse_tree.getOp(pn), child.index()) ){\n")

        for rule in unary_rules:
            ops = rule.op.split('|')
            for op in ops:
                if rule.a == "ANY":
                    ops = all_ops
                else:
                    ops = {rule.a}

                for typ in ops:
                    constraints = []
                    if rule.constraint:
                        constraints = rule.constraint.split(":")
                        impl = rule.impl
                    else:
                        impl = re.sub(r'\ba\b', f'std::get<{typ}>(child)', rule.impl)

                    codegen_file.write(f"        case unaryCode(OP_{op}, {typ}_index):\n")
            if rule.constraint:
                codegen_file.write("        {\n")
                codegen_file.write(f"            const auto& a = std::get<{typ}>(child);\n")
            for con in constraints:
                parts = con.split("=>")
                assert len(parts) == 2, f"Missing arrow for {con}"
                condition = parts[0]
                error = parts[1]
                codegen_file.write(f"            if({condition}) return error({error}, pn);\n")
            codegen_file.write(f"            return {impl};\n")
            if rule.constraint:
                codegen_file.write("        }\n")

        codegen_file.write("        default: return error(TYPE_ERROR, pn);\n"
                           "    }\n"
                           "}\n\n")

        codegen_file.write("static constexpr uint16_t binaryCode(Op t, size_t a_op, size_t b_op){\n"
                           "    assert((t & (a_op << 8) & (b_op << 12)) == 0);\n"
                           "    assert((t | (a_op << 8) | (b_op << 12)) <= std::numeric_limits<uint16_t>::max());\n"
                           "    return static_cast<uint16_t>(t | (a_op << 8) | (b_op << 12));\n"
                           "}\n\n")

        codegen_file.write("Value Interpreter::binaryDispatch(ParseNode pn) {\n"
                           "    ParseNode lhs = parse_tree.lhs(pn);\n"
                           "    ParseNode rhs = parse_tree.rhs(pn);\n"
                           "    Value vL = interpretExpr(lhs);\n"
                           "    Value vR = interpretExpr(rhs);\n"
                           "\n"
                           "    return binaryDispatch(parse_tree.getOp(pn), vL, vR, pn);\n"
                           "}\n"
                           "\n"
                           "Value Interpreter::binaryDispatch(Op op, const Value& lhs, const Value& rhs, ParseNode pn){\n"
                           "    switch( binaryCode(op, lhs.index(), rhs.index()) ){\n")

        for rule in binary_rules:
            ops = rule.op.split('|')
            for op in ops:
                if rule.a == "ANY":
                    a_ops = all_ops
                else:
                    a_ops = {rule.a}

                for a_op in a_ops:
                    if rule.b == "NOT_a":
                        b_ops = set(all_ops)
                        b_ops.remove(a_op)
                    else:
                        b_ops = {rule.b}

                    for b_op in b_ops:
                        constraints = []
                        if rule.constraint:
                            constraints = rule.constraint.split(":")
                            impl = rule.impl
                        else:
                            impl = re.sub(r'\ba\b', f'std::get<{a_op}>(lhs)', rule.impl)
                            impl = re.sub(r'\bb\b', f'std::get<{b_op}>(rhs)', impl)

                        codegen_file.write(f"        case binaryCode(OP_{op}, {a_op}_index, {b_op}_index):\n")
            if rule.constraint:
                codegen_file.write("        {\n")
                codegen_file.write(f"            const auto& a = std::get<{a_op}>(lhs);\n")
                codegen_file.write(f"            const auto& b = std::get<{b_op}>(rhs);\n")
            for con in constraints:
                parts = con.split("=>")
                assert len(parts) == 2, f"Missing arrow for {con}"
                condition = parts[0]
                error = parts[1]
                codegen_file.write(f"            if({condition}) return error({error}, pn);\n")
            codegen_file.write(f"            return {impl};\n")
            if rule.constraint:
                codegen_file.write("        }\n")

        codegen_file.write("        default: return error(TYPE_ERROR, pn);\n"
                           "    }\n"
                           "}\n\n")

        codegen_file.write("\n}\n\n}\n")


if __name__ == "__main__":
    main()
