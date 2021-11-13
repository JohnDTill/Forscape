import csv
import os
from collections import namedtuple


def main():
    constructs = []
    with open('construct_codes.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader, None)  # skip the headers
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Construct = namedtuple('Construct', fields)
        for row in reader:
            constructs.append(Construct(*row))

    keywords = []
    with open('typeset_keywords.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        next(reader, None)  # skip the headers
        Shortcut = namedtuple('Shortcut', 'keyword symbol')
        for row in reader:
            keywords.append(Shortcut(row[0], row[1]))

        with open("../src/typeset_keywords.h", "w", encoding="utf-8") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef TYPESET_KEYWORDS_H\n"
                               "#define TYPESET_KEYWORDS_H\n\n")

            codegen_file.write("#include <hope_construct_codes.h>\n"
                               "#include <string_view>\n"
                               "#include <unordered_map>\n"
                               "#include <vector>\n\n")

            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Typeset {\n\n")

            codegen_file.write("class Keywords{\n"
                               "private:\n"
                               "    static const std::unordered_map<std::string_view, std::string> map;\n"
                               "    static const std::string NONE;\n"
                               "\n"
                               "    static const std::string con(size_t code, size_t arity, std::vector<size_t> args){\n"
                               "        std::string str;\n"
                               "        str.reserve(2+arity+args.size());\n"
                               "        str += static_cast<char>(OPEN);\n"
                               "        str += static_cast<char>(code);\n"
                               "        for(const size_t arg : args) str += static_cast<char>(arg);\n"
                               "        for(size_t i = 0; i < arity; i++) str += static_cast<char>(CLOSE);\n"
                               "        return str;\n"
                               "    }\n"
                               "\n"
                               "public:\n"
                               "    static const std::string& lookup(std::string_view key) noexcept{\n"
                               "        auto result = map.find(key);\n"
                               "        return result==map.end() ? NONE : result->second;\n"
                               "    }\n"
                               "};\n\n"
                               )

            codegen_file.write(f"#define HOPE_NUM_KEYWORDS {len(keywords)}\n\n")

            codegen_file.write("#define HOPE_KEYWORD_LIST {\\\n")
            for keyword in keywords:
                codegen_file.write(f"    {{\"{keyword.keyword}\", \"{keyword.symbol}\"}},\\\n")
            codegen_file.write("}\n\n")

            codegen_file.write("}\n\n}\n\n#endif // TYPESET_KEYWORDS_H\n")

        with open("../src/typeset_keywords.cpp", "w", encoding="utf-8") as codegen_file:
            codegen_file.write("#include \"typeset_keywords.h\"\n\n")

            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Typeset {\n\n")

            codegen_file.write("const std::string Keywords::NONE = \"\";\n\n")

            codegen_file.write("const std::unordered_map<std::string_view, std::string> Keywords::map{\n")
            for keyword in keywords:
                codegen_file.write(f"    {{\"{keyword.keyword}\", \"{keyword.symbol}\"}},\n")
            for c in constructs:
                if c.keyword != "":
                    if c.arity == "nxm":
                        n = int(c.default_args[0])
                        m = int(c.default_args[1])
                        arity = n * m
                    elif c.arity == "2xn":
                        arity = 2*int(c.default_args)
                    else:
                        arity = c.arity

                    args = "{"
                    if c.default_args != "":
                        args += c.default_args[0]
                        for i in range(1,len(c.default_args)):
                            args += ", " + c.default_args[i]
                    args += "}"

                    codegen_file.write(f"    {{\"{c.keyword}\", Keywords::con({c.name.upper()}, {arity}, {args})}},\n")
            codegen_file.write("};\n\n}\n\n}\n\n")


if __name__ == "__main__":
    main()
