from utils import cpp, table_reader


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="code_settings.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="code_settings_constants",
        includes=["array", "string_view", "stdint.h"],
    )

    header_writer.write(f"#define NUM_CODE_SETTINGS {len(entries)}\n\n")

    header_writer.write("enum SettingId {\n")
    for entry in entries:
        header_writer.write(f"    WARN_{entry.setting},  // {entry.description}\n")
    header_writer.write(
        "    SETTING_NONE\n"
        "};\n\n"
    )

    header_writer.write("inline constexpr std::array<std::string_view, NUM_CODE_SETTINGS> setting_names = {\n")
    for entry in entries:
        header_writer.write(f'    "{entry.setting}",\n')
    header_writer.write("};\n\n")

    header_writer.write("inline constexpr std::array<std::string_view, NUM_CODE_SETTINGS> setting_id_labels = {\n")
    for entry in entries:
        header_writer.write(f'    "{entry.label}",\n')
    header_writer.write("};\n\n")

    header_writer.write("inline constexpr std::array<std::string_view, NUM_CODE_SETTINGS> setting_descriptions = {\n")
    for entry in entries:
        header_writer.write(f'    "{entry.description}",\n')
    header_writer.write("};\n\n")

    header_writer.write(
        "#define NUM_WARNING_LEVELS 3\n"
        "\n"
        "enum WarningLevel {\n"
        "    NO_WARNING,\n"
        "    WARN,\n"
        "    ERROR,\n"
        "    WARNING_NONE\n"
        "};\n"
        "\n"
        "inline constexpr std::array<std::string_view, NUM_WARNING_LEVELS> warning_names = {\n"
        '    "NO_WARNING",\n'
        '    "WARN",\n'
        '    "ERROR",\n'
        "};\n"
        "\n"
        "inline constexpr std::array<std::string_view, NUM_WARNING_LEVELS> warning_labels = {\n"
        '    "Ignore",\n'
        '    "Warning",\n'
        '    "Error",\n'
        "};\n"
        "\n"
        "inline constexpr std::array<std::string_view, NUM_WARNING_LEVELS> warning_descriptions = {\n"
        '    "Ignore the condition",\n'
        '    "Warn if the condition is encountered",\n'
        '    "Fail if the condition is encountered",\n'
        "};\n"
        "\n"
        '#define COMMA_SEPARATED_WARNING_LABELS "Ignore", "Warning", "Error"\n\n'
    )

    header_writer.write("typedef uint8_t SettingValue;\n\n")

    header_writer.write("inline constexpr std::array<SettingValue, NUM_CODE_SETTINGS> DEFAULT_CODE_SETTINGS = {\n")
    for entry in entries:
        header_writer.write(f"    {entry.default},  // {entry.setting}\n")
    header_writer.write("};\n\n")

    header_writer.write(
        "SettingId settingFromStr(std::string_view str) noexcept;\n\n"
        "WarningLevel warningFromStr(std::string_view str) noexcept;\n\n"
    )

    header_writer.finalize()

    # Write source file
    with open("../src/generated/code_settings_constants.cpp", "w", encoding="utf-8") as codegen_file:
        codegen_file.write("//CODEGEN\n\n")
        codegen_file.write("#include \"code_settings_constants.h\"\n\n")
        codegen_file.write("#include \"forscape_common.h\"\n\n")

        codegen_file.write("namespace Forscape {\n\n")

        codegen_file.write("const FORSCAPE_UNORDERED_MAP<std::string_view, SettingId> setting_map {\n")
        for entry in entries:
            codegen_file.write(f"    {{setting_names[WARN_{entry.setting}], WARN_{entry.setting}}},\n")
        codegen_file.write("};\n\n")

        codegen_file.write(
            "SettingId settingFromStr(std::string_view str) noexcept {\n"
            "    const auto result = setting_map.find(str);\n"
            "    return result==setting_map.end() ? SETTING_NONE : result->second;\n"
            "}\n\n"
        )

        codegen_file.write(
            "const FORSCAPE_UNORDERED_MAP<std::string_view, WarningLevel> warning_map {\n"
            "    {warning_names[NO_WARNING], NO_WARNING},\n"
            "    {warning_names[WARN], WARN},\n"
            "    {warning_names[ERROR], ERROR},\n"
            "};\n\n"
        )

        codegen_file.write(
            "WarningLevel warningFromStr(std::string_view str) noexcept {\n"
            "    const auto result = warning_map.find(str);\n"
            "    return result==warning_map.end() ? WARNING_NONE : result->second;\n"
            "}\n\n"
        )

        codegen_file.write("} // namespace Forscape\n")


if __name__ == "__main__":
    main()
