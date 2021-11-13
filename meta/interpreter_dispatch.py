import csv
import os
from collections import namedtuple
import re


def main():
    rules = []
    all_types = set()
    unary_ops = set()
    binary_ops = set()
    with open('interpreter_dispatch.csv') as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Rule = namedtuple('Rule', fields)
        for row in reader:
            rules.append(Rule(*row))

        nullary_rules = []
        unary_rules = []
        binary_rules = []

        for rule in rules:
            if not rule.a:
                nullary_rules.append(rule)
            else:
                all_types.add(rule.a)
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
                    all_types.add(rule.b)
                    binary_rules.append(rule)
        all_types.remove("ANY")
        all_types.remove("NOT_a")

        with open("../src/hope_interpreter_gen.cpp", "w", encoding="utf-8") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n")
            codegen_file.write("#include \"hope_interpreter.h\"\n\n")
            codegen_file.write("#include \"hope_error.h\" //DO THIS: REMOVE\n\n")
            codegen_file.write("#include <math.h>\n\n")
            codegen_file.write("#ifdef HOPE_EIGEN_UNSUPPORTED\n"
                               "#include <unsupported/Eigen/MatrixFunctions>\n"
                               "#endif\n")
            codegen_file.write("using namespace Eigen;\n")
            codegen_file.write("using namespace std;\n\n")
            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Code {\n\n")

            codegen_file.write("Value Interpreter::interpretExpr(ParseNode pn) {\n"
                               "    if(exit_mode != KEEP_GOING) return &errors.back(); //DO THIS - very wasteful to allows check for errors\n"
                               "\n"
                               "    switch( parse_tree.getEnum(pn) ){\n")
            for rule in nullary_rules:
                ops = rule.op.split('|')
                if len(ops) == 1:
                    codegen_file.write(f"        case PN_{ops[0]}: return {rule.impl};\n")
                else:
                    for op in ops:
                        codegen_file.write(f"        case PN_{op}:\n")
                    codegen_file.write(f"            return {rule.impl};\n")
            codegen_file.write("\n")
            for op in unary_ops:
                codegen_file.write(f"        case PN_{op}:\n")
            codegen_file.write("            return unaryDispatch(pn);\n\n")
            for op in binary_ops:
                codegen_file.write(f"        case PN_{op}:\n")
            codegen_file.write("            return binaryDispatch(pn);\n\n")
            codegen_file.write("        default: return error(UNRECOGNIZED_EXPR, pn);\n"
                               "    }\n"
                               "}\n\n")

            codegen_file.write("static constexpr uint16_t unaryCode(ParseNodeType t, size_t value_type){\n"
                               "    assert((t & (value_type << 8)) == 0);\n"
                               "    assert((t | (value_type << 8)) <= std::numeric_limits<uint16_t>::max());\n"
                               "    return static_cast<uint16_t>(t | (value_type << 8));\n"
                               "}\n\n")

            codegen_file.write("Value Interpreter::unaryDispatch(ParseNode pn) {\n"
                               "    Value child = interpretExpr( parse_tree.child(pn) );\n\n"
                               "    switch( unaryCode(parse_tree.getEnum(pn), child.index()) ){\n")

            for rule in unary_rules:
                ops = rule.op.split('|')
                for op in ops:
                    if rule.a == "ANY":
                        types = all_types
                    else:
                        types = {rule.a}

                    for typ in types:
                        constraints = []
                        if rule.constraint:
                            constraints = rule.constraint.split(":")
                            impl = rule.impl
                        else:
                            impl = re.sub(r'\ba\b', f'std::get<{typ}>(child)', rule.impl)

                        codegen_file.write(f"        case unaryCode(PN_{op}, {typ}_index):\n")
                if rule.eigen_unsupported:
                    codegen_file.write("            #ifdef HOPE_EIGEN_UNSUPPORTED\n")
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
                if rule.eigen_unsupported:
                    codegen_file.write("            #else\n"
                                       "            return error(EIGEN_UNSUPPORTED, pn);\n"
                                       "            #endif\n")

            codegen_file.write("        default: return error(TYPE_ERROR, pn);\n"
                               "    }\n"
                               "}\n\n")

            codegen_file.write("static constexpr uint16_t binaryCode(ParseNodeType t, size_t a_type, size_t b_type){\n"
                               "    assert((t & (a_type << 8) & (b_type << 12)) == 0);\n"
                               "    assert((t | (a_type << 8) | (b_type << 12)) <= std::numeric_limits<uint16_t>::max());\n"
                               "    return static_cast<uint16_t>(t | (a_type << 8) | (b_type << 12));\n"
                               "}\n\n")

            codegen_file.write("Value Interpreter::binaryDispatch(ParseNode pn) {\n"
                               "    ParseNode lhs = parse_tree.lhs(pn);\n"
                               "    ParseNode rhs = parse_tree.rhs(pn);\n"
                               "    Value vL = interpretExpr(lhs);\n"
                               "    Value vR = interpretExpr(rhs);\n"
                               "\n"
                               "    return binaryDispatch(parse_tree.getEnum(pn), vL, vR, pn);\n"
                               "}\n"
                               "\n"
                               "Value Interpreter::binaryDispatch(ParseNodeType type, const Value& lhs, const Value& rhs, ParseNode pn){\n"
                               "    switch( binaryCode(type, lhs.index(), rhs.index()) ){\n")

            for rule in binary_rules:
                ops = rule.op.split('|')
                for op in ops:
                    if rule.a == "ANY":
                        a_types = all_types
                    else:
                        a_types = {rule.a}

                    for a_type in a_types:
                        if rule.b == "NOT_a":
                            b_types = set(all_types)
                            b_types.remove(a_type)
                        else:
                            b_types = {rule.b}

                        for b_type in b_types:
                            constraints = []
                            if rule.constraint:
                                constraints = rule.constraint.split(":")
                                impl = rule.impl
                            else:
                                impl = re.sub(r'\ba\b', f'std::get<{a_type}>(lhs)', rule.impl)
                                impl = re.sub(r'\bb\b', f'std::get<{b_type}>(rhs)', impl)

                            codegen_file.write(f"        case binaryCode(PN_{op}, {a_type}_index, {b_type}_index):\n")
                if rule.eigen_unsupported:
                    codegen_file.write("            #ifdef HOPE_EIGEN_UNSUPPORTED\n")
                if rule.constraint:
                    codegen_file.write("        {\n")
                    codegen_file.write(f"            const auto& a = std::get<{a_type}>(lhs);\n")
                    codegen_file.write(f"            const auto& b = std::get<{b_type}>(rhs);\n")
                for con in constraints:
                    parts = con.split("=>")
                    assert len(parts) == 2, f"Missing arrow for {con}"
                    condition = parts[0]
                    error = parts[1]
                    codegen_file.write(f"            if({condition}) return error({error}, pn);\n")
                codegen_file.write(f"            return {impl};\n")
                if rule.constraint:
                    codegen_file.write("        }\n")
                if rule.eigen_unsupported:
                    codegen_file.write("            #else\n"
                                       "            return error(EIGEN_UNSUPPORTED, pn);\n"
                                       "            #endif\n")

            codegen_file.write("        default: return error(TYPE_ERROR, pn);\n"
                               "    }\n"
                               "}\n\n")

            codegen_file.write("\n}\n\n}\n")


if __name__ == "__main__":
    main()
