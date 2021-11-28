def to_num(uni):
    """
    Convert a unicode codepoint to a unique integer, which
    is not necessarily the corresponding unicode number.
    """
    assert len(uni) == 1
    b = bytes(uni, 'utf-8')
    num = 0
    for i in range(0, len(b)):
        num |= b[i] << 8*i
    return num


def is_ascii(s):
    """
    Determine if a string contains only ascii characters,
    as opposed to unicode, e.g. 'π' or '×'
    """
    return all(ord(ch) < 128 for ch in s)
