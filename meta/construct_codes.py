import os
from utils import cpp, table_reader
import glob


def main():
    constructs = table_reader.csv_to_list_of_tuples(
        csv_filepath="construct_codes.csv",
        tuple_name="Construct",
    )

    header_writer = cpp.HeaderWriter(
        name="construct_codes",
    )

    header_writer.write("constexpr char OPEN = 2;\n"
                        "constexpr char CLOSE = 3;\n\n")

    curr = 1
    for con in constructs:
        header_writer.write(f"constexpr char {con.name.upper()} = {curr};\n")
        curr += 1
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_NULLARY_CASES")
    for name in [entry.name for entry in constructs if entry.arity == "0"]:
        header_writer.write(f" \\\n    case {name.upper()}:")
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_UNARY_CASES")
    for name in [entry.name for entry in constructs if entry.arity == "1"]:
        header_writer.write(f" \\\n    case {name.upper()}:")
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_BINARY_CASES")
    for name in [entry.name for entry in constructs if entry.arity == "2"]:
        header_writer.write(f" \\\n    case {name.upper()}:")
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_MATRIX_CASES")
    for name in [entry.name for entry in constructs if entry.arity == "nxm"]:
        header_writer.write(f" \\\n    case {name.upper()}:")
    header_writer.write("\n")

    header_writer.write("\n")
    header_writer.write("#define FORSCAPE_TYPESET_PARSER_CASES")
    for entry in constructs:
        name = entry.name
        header_writer.write(f" \\\n    case {name.upper()}: TypesetSetup")
        if entry.arity == "0":
            header_writer.write(f"Nullary({name});")
        elif entry.arity == "nxm":
            header_writer.write(f"Matrix({name});")
        elif entry.arity == "2xn":
            header_writer.write(f"Construct({name}, static_cast<uint8_t>(src[index++]));")
        else:
            header_writer.write(f"Construct({name},);")
    header_writer.write("\n")

    header_writer.finalize()

    for entry in constructs:
        if entry.implemented == "y":
            continue
        header_writer = cpp.HeaderWriter(
            name="typeset_{entry.name.lower()}",
            includes=["typeset_construct.h", "typeset_subphrase.h"],
        )

        header_writer.write(f"class {entry.name} final : public Construct {{ \n")
        header_writer.write("public:\n")

        if entry.arity == "1":
            header_writer.write(f"    {entry.name}(){{\n"
                                "        setupUnaryArg();\n"
                                "    }\n\n")
        elif entry.arity == "2":
            header_writer.write(f"    {entry.name}(){{\n"
                                "        setupBinaryArgs();\n"
                                "    }\n\n")
        elif entry.arity == "2xn":
            header_writer.write(f"    {entry.name}(uint8_t n){{\n"
                                "        setupNAargs(2*n);\n"
                                "    }\n\n")
            header_writer.write("    virtual void writeArgs(std::string& out, size_t& curr) "
                                "const noexcept override {\n"
                                "        out[curr++] = static_cast<uint8_t>(numArgs()/2);\n"
                                "    }\n\n")
            header_writer.write("    virtual size_t dims() const noexcept override { "
                                "return 1; }\n")
        elif entry.arity == "nxm":
            header_writer.write("    uint16_t rows;\n"
                                "    uint16_t cols;\n\n")
            header_writer.write(f"    {entry.name}(uint16_t rows, uint16_t cols)\n"
                                "        : rows(rows), cols(cols) {\n"
                                "        setupNAargs(rows*cols);\n"
                                "    }\n\n")
            header_writer.write("    virtual void writeArgs(std::string& out, size_t& curr) "
                                "const noexcept override {\n"
                                "        out[curr++] = static_cast<uint8_t>(rows);\n"
                                "        out[curr++] = static_cast<uint8_t>(cols);\n"
                                "    }\n\n")
            header_writer.write("    virtual size_t dims() const noexcept override { "
                                "return 2; }\n")

        header_writer.write("    virtual char constructCode() const noexcept override { return ")
        header_writer.write(entry.name.upper())
        header_writer.write("; }\n")

        if entry.script_child == "y":
            header_writer.write("    virtual bool increasesScriptDepth() const noexcept override "
                                "{ return true; }\n")

        if entry.parent == "BigSymbol0":
            header_writer.write(
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
            header_writer.write(
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
            header_writer.write(
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

        header_writer.write("};\n\n")
        header_writer.finalize()

    old_constructs = table_reader.csv_to_list_of_tuples(
        csv_filepath="cache/construct_codes.csv",
        tuple_name="OldConstruct",
    )

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
