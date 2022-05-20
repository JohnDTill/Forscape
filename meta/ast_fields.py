from utils import cpp, table_reader


def to_camel_case(snake_str):
    components = snake_str.split('_')
    return components[0] + ''.join(x.title() for x in components[1:])


def main():
    fields = table_reader.csv_to_list_of_tuples(
        csv_filepath="ast_fields.csv",
    )

    header_writer = cpp.HeaderWriter(
        name="ast_fields",
        inner_namespace="Code",
        includes=["hope_value.h"]
    )

    nodes = table_reader.csv_to_list_of_tuples(
        csv_filepath="parse_nodes.csv",
        tuple_name="Node",
    )

    header_writer.write("#define HOPE_AST_FIELD_CODEGEN_DECLARATIONS \\\n")
    prev = 0

    with open("../src/generated/code_ast_fields.cpp", "w", encoding="utf-8") as source_file:
        source_file.write("#include \"hope_parse_tree.h\"\n\n")
        source_file.write("#include \"typeset_selection.h\"\n\n")

        source_file.write("namespace Hope {\n\n")
        source_file.write("namespace Code {\n\n")

        for field in fields:
            word = (field.word == "y")
            header_writer.write(f"    static constexpr size_t {field.property.upper()}_OFFSET = {prev}; \\\n")
            if word:
                prev = f"{field.property.upper()}_OFFSET + 1"
                T = "size_t"
                ref = f"data[pn+{field.property.upper()}_OFFSET]"
                const_ref = f"data[pn+{field.property.upper()}_OFFSET]"
            else:
                prev = f"{field.property.upper()}_OFFSET + sizeof({field.type})/sizeof(size_t) + (sizeof({field.type}) % sizeof(size_t) != 0)"
                T = f"const {field.type}&"
                ref = f"*reinterpret_cast<{field.type}*>(data.data()+pn+{field.property.upper()}_OFFSET)"
                const_ref = f"*reinterpret_cast<const {field.type}*>(data.data()+pn+{field.property.upper()}_OFFSET)"
            getter = to_camel_case("get_" + field.property)
            setter = to_camel_case("set_" + field.property)
            header_writer.write(f"    {T} {getter}(ParseNode pn) const noexcept; \\\n")
            header_writer.write(f"    void {setter}(ParseNode pn, {T} {field.property}) noexcept;  \\\n")
            source_file.write(
                f"{T} ParseTree::{getter}(ParseNode pn) const noexcept {{\n"
                "    assert(isNode(pn));\n"
                f"    return {const_ref};\n"
                "}\n\n"
            )
            source_file.write(
                f"void ParseTree::{setter}(ParseNode pn, {T} {field.property}) noexcept {{\n"
                "    assert(isNode(pn));\n"
                f"    {ref} = {field.property};\n"
                "}\n\n"
            )

        for node in [node for node in nodes if node.ast_flag]:
            getter = to_camel_case("get_" + node.ast_flag)
            setter = to_camel_case("set_" + node.ast_flag)
            header_writer.write(f"    size_t {getter}(ParseNode pn) const noexcept; \\\n")
            header_writer.write(f"    void {setter}(ParseNode pn, size_t value) noexcept;  \\\n")
            source_file.write(
                f"size_t ParseTree::{getter}(ParseNode pn) const noexcept {{\n"
                f"    assert(getOp(pn) == OP_{node.enum});\n"
                f"    return getFlag(pn);\n"
                "}\n\n"
            )
            source_file.write(
                f"void ParseTree::{setter}(ParseNode pn, size_t value) noexcept {{\n"
                f"    assert(getOp(pn) == OP_{node.enum});\n"
                f"    setFlag(pn, value);\n"
                "}\n\n"
            )

        header_writer.write(f"    static constexpr size_t FIXED_FIELDS = {prev};\n\n")
        source_file.write("}\n\n}\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
