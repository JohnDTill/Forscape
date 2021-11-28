import csv
import os
from collections import namedtuple


def unicode_to_num(uni):
    b = bytes(uni, 'utf-8')
    num = 0
    for i in range(0, len(b)):
        num |= b[i] << 8*i
    return num


def main():
    entries = []
    with open('typeset_shorthand.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Entry = namedtuple('Entry', fields)
        for row in reader:
            entries.append(Entry(*row))

        with open("../src/generated/typeset_shorthand.h", "w", encoding="utf-8") as codegen_file:
            codegen_file.write("#ifndef TYPESET_SHORTHAND_H\n"
                               "#define TYPESET_SHORTHAND_H\n\n")

            codegen_file.write("#include <cassert>\n"
                               "#include <limits>\n"
                               "#include <unordered_map>\n\n")

            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Typeset {\n\n")

            codegen_file.write("class Shorthand{\n"
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
                               "const std::string Shorthand::NONE = \"\";\n\n")

            codegen_file.write("const std::unordered_map<uint32_t, std::string> Shorthand::map{\n")
            for e in entries:
                first = unicode_to_num(e.first)
                second = unicode_to_num(e.second)
                assert first < 2**24
                assert second < 2**8
                key = (first << 8) | second
                codegen_file.write(f"    {{{key}, \"{e.result}\"}},\n")
            codegen_file.write("};\n\n")

            codegen_file.write("}\n\n}\n\n#endif // TYPESET_SHORTHAND_H\n")


if __name__ == "__main__":
    main()
