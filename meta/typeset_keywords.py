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
        includes=("hope_construct_codes.h", "string_view", "unordered_map", "vector"),
    )

    header_writer.write("class Keywords{\n"
                        "private:\n"
                        "    static const std::unordered_map<std::string_view, std::string> map;\n"
                        "    static const std::string NONE;\n"
                        "\n"
                        "    static const std::string con(size_t code, size_t arity, std::vector<size_t> args){\n"
                        "        std::string str;\n"
                        "        str.reserve(2+arity+args.size());\n"
                        "        str += static_cast<char>(OPEN);\n"
                        "        str += static_cast<char>(code);\n"
                        "        for(const size_t arg : args) str += static_cast<char>(arg);\n"
                        "        for(size_t i = 0; i < arity; i++) str += static_cast<char>(CLOSE);\n"
                        "        return str;\n"
                        "    }\n"
                        "\n"
                        "public:\n"
                        "    static const std::string& lookup(std::string_view key) noexcept{\n"
                        "        auto result = map.find(key);\n"
                        "        return result==map.end() ? NONE : result->second;\n"
                        "    }\n"
                        "};\n\n"
                        )

    header_writer.write(f"#define HOPE_NUM_KEYWORDS {len(keywords)}\n\n")

    header_writer.write("#define HOPE_KEYWORD_LIST {\\\n")
    for keyword in keywords:
        header_writer.write(f"    {{\"{keyword.keyword}\", \"{keyword.symbol}\"}},\\\n")
    header_writer.write("}\n\n")

    header_writer.finalize()

    with open("../src/generated/typeset_keywords.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("#include \"typeset_keywords.h\"\n\n")

        codegen_file.write("namespace Hope {\n\n")
        codegen_file.write("namespace Typeset {\n\n")

        codegen_file.write("const std::string Keywords::NONE = \"\";\n\n")

        codegen_file.write("const std::unordered_map<std::string_view, std::string> Keywords::map{\n")
        for keyword in keywords:
            codegen_file.write(f"    {{\"{keyword.keyword}\", \"{keyword.symbol}\"}},\n")
        for c in constructs:
            if c.keyword != "":
                if c.arity == "nxm":
                    n = int(c.default_args[0])
                    m = int(c.default_args[1])
                    arity = n * m
                elif c.arity == "2xn":
                    arity = 2*int(c.default_args)
                else:
                    arity = c.arity

                args = "{"
                if c.default_args != "":
                    args += c.default_args[0]
                    for i in range(1,len(c.default_args)):
                        args += ", " + c.default_args[i]
                args += "}"

                codegen_file.write(f"    {{\"{c.keyword}\", Keywords::con({c.name.upper()}, {arity}, {args})}},\n")
        codegen_file.write("};\n\n}\n\n}\n\n")


if __name__ == "__main__":
    main()
