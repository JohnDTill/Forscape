from utils import cpp, table_reader, unicode


def main():
    for script in ["subscripts", "superscripts"]:
        scripts = table_reader.csv_to_list_of_tuples(
            csv_filepath=f"unicode_{script}.csv",
            tuple_name="Script",
        )

        header_writer = cpp.HeaderWriter(
            name=f"unicode_{script}"
        )

        header_writer.write(f"#define FORSCAPE_{script.upper()}_CASES")
        for s in scripts:
            code = unicode.to_num(s.normal)
            header_writer.write(f"\\\n    case {code}:")
        header_writer.write("\n\n")

        header_writer.write(f"#define FORSCAPE_{script.upper()}_CONVERSION")
        for s in scripts:
            code = unicode.to_num(s.normal)
            header_writer.write(f"\\\n    case {code}: out += \"{s.scripted}\"; break;")
        header_writer.write("\n\n")

        num_bytes = len(bytes(scripts[0].normal, 'utf-8'))
        if all(len(bytes(s.normal, 'utf-8')) == num_bytes for s in scripts):
            header_writer.write(f"#define FORSCAPE_{script.upper()}_NUM_BYTES {num_bytes}\n\n")

        header_writer.finalize()


if __name__ == "__main__":
    main()
