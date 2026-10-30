"""TPL texture files in the two GX formats the banner uses.

GX stores textures in tiles: I4 in 8x8 tiles at 4 bits per pixel, I8 in 8x4 tiles at a byte per
pixel.
"""

import struct

MAGIC = 0x0020AF30
I4, I8 = 0, 1
NAMES = {I4: "I4", I8: "I8"}
TILE = {I4: (8, 8), I8: (8, 4)}
# A one-texture TPL's pixels start here, in Nintendo's files and ours.
HEADER = 64


def encode(img, fmt):
    w, h = img.size
    tw, th = TILE[fmt]
    px = img.convert("L").load()
    out = bytearray()
    for by in range(0, h, th):
        for bx in range(0, w, tw):
            row = [px[x, y] if x < w and y < h else 0
                   for y in range(by, by + th) for x in range(bx, bx + tw)]
            if fmt == I8:
                out += bytes(row)
            else:
                out += bytes((row[i] & 0xF0) | (row[i + 1] >> 4) for i in range(0, len(row), 2))
    return bytes(out)


def build(img, fmt):
    """One texture, laid out like the stock TPLs: header, texture table, texture header, then the
    pixels."""
    w, h = img.size
    table, texture_header = 12, 20
    out = struct.pack(">III", MAGIC, 1, table) + struct.pack(">II", texture_header, 0)
    # wrapS/T=0 (clamp), min/mag filter=1 (linear), lodbias 0, unpacked 0
    out += struct.pack(">HHIIIIIIfBBBB", h, w, fmt, HEADER, 0, 0, 1, 1, 0.0, 0, 0, 0, 0)
    return out.ljust(HEADER, b"\0") + encode(img, fmt)
