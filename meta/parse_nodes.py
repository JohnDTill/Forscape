from utils import cpp, table_reader


def main():
    nodes = table_reader.csv_to_list_of_tuples(
        csv_filepath="parse_nodes.csv",
        tuple_name="Node",
    )

    header_writer = cpp.HeaderWriter(
        name="parsenodetype",
        inner_namespace="Code",
        includes=["inttypes.h"],
    )

    header_writer.write("typedef size_t ParseNodeType;\n\n")
    for i in range(0, len(nodes)):
        header_writer.write(f"constexpr size_t PN_{nodes[i].enum} = {i};\n")
    header_writer.write("\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
