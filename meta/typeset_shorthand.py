from utils import cpp, table_reader, unicode


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="typeset_shorthand.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="Shorthand",
        inner_namespace="Typeset",
        includes=("cassert", "limits", "unordered_map"),
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
        "private:\n"
        "    static const std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> map;\n"
        "    static const std::string NONE;\n"
        "\n"
        "public:\n"
        "    static const std::string& lookup(uint32_t first, uint32_t second) noexcept{\n"
        "        auto result = map.find(std::make_pair(first, second));\n"
        "        return result==map.end() ? NONE : result->second;\n"
        "    }\n"
        "};\n\n"
        "const std::string Shorthand::NONE = \"\";\n\n"
    )
    header_writer.write("const std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash> Shorthand::map{\n")
    for e in entries:
        first = unicode.to_num(e.first)
        second = unicode.to_num(e.second)
        key = f"std::make_pair({first}, {second})"
        header_writer.write(f"    {{{key}, \"{e.result}\"}},\n")
    header_writer.write("};\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
