from utils import cpp, unicode
from wcwidth import wcwidth


def main():
    header_writer = cpp.HeaderWriter(
        name="unicode_zerowidth",
        includes=["hope_common.h"],
    )

    header_writer.write(
        "inline bool isZeroWidth(uint32_t code) noexcept {\n"
        "    switch(code){\n")
    for i in range(256, 1114112):
        ch = chr(i)
        if wcwidth(ch) == 0:
            print(f"{i},")
            header_writer.write(f"        case {unicode.to_num(ch)}:\n")
    header_writer.write(
        "            return true;\n"
        "        default: return false;\n"
        "    }\n"
        "};\n\n")

    header_writer.finalize()


if __name__ == "__main__":
    main()
