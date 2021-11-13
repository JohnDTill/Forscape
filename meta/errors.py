import csv
import os
from collections import namedtuple


def main():
    errors = []
    with open('errors.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Error = namedtuple('Error', fields)
        for row in reader:
            errors.append(Error(*row))

        errors = sorted(errors, key=lambda x: x.quote == "y")
        first_quote = 0
        for i in range(0, len(errors)):
            if errors[i].quote == "y":
                first_quote = i
                break

        with open("../src/hope_error_types.h", "w", encoding="utf-8") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef HOPE_ERROR_TYPES_H\n"
                               "#define HOPE_ERROR_TYPES_H\n\n")
            codegen_file.write("#include <cassert>\n#include <string>\n\n")
            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Code {\n\n")

            codegen_file.write("enum ErrorCode{\n")
            for e in errors:
                codegen_file.write(f"    {e.name.upper()},\n")
            codegen_file.write("};\n\n")

            codegen_file.write("inline std::string getMessage(ErrorCode code){\n"
                               "    switch(code){\n")
            for e in errors:
                codegen_file.write(f"        case {e.name.upper()}: return \"{e.msg}")
                if e.quote == "y":
                    codegen_file.write(": ")
                codegen_file.write("\";\n")
            codegen_file.write("        default: assert(false); return \"\";\n")
            codegen_file.write("    }\n}\n\n")

            codegen_file.write("inline bool shouldQuote(ErrorCode code){\n"
                               f"    return code >= {errors[first_quote].name};\n"
                               "}\n")

            codegen_file.write("\n}\n\n}\n\n#endif // HOPE_ERROR_TYPES_H\n")


if __name__ == "__main__":
    main()
