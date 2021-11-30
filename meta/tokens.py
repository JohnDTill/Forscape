from utils import cpp, table_reader, unicode


def main():
    constructs = table_reader.csv_to_list_of_tuples(
        csv_filepath="construct_codes.csv",
        tuple_name="Construct",
    )

    tokens = table_reader.csv_to_list_of_tuples(
        csv_filepath="tokens.csv",
        tuple_name="Token",
        delimiter='9',
    )

    header_writer = cpp.HeaderWriter(
        name="tokentype",
        inner_namespace="Code",
    )

    header_writer.write("enum TokenType {\n")
    for token in tokens:
        header_writer.write(f"    {token.enum.upper()},\n")
    for construct in constructs:
        header_writer.write(f"    TOKEN_{construct.name.upper()},\n")
    header_writer.write("};\n\n")

    header_writer.write("#define HOPE_SCANNER_CASES \\\n")
    header_writer.write("    case '\"': return scanString();\\\n")
    for token in [token for token in tokens if token.simple == 'y' and unicode.is_ascii(token.label)]:
        header_writer.write(f"    case '{token.label}':")
        if token.group == "open":
            header_writer.write(" incrementScope();")
        elif token.group == "close":
            header_writer.write(" decrementScope();")
        header_writer.write(f" return createToken({token.enum.upper()});\\\n")
    for token in [token for token in tokens if token.simple == 'y' and not unicode.is_ascii(token.label)]:
        header_writer.write(f"    case {unicode.to_num(token.label)}:")
        if token.group == "open":
            header_writer.write(" incrementScope();")
        elif token.group == "close":
            header_writer.write(" decrementScope();")
        header_writer.write(f" return createToken({token.enum.upper()});\\\n")
    for i in range(0, 10):
        header_writer.write(f"    case '{i}': return scanNumber();\\\n")
    for i in range(ord('a'), ord('z') + 1):
        header_writer.write(f"    case '{chr(i)}': return scanIdentifier();\\\n")
    for i in range(ord('A'), ord('Z') + 1):
        header_writer.write(f"    case '{chr(i)}': return scanIdentifier();\\\n")
    header_writer.write(f"    case '_': return scanIdentifier();\\\n")
    for i in range(945, 970):  # α-ω
        ch = chr(i)
        header_writer.write(
            f"    case {unicode.to_num(ch)}: controller->formatBasicIdentifier(); return createToken(IDENTIFIER);\\\n")
    for i in range(913, 938):  # Α-Ω
        ch = chr(i)
        header_writer.write(
            f"    case {unicode.to_num(ch)}: controller->formatBasicIdentifier(); return createToken(IDENTIFIER);\\\n")
    for con in constructs:
        header_writer.write(
            f"    case (1 << 7) | {con.name.upper()}: return scanConstruct(TOKEN_{con.name.upper()});\\\n")
    header_writer.write("    case '/': return forwardSlash();\n")
    header_writer.write("\n")

    header_writer.write("#define HOPE_IMPLICIT_MULT_TOKENS ")
    for token in tokens:
        if token.imp_mult == "y":
            header_writer.write(f"\\\n    case {token.enum.upper()}:")
    for construct in constructs:
        if construct.imp_mult == "y":
            header_writer.write(f"\\\n    case TOKEN_{construct.name.upper()}:")
    header_writer.write("\n\n")

    header_writer.write("#define HOPE_KEYWORD_MAP ")
    for token in tokens:
        if token.keyword == "y":
            header_writer.write(f"\\\n    {{\"{token.label}\", {token.enum.upper()}}},")
    header_writer.write("\n\n")

    header_writer.finalize()

    header_writer = cpp.HeaderWriter(
        name="closesymbol",
        inner_namespace="Typeset",
        includes=["unordered_set"],
    )

    header_writer.write("class CloseSymbol{\n"
                        "private:\n"
                        "    static const std::unordered_set<char> close_symbols;\n"
                        "\n"
                        "public:\n"
                        "    static bool isClosing(char ch) noexcept{\n"
                        "        return close_symbols.find(ch) != close_symbols.end();\n"
                        "    }\n"
                        "};\n\n")

    header_writer.write("const std::unordered_set<char> CloseSymbol::close_symbols{\n")
    for token in tokens:
        if token.simple == "y" and token.group == "close" and unicode.is_ascii(token.label):
            header_writer.write(f"    '{token.label}',\n")
    header_writer.write("};\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
