from utils import table_reader


def main():
    nodes = table_reader.csv_to_list_of_tuples(
        csv_filepath="parse_nodes.csv",
        tuple_name="Node",
    )

    with open("../src/generated/hope_parsenodetype.h", "w") as codegen_file:
        codegen_file.write("//CODEGEN FILE\n\n"
                           "#ifndef HOPE_PARSENODETYPE_H\n"
                           "#define HOPE_PARSENODETYPE_H\n\n")
        codegen_file.write("#include <inttypes.h>\n\n")
        codegen_file.write("namespace Hope {\n\n")
        codegen_file.write("namespace Code {\n\n")

        codegen_file.write("typedef size_t ParseNodeType;\n\n")

        for i in range(0, len(nodes)):
            codegen_file.write(f"constexpr size_t PN_{nodes[i].enum} = {i};\n")

        codegen_file.write("\n\n")

        codegen_file.write("\n}\n\n}\n\n#endif // HOPE_PARSENODETYPE_H\n")


if __name__ == "__main__":
    main()
