from utils import cpp, table_reader


def main():
    constructs = table_reader.csv_to_list_of_tuples(
        csv_filepath="construct_codes.csv",
        tuple_name="Construct",
    )

    keywords = table_reader.csv_to_list_of_tuples(
        csv_filepath="typeset_keywords.csv",
        tuple_name="Shortcut",
    )

    header_writer = cpp.HeaderWriter(
        name="keywords",
        inner_namespace="Typeset",
        includes=("construct_codes.h", "forscape_common.h", "string", "vector"),
    )

    header_writer.write("class Keywords{\n"
                        "public:\n"
                        "    static FORSCAPE_UNORDERED_MAP<std::string, std::string> map;\n"
                        "    static const std::string NONE;\n"
                        "\n"
                        "    static const std::string& lookup(const std::string& key) noexcept{\n"
                        "        auto result = map.find(key);\n"
                        "        return result==map.end() ? NONE : result->second;\n"
                        "    }\n"
                        "\n"
                        "private:\n"
                        "    static const FORSCAPE_UNORDERED_MAP<std::string, std::string> defaults;\n"
                        "public:\n"
                        "    static void reset() { map = defaults; }\n"
                        "};\n\n"
                        )

    header_writer.write(f"#define FORSCAPE_NUM_KEYWORDS {len(keywords)}\n\n")

    header_writer.write("#define FORSCAPE_KEYWORD_LIST")
    first = True
    for keyword in keywords:
        if not first:
            header_writer.write(",")
        first = False
        header_writer.write(f"\\\n    {{\"{keyword.keyword}\", \"{keyword.symbol}\"}}")
    header_writer.write("\n\n")

    header_writer.finalize()

    with open("../src/generated/typeset_keywords.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("#include \"typeset_keywords.h\"\n\n")

        codegen_file.write("namespace Forscape {\n\n")
        codegen_file.write("namespace Typeset {\n\n")

        codegen_file.write("const std::string Keywords::NONE = \"\";\n\n")

        codegen_file.write("const FORSCAPE_UNORDERED_MAP<std::string, std::string> Keywords::defaults {\n"
                           "    FORSCAPE_KEYWORD_LIST,\n")
        for c in constructs:
            if c.keyword != "":
                if not c.keyword:
                    continue
                if c.arity == "0":
                    codegen_file.write(f"    {{\"{c.keyword}\", OPEN_STR {c.name.upper()}_STR}},\n")
                elif c.arity == "1":
                    codegen_file.write(f"    {{\"{c.keyword}\", OPEN_STR {c.name.upper()}_STR CLOSE_STR}},\n")
                elif c.arity == "2":
                    codegen_file.write(f"    {{\"{c.keyword}\", OPEN_STR {c.name.upper()}_STR CLOSE_STR CLOSE_STR}},\n")
                elif not c.arity:
                    codegen_file.write(f"    {{\"{c.keyword}\", OPEN_STR {c.name.upper()}_STR \"{c.default_args}\"}},\n")
        codegen_file.write('    {"^T", OPEN_STR SUPERSCRIPT_STR "⊤" CLOSE_STR},\n')
        codegen_file.write('    {"^+", OPEN_STR SUPERSCRIPT_STR "+" CLOSE_STR},\n')
        codegen_file.write('    {"^*", OPEN_STR SUPERSCRIPT_STR "*" CLOSE_STR},\n')
        codegen_file.write('    {"^-1", OPEN_STR SUPERSCRIPT_STR "-1" CLOSE_STR},\n')
        codegen_file.write('    {"^dag", OPEN_STR SUPERSCRIPT_STR "†" CLOSE_STR},\n')
        codegen_file.write(
            f'   {{"cases", OPEN_STR CASES_STR "\\{hex(2)[1:]}" CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR}},\n')
        codegen_file.write(
            f'   {{"mat", OPEN_STR MATRIX_STR "\\{hex(2)[1:]}\\{hex(2)[1:]}" CLOSE_STR CLOSE_STR CLOSE_STR CLOSE_STR}},\n')
        codegen_file.write("};\n\n")

        codegen_file.write("FORSCAPE_UNORDERED_MAP<std::string, std::string> Keywords::map = defaults;\n\n"
                           "}\n\n}\n\n")


if __name__ == "__main__":
    main()
