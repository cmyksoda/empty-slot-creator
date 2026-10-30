"""Just enough BRLYT/BRLAN reading to move a Nintendo layout between archives."""

import struct

PANE_KINDS = (b"pan1", b"pic1", b"txt1", b"wnd1", b"bnd1")


def sections(data):
    offset, count = struct.unpack(">HH", data[0x0C:0x10])
    for _ in range(count):
        magic = data[offset:offset + 4]
        size = struct.unpack(">I", data[offset + 4:offset + 8])[0]
        yield magic, offset, size
        offset += size


def _string_table(data, base, count, stride):
    names = []
    for i in range(count):
        off = struct.unpack(">I", data[base + i * stride:base + i * stride + 4])[0]
        names.append(data[base + off:data.index(b"\0", base + off)].decode("ascii"))
    return names


def layout_textures(brlyt):
    for magic, off, _ in sections(brlyt):
        if magic == b"txl1":
            count = struct.unpack(">H", brlyt[off + 8:off + 10])[0]
            return _string_table(brlyt, off + 12, count, 8)
    return []


def animation_textures(brlan):
    for magic, off, _ in sections(brlan):
        if magic == b"pai1":
            count = struct.unpack(">H", brlan[off + 12:off + 14])[0]
            return _string_table(brlan, off + 20, count, 4)
    return []


def panes(brlyt):
    """Yield (name, flags offset, flags) for every pane."""
    for magic, off, _ in sections(brlyt):
        if magic in PANE_KINDS:
            name = brlyt[off + 12:off + 28].split(b"\0")[0].decode("ascii")
            yield name, off + 8, brlyt[off + 8]
