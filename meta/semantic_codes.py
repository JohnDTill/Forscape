from utils import cpp, table_reader


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="semantic_codes.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="semantic_tags",
        includes=["string_view"],
    )

    header_writer.write("enum SemanticType {\n")
    for e in entries:
        header_writer.write(f"    SEM_{e.name.upper()},\n")
    header_writer.write("};\n\n")

    header_writer.write(
        "struct SemanticTag {\n"
        "    size_t index;\n"
        "    SemanticType type;\n"
        "\n"
        "    SemanticTag(size_t index, SemanticType type)\n"
        "        : index(index), type(type) {}\n"
        "};\n\n")

    header_writer.write("inline constexpr bool isId(SemanticType type){\n"
                       "    return type >= SEM_ID;\n"
                       "}\n\n")

    header_writer.write("#ifndef HOPE_TYPESET_HEADLESS\n")

    header_writer.write(f"constexpr size_t NUM_SEM_TYPES = {len(entries)};\n\n")

    for ch in "rgb":
        header_writer.write(f"constexpr uint8_t {ch}[NUM_SEM_TYPES] {{\n")
        for e in entries:
            header_writer.write(f"    {getattr(e, ch)},\n")
        header_writer.write("};\n\n")

    fonts = set()
    for e in entries:
        fonts.add((e.font + ' ' + e.family).replace(' ', '_').upper())

    header_writer.write(f"static constexpr size_t NUM_FONTS = {len(fonts)};\n\n")

    header_writer.write("enum Font{\n")
    for f in fonts:
        header_writer.write(f"    {f},\n")
    header_writer.write("};\n\n")

    header_writer.write(f"constexpr Font font_enum[NUM_SEM_TYPES] {{\n")
    for e in entries:
        header_writer.write(f"    {(e.font + ' ' + e.family).replace(' ', '_').upper()},\n")
    header_writer.write("};\n\n")

    header_writer.write("#endif\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
