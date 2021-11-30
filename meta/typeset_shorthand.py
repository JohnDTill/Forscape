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
        "class Shorthand{\n"
        "private:\n"
        "    static const std::unordered_map<uint32_t, std::string> map;\n"
        "    static const std::string NONE;\n"
        "\n"
        "public:\n"
        "    static const std::string& lookup(uint32_t first, uint32_t second) noexcept{\n"
        "        assert(second <= std::numeric_limits<uint8_t>::max());\n"
        "        assert(first <= (std::numeric_limits<uint32_t>::max() >> 8));\n"
        "        uint32_t key = (first << 8) | second;\n"
        "        auto result = map.find(key);\n"
        "        return result==map.end() ? NONE : result->second;\n"
        "    }\n"
        "};\n\n"
        "const std::string Shorthand::NONE = \"\";\n\n"
    )
    header_writer.write("const std::unordered_map<uint32_t, std::string> Shorthand::map{\n")
    for e in entries:
        first = unicode.to_num(e.first)
        second = unicode.to_num(e.second)
        assert first < 2 ** 24
        assert second < 2 ** 8
        key = (first << 8) | second
        header_writer.write(f"    {{{key}, \"{e.result}\"}},\n")
    header_writer.write("};\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
