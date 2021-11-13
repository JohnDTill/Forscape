import csv
import os
from collections import namedtuple
import glob
import shutil


def main():
    with open('construct_codes.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        constructs = []
        headers = next(reader, None)  # skip the headers
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Construct = namedtuple('Construct', fields)
        for row in reader:
            constructs.append(Construct(*row))

        with open("../include/hope_construct_codes.h", "w") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef HOPE_CONSTRUCT_CODES_H\n"
                               "#define HOPE_CONSTRUCT_CODES_H\n\n")
            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("constexpr char OPEN = 2;\n"
                               "constexpr char CLOSE = 3;\n\n")

            curr = 1
            for con in constructs:
                codegen_file.write(f"constexpr char {con.name.upper()} = {curr};\n")
                curr += 1
            codegen_file.write("\n")

            codegen_file.write("#define HOPE_SERIAL_NULLARY_CASES")
            for name in [entry.name for entry in constructs if entry.arity == "0"]:
                codegen_file.write(f" \\\n    case {name.upper()}:")
            codegen_file.write("\n")

            codegen_file.write("#define HOPE_SERIAL_UNARY_CASES")
            for name in [entry.name for entry in constructs if entry.arity == "1"]:
                codegen_file.write(f" \\\n    case {name.upper()}:")
            codegen_file.write("\n")

            codegen_file.write("#define HOPE_SERIAL_BINARY_CASES")
            for name in [entry.name for entry in constructs if entry.arity == "2"]:
                codegen_file.write(f" \\\n    case {name.upper()}:")
            codegen_file.write("\n")

            codegen_file.write("#define HOPE_SERIAL_MATRIX_CASES")
            for name in [entry.name for entry in constructs if entry.arity == "nxm"]:
                codegen_file.write(f" \\\n    case {name.upper()}:")
            codegen_file.write("\n")

            codegen_file.write("\n")
            codegen_file.write("#define HOPE_TYPESET_PARSER_CASES")
            for entry in constructs:
                name = entry.name
                codegen_file.write(f" \\\n    case {name.upper()}: TypesetSetup")
                if entry.arity == "0":
                    codegen_file.write(f"Nullary({name});")
                elif entry.arity == "nxm":
                    codegen_file.write(f"Matrix({name});")
                elif entry.arity == "2xn":
                    codegen_file.write(f"Construct({name}, static_cast<uint8_t>(src[index++]));")
                else:
                    codegen_file.write(f"Construct({name},);")
            codegen_file.write("\n")

            codegen_file.write("}\n\n#endif // HOPE_CONSTRUCT_CODES_H\n")

        with open("../src/typeset_constructs/typeset_all_constructs.h", "w") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef TYPESET_ALL_CONSTRUCTS_H\n"
                               "#define TYPESET_ALL_CONSTRUCTS_H\n\n")

            for name in [entry.name.lower() for entry in constructs]:
                codegen_file.write(f"#include \"typeset_{name}.h\"\n")

            codegen_file.write("\n\n#endif // TYPESET_ALL_CONSTRUCTS_H\n")

        for entry in constructs:
            if entry.implemented == "y":
                continue
            header_path = f"../src/typeset_constructs/typeset_{entry.name.lower()}.h"
            with open(header_path, "w", encoding="utf-8") as header_file:
                header_file.write("//CODEGEN FILE\n\n"
                                  f"#ifndef TYPESET_{entry.name.upper()}_H\n"
                                  f"#define TYPESET_{entry.name.upper()}_H\n\n")

                header_file.write("#include \"typeset_construct.h\"\n")
                if entry.parent == "BigSymbol0" or entry.parent == "BigSymbol1" or entry.parent == "BigSymbol2":
                    header_file.write("#include \"typeset_subphrase.h\"\n")
                header_file.write("\n")

                header_file.write("namespace Hope {\n\n"
                                  "namespace Typeset {\n\n")

                header_file.write(f"class {entry.name} final : public Construct {{ \n")
                header_file.write("public:\n")

                if entry.arity == "1":
                    header_file.write(f"    {entry.name}(){{\n"
                                      "        setupUnaryArg();\n"
                                      "    }\n\n")
                elif entry.arity == "2":
                    header_file.write(f"    {entry.name}(){{\n"
                                      "        setupBinaryArgs();\n"
                                      "    }\n\n")
                elif entry.arity == "2xn":
                    header_file.write(f"    {entry.name}(uint8_t n){{\n"
                                      "        setupNAargs(2*n);\n"
                                      "    }\n\n")
                    header_file.write("    virtual void writeArgs(std::string& out, size_t& curr) "
                                      "const noexcept override {\n"
                                      "        out[curr++] = static_cast<uint8_t>(numArgs()/2);\n"
                                      "    }\n\n")
                    header_file.write("    virtual size_t dims() const noexcept override { "
                                      "return 1; }\n")
                elif entry.arity == "nxm":
                    header_file.write("    uint16_t rows;\n"
                                      "    uint16_t cols;\n\n")
                    header_file.write(f"    {entry.name}(uint16_t rows, uint16_t cols)\n"
                                      "        : rows(rows), cols(cols) {\n"
                                      "        setupNAargs(rows*cols);\n"
                                      "    }\n\n")
                    header_file.write("    virtual void writeArgs(std::string& out, size_t& curr) "
                                      "const noexcept override {\n"
                                      "        out[curr++] = static_cast<uint8_t>(rows);\n"
                                      "        out[curr++] = static_cast<uint8_t>(cols);\n"
                                      "    }\n\n")
                    header_file.write("    virtual size_t dims() const noexcept override { "
                                      "return 2; }\n")

                header_file.write("    virtual char constructCode() const noexcept override { return ")
                header_file.write(entry.name.upper())
                header_file.write("; }\n")

                if entry.script_child == "y":
                    header_file.write("    virtual bool increasesScriptDepth() const noexcept override "
                                      "{ return true; }\n")

                if entry.parent == "BigSymbol0":
                    header_file.write(
"\n    virtual void updateSizeSpecific() noexcept override {\n"
f"        width = getWidth(SEM_DEFAULT, parent->script_level, \"{entry.label}\");\n"
"        above_center = getAboveCenter(SEM_DEFAULT, parent->script_level);\n"
"        under_center = getUnderCenter(SEM_DEFAULT, parent->script_level);\n"
"    }\n"
"\n"
"    virtual void paintSpecific(Painter& painter) const override {\n"
f"        painter.drawSymbol(x, y, \"{entry.label}\");\n"
"    }\n"
                    )
                elif entry.parent == "BigSymbol1":
                    header_file.write(
"    double symbol_width;\n"
"\n"
"    virtual void updateSizeSpecific() noexcept override {\n"
f"        symbol_width = getWidth(SEM_DEFAULT, parent->script_level, \"{entry.label}\");\n"
"        width = std::max(symbol_width, child()->width);\n"
"        above_center = getAboveCenter(SEM_DEFAULT, parent->script_level);\n"
"        under_center = getUnderCenter(SEM_DEFAULT, parent->script_level) + child()->height();\n"
"    }\n"
"\n"
"    virtual void updateChildPositions() override {\n"
"        child()->x = x + (width - child()->width)/2;\n"
"        child()->y = y + height() - child()->height();\n"
"    }\n"
"\n"
"    virtual void paintSpecific(Painter& painter) const override {\n"
"        double symbol_x = x + (width - symbol_width) / 2;\n"
f"        painter.drawSymbol(symbol_x, y, \"{entry.label}\");\n"
"    }\n"
                    )
                elif entry.parent == "BigSymbol2":
                    header_file.write(
"    double symbol_width;\n"
"\n"
"virtual void updateSizeSpecific() noexcept override {\n"
f"        symbol_width = 1*getWidth(SEM_DEFAULT, parent->script_level, \"{entry.label}\");\n"
"        width = std::max(symbol_width, std::max(first()->width, second()->width));\n"
"        above_center = getAboveCenter(SEM_DEFAULT, parent->script_level) + first()->height();\n"
"        under_center = 1*getUnderCenter(SEM_DEFAULT, parent->script_level) + second()->height();\n"
"    }\n"
"\n"
"    virtual void updateChildPositions() override {\n"
"        first()->x = x + (width - first()->width)/2;\n"
"        first()->y = y;\n"
"        second()->x = x + (width - second()->width)/2;\n"
"        second()->y = y + height() - second()->height();\n"
"    }\n"
"\n"
"    virtual void paintSpecific(Painter& painter) const override {\n"
"        double symbol_x = x + (width - symbol_width) / 2;\n"
f"        painter.drawSymbol(symbol_x, y + second()->height(), \"{entry.label}\");\n"
"    }\n"
                    )

                header_file.write("};\n\n}\n\n}")
                header_file.write(f"\n\n#endif // TYPESET_{entry.name.upper()}_H\n")

        with open('cache/construct_codes.csv', encoding="utf-8") as csvfile:
            reader = csv.reader(csvfile)
            old_constructs = []
            headers = next(reader, None)  # skip the headers
            fields = headers[0]
            for i in range(1, len(headers)):
                fields += ' ' + headers[i]
            OldConstruct = namedtuple('OldConstruct', fields)
            for row in reader:
                old_constructs.append(OldConstruct(*row))

            changes = {}
            for i in range(0, len(old_constructs)):
                oc = old_constructs[i]
                for j in range(0, len(constructs)):
                    c = constructs[j]
                    if oc.name == c.name:
                        if i != j:
                            changes[chr(i+1)] = chr(j+1)
                        break

            if len(changes) > 0:
                dirs = ["../test"] #, "../example"

                for dir in dirs:
                    files = glob.iglob(dir + '**/**', recursive=True)
                    files = [f for f in files if os.path.isfile(f)]
                    for f in files:
                        with open(f, 'r', encoding="utf-8") as file:
                            filedata = file.read()

                        #Since '' is a construct code, search and replace is complicated
                        newstr = ""
                        for i in range(0, len(filedata)):
                            newstr += filedata[i]
                            if filedata[i] == '':
                                i += 1
                                assert i < len(filedata)
                                if filedata[i] in changes.keys():
                                    newstr += changes.get(filedata[i])
                                else:
                                    newstr += filedata[i]
                        filedata = newstr

                        with open(f, 'w', encoding="utf-8") as file:
                            file.write(filedata)


if __name__ == "__main__":
    main()
