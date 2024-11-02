import os
from utils import cpp, table_reader
import glob
from itertools import product


def hash(keyword, w):
    keyword = bytes(keyword, 'utf-8')
    sze = len(keyword)
    c0 = int(keyword[0])
    cN = int(keyword[-1])
    return ((sze << w[0]) % 256) ^ ((c0 << w[1]) % 256) ^ ((cN << w[2]) % 256)


def hash_str(w):
    ans = f"(sze << {w[0]})" if w[0] else "sze"
    ans += " ^ "
    ans += f"(c0 << {w[1]})" if w[1] else "c0"
    ans += " ^ "
    ans += f"(cN << {w[2]})" if w[2] else "cN"

    return ans


def find_keyword_hash_weights(keywords):
    for w0, w1, w2 in product(range(8), range(8), range(8)):
        hashes = {hash(keyword, [w0, w1, w2]) for keyword in keywords}
        if len(hashes) == len(keywords):
            return [w0, w1, w2]
    assert False, "Failed to find perfect hash function for encoding keywords"


def main():
    constructs = table_reader.csv_to_list_of_tuples(
        csv_filepath="construct_codes.csv",
        tuple_name="Construct",
    )

    header_writer = cpp.HeaderWriter(
        name="construct_codes",
        includes=["cinttypes", "string_view"]
    )

    hash_weights = find_keyword_hash_weights([con.encode_word for con in constructs if con.encode_word])

    header_writer.write(f'#define CONSTRUCT_STR "⁜"\n'
                        f'#define OPEN_STR "⏴"\n'
                        f'#define CLOSE_STR "⏵"\n'
                        f'inline constexpr char CONSTRUCT_STRVIEW[] = CONSTRUCT_STR;\n'
                        f'inline constexpr auto CON_0 = CONSTRUCT_STRVIEW[0];\n'
                        f'inline constexpr auto CON_1 = CONSTRUCT_STRVIEW[1];\n'
                        f'inline constexpr auto CON_2 = CONSTRUCT_STRVIEW[2];\n'
                        f'inline constexpr char OPEN_STRVIEW[] = OPEN_STR;\n'
                        f'inline constexpr auto OPEN_0 = OPEN_STRVIEW[0];\n'
                        f'inline constexpr auto OPEN_1 = OPEN_STRVIEW[1];\n'
                        f'inline constexpr auto OPEN_2 = OPEN_STRVIEW[2];\n'
                        f'inline constexpr char CLOSE_STRVIEW[] = CLOSE_STR;\n'
                        f'inline constexpr auto CLOSE_0 = CLOSE_STRVIEW[0];\n'
                        f'inline constexpr auto CLOSE_1 = CLOSE_STRVIEW[1];\n'
                        f'inline constexpr auto CLOSE_2 = CLOSE_STRVIEW[2];\n'
                        "\n")

    header_writer.write(
        "#define DECLARE_CONSTRUCT_CSTRINGS"
    )
    for con in constructs:
        if con.encode_word:
            header_writer.write(f"\\\n    inline constexpr char {con.name.upper()}_CSTRING[] = \"{con.encode_word}\";")
    header_writer.write("\n\n")

    header_writer.write(
        f"inline constexpr uint8_t encodingHash(std::string_view keyword) noexcept {{\n"
        f"    const uint8_t sze = static_cast<uint8_t>(keyword.size());\n"
        f"    const uint8_t c0 = keyword[0];\n"
        f"    const uint8_t cN = keyword[sze-1];\n"
        f"    return {hash_str(hash_weights)};\n"
        "}\n\n"
    )

    curr = 1
    for con in constructs:
        header_writer.write(f"inline constexpr char {con.name.upper()} = {curr};\n")
        header_writer.write(f'#define {con.name.upper()}_STR "{con.encode_word}"\n')
        curr += 1
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_NULLARY_CASES")
    for encode_word in [entry.encode_word for entry in constructs if entry.arity == "0" and entry.encode_word]:
        header_writer.write(
            f" \\\n    case encodingHash(\"{encode_word}\"): if(keyword != \"{encode_word}\") return false; break;"
        )
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_UNARY_CASES")
    for encode_word in [entry.encode_word for entry in constructs if entry.arity == "1"]:
        header_writer.write(
            f" \\\n    case encodingHash(\"{encode_word}\"): if(keyword != \"{encode_word}\") return false;"
            " depth += 1; break;")
    header_writer.write("\n")

    header_writer.write("#define FORSCAPE_SERIAL_BINARY_CASES")
    for encode_word in [entry.encode_word for entry in constructs if entry.arity == "2"]:
        header_writer.write(
            f" \\\n    case encodingHash(\"{encode_word}\"): if(keyword != \"{encode_word}\") return false;"
            " depth += 2; break;")
    header_writer.write("\n")

    header_writer.write("\n")
    header_writer.write("#define FORSCAPE_TYPESET_PARSER_CASES")
    for entry in constructs:
        if not entry.arity or not entry.encode_word:
            continue
        name = entry.name
        word = entry.encode_word
        header_writer.write(f" \\\n    case encodingHash(\"{word}\"): assert(keyword == \"{word}\"); TypesetSetup")
        if entry.arity == "0":
            header_writer.write(f"Nullary({name});")
        else:
            header_writer.write(f"Construct({name},);")
    header_writer.write("\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
