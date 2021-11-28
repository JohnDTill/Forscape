import csv
import os
from collections import namedtuple


def main():
    with open('semantic_codes.csv') as csvfile:
        reader = csv.reader(csvfile)
        next(reader, None)  # skip the headers
        Entry = namedtuple('Entry', 'name font family r g b')
        entries = []
        for row in reader:
            entries.append(Entry("SEM_"+row[0].upper(), row[1], row[2], row[3], row[4], row[5]))

        with open("../src/generated/typeset_semantic_tags.h", "w") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef TYPESET_SEMANTIC_TAGS_H\n"
                               "#define TYPESET_SEMANTIC_TAGS_H\n\n")
            codegen_file.write("#include <string_view>\n\n")
            codegen_file.write("namespace Hope {\n\n")

            codegen_file.write("enum SemanticType {\n")
            for e in entries:
                codegen_file.write(f"    {e.name},\n")
            codegen_file.write("};\n\n")

            codegen_file.write(
                "struct SemanticTag {\n"
                "    size_t index;\n"
                "    SemanticType type;\n"
                "\n"
                "    SemanticTag(size_t index, SemanticType type)\n"
                "        : index(index), type(type) {}\n"
                "};\n\n")

            codegen_file.write("inline constexpr bool isId(SemanticType type){\n"
                               "    return type >= SEM_ID;\n"
                               "}\n\n")

            codegen_file.write("#ifndef HOPE_TYPESET_HEADLESS\n")

            for ch in "rgb":
                codegen_file.write(f"constexpr uint8_t {ch}[{len(entries)}] {{\n")
                for e in entries:
                    codegen_file.write(f"    {getattr(e, ch)},\n")
                codegen_file.write("};\n\n")

            codegen_file.write(f"constexpr std::string_view font_names[{len(entries)}] {{\n")
            for e in entries:
                codegen_file.write(f"    \"{e.font}\",\n")
            codegen_file.write("};\n\n")

            codegen_file.write(f"constexpr std::string_view font_family[{len(entries)}] {{\n")
            for e in entries:
                codegen_file.write(f"    \"{e.family}\",\n")
            codegen_file.write("};\n\n")

            codegen_file.write("#endif\n\n")

            codegen_file.write("}\n\n#endif // TYPESET_SEMANTIC_TAGS_H\n")


if __name__ == "__main__":
    main()
