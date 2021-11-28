import csv
import os
from collections import namedtuple


def main():
    with open('parse_nodes.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile, delimiter=',')
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Node = namedtuple('Node', fields)
        nodes = []
        for row in reader:
            row[0] = "PN_" + row[0]
            nodes.append(Node(*row))

        with open("../src/generated/hope_parsenodetype.h", "w") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef HOPE_PARSENODETYPE_H\n"
                               "#define HOPE_PARSENODETYPE_H\n\n")
            codegen_file.write("#include <inttypes.h>\n\n")
            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Code {\n\n")

            codegen_file.write("typedef size_t ParseNodeType;\n\n")

            for i in range(0, len(nodes)):
                codegen_file.write(f"constexpr size_t {nodes[i].enum} = {i};\n")

            codegen_file.write("\n\n")

            codegen_file.write("\n}\n\n}\n\n#endif // HOPE_PARSENODETYPE_H\n")


if __name__ == "__main__":
    main()
