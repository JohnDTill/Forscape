from utils import cpp, table_reader


def main():
    errors = table_reader.csv_to_list_of_tuples(
        csv_filepath="errors.csv",
        tuple_name="Error",
    )

    errors = sorted(errors, key=lambda x: x.quote == "y")
    first_quote = 0
    for i in range(0, len(errors)):
        if errors[i].quote == "y":
            first_quote = i
            break

    header_writer = cpp.HeaderWriter(
        name="error_types",
        inner_namespace="Code",
        includes=("cassert", "string"),
    )

    header_writer.write("enum ErrorCode{\n")
    for e in errors:
        header_writer.write(f"    {e.name.upper()},\n")
    header_writer.write("};\n\n")

    header_writer.write("inline std::string getMessage(ErrorCode code){\n"
                        "    switch(code){\n")
    for e in errors:
        header_writer.write(f"        case {e.name.upper()}: return \"{e.msg}")
        if e.quote == "y":
            header_writer.write(": ")
        header_writer.write("\";\n")
    header_writer.write("        default: assert(false); return \"\";\n")
    header_writer.write("    }\n}\n\n")

    header_writer.write("inline bool shouldQuote(ErrorCode code){\n"
                        f"    return code >= {errors[first_quote].name};\n"
                        "}\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
