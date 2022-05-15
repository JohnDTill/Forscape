from utils import cpp, table_reader, unicode


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="typeset_shorthand.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="Shorthand",
        inner_namespace="Typeset",
        includes=("cassert", "limits", "string", "unordered_map"),
    )

    header_writer.write(
        "struct PairHash {\n"
        "    size_t operator()(const std::pair<uint32_t, uint32_t>& key) const noexcept {\n"
        "        if(sizeof(size_t) > 4) return std::hash<size_t>()(\n"
        "            static_cast<size_t>(key.first) |\n"
        "            (static_cast<size_t>(key.second) << 32)\n"
        "        );\n"
        "        else return std::hash<uint32_t>()(key.first) ^ (std::hash<uint32_t>()(key.second) << 16);\n"
        "    }\n"
        "};\n\n")

    header_writer.write(
        "class Shorthand{\n"
        "public:\n"
        "    static std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> map;\n"
        "    static void reset();\n"
        "    static const std::string& lookup(uint32_t first, uint32_t second) noexcept;\n"
        "\n"
        "private:\n"
        "    static const std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> defaults;\n"
        "    static const std::string NONE;\n"
        "};\n\n"
    )

    header_writer.finalize()

    with open("../src/generated/typeset_shorthand.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("#include \"typeset_shorthand.h\"\n\n")

        codegen_file.write("namespace Hope {\n\n")
        codegen_file.write("namespace Typeset {\n\n")

        codegen_file.write("const std::string Shorthand::NONE = \"\";\n\n")

        codegen_file.write(
            "const std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> Shorthand::defaults{\n")
        for e in entries:
            first = unicode.to_num(e.first)
            second = unicode.to_num(e.second)
            key = f"std::make_pair({first}, {second})"
            codegen_file.write(f"    {{{key}, \"{e.result}\"}},\n")
        codegen_file.write("};\n\n")

        codegen_file.write(
            "void Shorthand::reset(){ map = defaults; }\n\n"
            "const std::string& Shorthand::lookup(uint32_t first, uint32_t second) noexcept{\n"
            "    auto result = map.find(std::make_pair(first, second));\n"
            "    return result==map.end() ? NONE : result->second;\n"
            "}\n\n"
        )

        codegen_file.write(
            "std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> Shorthand::map = defaults;\n\n"
            "}\n\n}\n"
        )


if __name__ == "__main__":
    main()
