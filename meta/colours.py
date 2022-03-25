from utils import cpp, table_reader


def to_id(s):
    return ''.join(filter(str.isalnum, s)).upper()


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="colours.csv",
    )

    headers = table_reader.csv_headers(
        csv_filepath="colours.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="themes",
        inner_namespace="Typeset",
        includes=["string_view", "QColor"],
    )

    header_writer.write("#ifndef HOPE_HEADLESS\n\n")

    for i in range(1, len(headers)):
        header = to_id(headers[i])
        header_writer.write(f"constexpr size_t PRESET_{header} = {i-1};\n")
    header_writer.write(f"constexpr size_t NUM_COLOUR_PRESETS = {len(headers)-1};\n\n")

    for i in range(0, len(entries)):
        role = to_id(entries[i].role)
        header_writer.write(f"constexpr size_t COLOUR_{role} = {i};\n")
    header_writer.write(f"constexpr size_t NUM_COLOUR_ROLES = {len(entries)};\n\n")

    header_writer.write(
        "void setColour(size_t role, const QColor& colour) noexcept;\n"
        "const QColor& getColour(size_t role) noexcept;\n"
        "void setPreset(size_t preset) noexcept;\n"
        "std::string_view getColourName(size_t role) noexcept;\n"
        "std::string_view getPresetName(size_t preset) noexcept;\n\n"
    )

    header_writer.write("#endif HOPE_HEADLESS\n\n")
    header_writer.finalize()

    with open("../src/generated/typeset_themes.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("#include \"typeset_themes.h\"\n\n")
        codegen_file.write("#include <array>\n\n")

        codegen_file.write("namespace Hope {\n\n")
        codegen_file.write("namespace Typeset {\n\n")

        codegen_file.write("static std::array<QColor, NUM_COLOUR_ROLES> colours;\n\n")

        codegen_file.write("static constexpr std::array<std::string_view, NUM_COLOUR_ROLES> colour_names {\n")
        for e in entries:
            codegen_file.write(f"    \"{e.role}\",\n")
        codegen_file.write("};\n\n")

        codegen_file.write("static constexpr std::array<std::string_view, NUM_COLOUR_PRESETS> preset_names {\n")
        for i in range(1, len(headers)):
            codegen_file.write(f"    \"{headers[i]}\",\n")
        codegen_file.write("};\n\n")

        codegen_file.write("static const std::array<std::array<QColor, NUM_COLOUR_ROLES>, NUM_COLOUR_PRESETS> presets {\n")
        for i in range(1, len(headers)):
            codegen_file.write("    std::array<QColor, NUM_COLOUR_ROLES>({\n")
            for e in entries:
                colour = f"QColor({e[i].replace('|', ', ')})"
                codegen_file.write(f"        {colour},\n")
            codegen_file.write("    }),\n")
        codegen_file.write("};\n\n")

        codegen_file.write(
            "void setColour(size_t role, const QColor& colour) noexcept {\n"
            "    assert(role < NUM_COLOUR_ROLES);\n"
            "    colours[role] = colour;\n"
            "}\n\n"
            "const QColor& getColour(size_t role) noexcept {\n"
            "    assert(role < NUM_COLOUR_ROLES);\n"
            "    return colours[role];\n"
            "}\n\n"
            "void setPreset(size_t preset) noexcept {\n"
            "    assert(preset < NUM_COLOUR_PRESETS);\n"
            "    colours = presets[preset];\n"
            "}\n\n"
            "std::string_view getColourName(size_t role) noexcept {\n"
            "    assert(role < NUM_COLOUR_ROLES);\n"
            "    return colour_names[role];\n"
            "}\n\n"
            "std::string_view getPresetName(size_t preset) noexcept {\n"
            "    assert(preset < NUM_COLOUR_PRESETS);\n"
            "    return preset_names[preset];\n"
            "}\n\n"
        )

        codegen_file.write("\n}\n\n}\n")


if __name__ == "__main__":
    main()
