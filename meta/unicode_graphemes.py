from utils import cpp, unicode
from wcwidth import wcwidth


def main():
    header_writer = cpp.HeaderWriter(
        name="unicode_zerowidth",
        includes=["unordered_set"],
    )

    header_writer.write("static const std::unordered_set<uint32_t> ZERO_WIDTH_CHARS = {\n")
    for i in range(256, 1000000):
        ch = chr(i)
        if wcwidth(ch) == 0:
            header_writer.write(f"    {unicode.to_num(ch)},\n")
    header_writer.write("};\n\n")
    header_writer.write("inline bool isZeroWidth(uint32_t code){"
                        "return ZERO_WIDTH_CHARS.find(code) != ZERO_WIDTH_CHARS.end();"
                        "}\n\n")

    header_writer.finalize()


# if __name__ == "__main__":
#     main()
