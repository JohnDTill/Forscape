import csv
import os
from collections import namedtuple


def is_ascii(s):
    for ch in s:
        if ord(ch) >= 128:
            return False
    return True


def unicode_to_num(uni):
    b = bytes(uni, 'utf-8')
    num = 0
    for i in range(0, len(b)):
        num |= b[i] << 8*i
    return num


def main():
    constructs = []
    with open('construct_codes.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Construct = namedtuple('Construct', fields)
        for row in reader:
            constructs.append(Construct(*row))

    with open('tokens.csv', encoding="utf-8") as csvfile:
        reader = csv.reader(csvfile, delimiter='9')
        headers = next(reader, None)
        fields = headers[0]
        for i in range(1, len(headers)):
            fields += ' ' + headers[i]
        Token = namedtuple('Token', fields)
        tokens = []
        for row in reader:
            tokens.append(Token(*row))

        with open("../src/hope_tokentype.h", "w") as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef HOPE_TOKENTYPE_H\n"
                               "#define HOPE_TOKENTYPE_H\n\n")
            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Code {\n\n")

            codegen_file.write("enum TokenType {\n")
            for token in tokens:
                codegen_file.write(f"    {token.enum.upper()},\n")
            for construct in constructs:
                codegen_file.write(f"    TOKEN_{construct.name.upper()},\n")
            codegen_file.write("};\n\n")

            codegen_file.write("#define HOPE_SCANNER_CASES \\\n")
            codegen_file.write("    case '\"': return scanString();\\\n")
            for token in [token for token in tokens if token.simple == 'y' and is_ascii(token.label)]:
                codegen_file.write(f"    case '{token.label}':")
                if token.group == "open":
                    codegen_file.write(" incrementScope();")
                elif token.group == "close":
                    codegen_file.write(" decrementScope();")
                codegen_file.write(f" return createToken({token.enum.upper()});\\\n")
            for token in [token for token in tokens if token.simple == 'y' and not is_ascii(token.label)]:
                codegen_file.write(f"    case {unicode_to_num(token.label)}:")
                if token.group == "open":
                    codegen_file.write(" incrementScope();")
                elif token.group == "close":
                    codegen_file.write(" decrementScope();")
                codegen_file.write(f" return createToken({token.enum.upper()});\\\n")
            for i in range(0, 10):
                codegen_file.write(f"    case '{i}': return scanNumber();\\\n")
            for i in range(ord('a'), ord('z')+1):
                codegen_file.write(f"    case '{chr(i)}': return scanIdentifier();\\\n")
            for i in range(ord('A'), ord('Z')+1):
                codegen_file.write(f"    case '{chr(i)}': return scanIdentifier();\\\n")
            codegen_file.write(f"    case '_': return scanIdentifier();\\\n")
            for i in range(945, 970):  # α-ω
                ch = chr(i)
                codegen_file.write(f"    case {unicode_to_num(ch)}: controller->formatBasicIdentifier(); return createToken(IDENTIFIER);\\\n")
            for i in range(913, 938):  # Α-Ω
                ch = chr(i)
                codegen_file.write(f"    case {unicode_to_num(ch)}: controller->formatBasicIdentifier(); return createToken(IDENTIFIER);\\\n")
            for con in constructs:
                codegen_file.write(f"    case (1 << 7) | {con.name.upper()}: return scanConstruct(TOKEN_{con.name.upper()});\\\n")
            codegen_file.write("    case '/': return forwardSlash();\n")
            codegen_file.write("\n")

            codegen_file.write("#define HOPE_IMPLICIT_MULT_TOKENS ")
            for token in tokens:
                if token.imp_mult == "y":
                    codegen_file.write(f"\\\n    case {token.enum.upper()}:")
            for construct in constructs:
                if construct.imp_mult == "y":
                    codegen_file.write(f"\\\n    case TOKEN_{construct.name.upper()}:")
            codegen_file.write("\n\n")

            codegen_file.write("#define HOPE_KEYWORD_MAP ")
            for token in tokens:
                if token.keyword == "y":
                    codegen_file.write(f"\\\n    {{\"{token.label}\", {token.enum.upper()}}},")
            codegen_file.write("\n\n")

            codegen_file.write("}\n\n}\n\n#endif // TOKENTYPE_H\n")

        with open("../src/typeset_closesymbol.h", "w", encoding='utf-8') as codegen_file:
            codegen_file.write("//CODEGEN FILE\n\n"
                               "#ifndef TYPESET_CLOSESYMBOL_H\n"
                               "#define TYPESET_CLOSESYMBOL_H\n\n")

            codegen_file.write("#include <unordered_set>\n\n")

            codegen_file.write("namespace Hope {\n\n")
            codegen_file.write("namespace Typeset {\n\n")

            codegen_file.write("class CloseSymbol{\n"
                               "private:\n"
                               "    static const std::unordered_set<char> close_symbols;\n"
                               "\n"
                               "public:\n"
                               "    static bool isClosing(char ch) noexcept{\n"
                               "        return close_symbols.find(ch) != close_symbols.end();\n"
                               "    }\n"
                               "};\n\n")

            codegen_file.write("const std::unordered_set<char> CloseSymbol::close_symbols{\n")
            for token in tokens:
                if token.simple == "y" and token.group == "close" and is_ascii(token.label):
                    codegen_file.write(f"    '{token.label}',\n")
            codegen_file.write("};\n\n")

            codegen_file.write("}\n\n}\n\n#endif // TYPESET_CLOSESYMBOL_H\n")


if __name__ == "__main__":
    main()
