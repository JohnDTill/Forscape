from utils import cpp, table_reader


def main():
    nodes = table_reader.csv_to_list_of_tuples(
        csv_filepath="parse_nodes.csv",
        tuple_name="Node",
    )

    header_writer = cpp.HeaderWriter(
        name="parsenode_ops",
        inner_namespace="Code",
        includes=["inttypes.h", "stddef.h"],
    )

    header_writer.write("typedef size_t Op;\n\n")
    for i in range(0, len(nodes)):
        header_writer.write(f"constexpr size_t OP_{nodes[i].enum} = {i};\n")
    header_writer.write("\n\n")

    header_writer.finalize()

    header_writer = cpp.HeaderWriter(
        name="parsenodegraphviz",
        inner_namespace="Code",
        includes="hope_parse_tree.h"
    )

    header_writer.write("void ParseTree::writeType(std::string& src, ParseNode n) const{\n"
                        "    switch(getOp(n)){\n")
    for node in nodes:
        header_writer.write(f"        case OP_{node.enum}: ")
        if node.label:
            header_writer.write(f"src += \"\\\"{node.label}\\\"\"; ")
        elif node.enum == "IDENTIFIER":
            header_writer.write("src += '\"'; src += str(n); src += \"\\\", fillcolor=cyan, style=filled\"; ")
        elif node.enum == "STRING":
            header_writer.write("src += \"\\\"\\\\\\\"\"; src += str(n).substr(1, str(n).size()-2); src += \"\\\\\\\"\\\", fillcolor=green, style=filled\"; ")
        else:
            header_writer.write("src += '\"'; src += str(n); src += \"\\\", fillcolor=orange, style=filled\"; ")
        header_writer.write("break;\n")
    header_writer.write("    }\n}\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
