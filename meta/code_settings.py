from utils import cpp, table_reader


def main():
    entries = table_reader.csv_to_list_of_tuples(
        csv_filepath="code_settings.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="code_settings_constants",
        includes=["array", "string_view"],
    )

    header_writer.write(f"#define NUM_CODE_SETTINGS {len(entries)}\n\n")

    header_writer.write("enum SettingId {\n")
    for entry in entries:
        header_writer.write(f"    WARN_{entry.setting}, //{entry.description}\n")
    header_writer.write("};\n\n")

    header_writer.write("static constexpr std::array<std::string_view, NUM_CODE_SETTINGS> setting_id_labels = {\n")
    for entry in entries:
        header_writer.write(f'    "{entry.label}",\n')
    header_writer.write("};\n\n")

    header_writer.write("static constexpr std::array<std::string_view, NUM_CODE_SETTINGS> setting_descriptions = {\n")
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
        "};\n"
        "\n"
        "static constexpr std::array<std::string_view, NUM_WARNING_LEVELS> warning_labels = {\n"
        '    "No Warning",\n'
        '    "Warning",\n'
        '    "Error",\n'
        "};\n"
        "\n"
        "static constexpr std::array<std::string_view, NUM_WARNING_LEVELS> warning_descriptions = {\n"
        '    "Ignore the condition",\n'
        '    "Warn if the condition is encountered",\n'
        '    "Fail if the condition is encountered",\n'
        "};\n"
        "\n"
        '#define COMMA_SEPARATED_WARNING_LABELS "No Warning", "Warning", "Error"\n\n'
    )

    header_writer.write("typedef uint8_t SettingValue;\n\n")

    header_writer.write("static constexpr std::array<SettingValue, NUM_CODE_SETTINGS> DEFAULT_CODE_SETTINGS = {\n")
    for entry in entries:
        header_writer.write(f"    {entry.default},\n")
    header_writer.write("};\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
