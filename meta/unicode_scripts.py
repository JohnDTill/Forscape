from utils import table_reader


def unicode_to_num(uni):
    b = bytes(uni, 'utf-8')
    num = 0
    for i in range(0, len(b)):
        num |= b[i] << 8*i
    return num


def main():
    for script in ["subscripts", "superscripts"]:
        scripts = table_reader.csv_to_list_of_tuples(
            csv_filepath=f"unicode_{script}.csv",
            tuple_name="Script",
        )

        with open(f"../src/generated/unicode_{script}.h", "w", encoding="utf-8") as codegen_file:
            script = script.upper()
            codegen_file.write("//CODEGEN FILE\n\n"
                               f"#ifndef HOPE_UNICODE_{script}_H\n"
                               f"#define HOPE_UNICODE_{script}_H\n\n")

            codegen_file.write(f"#define HOPE_{script}_CASES")
            for s in scripts:
                code = unicode_to_num(s.normal)
                codegen_file.write(f"\\\n    case {code}:")
            codegen_file.write("\n\n")

            codegen_file.write(f"#define HOPE_{script}_CONVERSION")
            for s in scripts:
                code = unicode_to_num(s.normal)
                codegen_file.write(f"\\\n    case {code}: out += \"{s.scripted}\"; break;")
            codegen_file.write("\n\n")

            num_bytes = len(bytes(scripts[0].normal, 'utf-8'))
            if all(len(bytes(s.normal, 'utf-8')) == num_bytes for s in scripts):
                codegen_file.write(f"#define HOPE_{script}_NUM_BYTES {num_bytes}\n\n")

            codegen_file.write(f"#endif // HOPE_UNICODE_{script}_H\n")


if __name__ == "__main__":
    main()
