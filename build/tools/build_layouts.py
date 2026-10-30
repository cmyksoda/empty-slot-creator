"""Build icon.bin and banner.bin from the System Menu's own empty-slot layout.

The icon is Nintendo's layout byte for byte, so the Menu draws it exactly as it
draws an unused slot. The banner is the same layout scaled up to fill the
screen, with every texture drawn fresh at banner size: static in each frame's
own grey levels around the official Wii logo, and the icon's sheen and scanline
gradients resampled smoothly.
"""

import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))

from PIL import Image, ImageFilter

import tpl
from lyt import panes
from slot import (BAND, BAND_ROWS, BANNER_NOISE_SIZE, BANNER_SCALE, EXTRACTED, LOGO,
                  LOGO_CENTER_Y, LOGO_WIDTH, NOISE_FRAMES, NOISE_GRAIN, OUT, SHEEN, SHEEN_SIZE,
                  SLOT_LAYOUT, SLOT_PANE)
from wiilib import U8, U8Node, pack_lz77_imd5

SLOT = os.path.join(EXTRACTED, "slot")
LEVEL = 17                       # GX expands a 4-bit intensity n to n * 17


def layout_archive(kind, brlyt, brlan, textures):
    """The usual banner tree: arc/anim, arc/blyt, arc/timg."""
    count = 7 + len(textures)
    nodes = [
        U8Node(True, "", parent=0, last=count),
        U8Node(True, "arc", parent=0, last=count),
        U8Node(True, "anim", parent=1, last=4),
        U8Node(False, f"{kind}.brlan", brlan),
        U8Node(True, "blyt", parent=1, last=6),
        U8Node(False, f"{kind}.brlyt", brlyt),
        U8Node(True, "timg", parent=1, last=count),
    ]
    nodes += [U8Node(False, name, data) for name, data in textures]
    arc = U8(nodes, data_align=64, tail_align=32)
    problems = arc.check_tree()
    assert not problems, problems
    return arc.to_bytes()


def tpl_size(data):
    h, w = struct.unpack(">HH", data[0x14:0x18])
    return w, h


def ia4_levels(data):
    """Intensity nibbles of a single-texture IA4 TPL, row-major."""
    w, h = tpl_size(data)
    out = [0] * (w * h)
    i = tpl.HEADER
    for ty in range(0, h, 4):
        for tx in range(0, w, 8):
            for y in range(4):
                for x in range(8):
                    out[(ty + y) * w + tx + x] = data[i] & 0x0F
                    i += 1
    return out


def i4_column(data):
    """The first column of a single-texture I4 TPL, as 0-255 values."""
    w, h = tpl_size(data)
    tile_bytes, tiles_across = 32, (w + 7) // 8
    return [(data[tpl.HEADER + (y // 8) * tiles_across * tile_bytes + (y % 8) * 4] >> 4) * LEVEL
            for y in range(h)]


def logo_mask(frames, w, h):
    """The Wii logo is the one spot that sits at each frame's darkest level in all four."""
    lows = [min(levels) for levels in frames]
    hit = [all(f[i] == low for f, low in zip(frames, lows)) for i in range(w * h)]
    mask = Image.new("L", (w, h))
    mask.putdata([255 if v else 0 for v in hit])
    return mask.filter(ImageFilter.MedianFilter(3)), lows


def logo_coverage(size):
    """The official logo as coverage. Banner texels aren't square, so it's drawn squeezed by the
    texel shape to come out true on screen."""
    w, h = size
    texel_w = 128 * BANNER_SCALE[0] / w
    texel_h = 96 * BANNER_SCALE[1] / h
    logo = Image.open(LOGO)
    lw = round(w * LOGO_WIDTH)
    lh = round(lw * logo.height / logo.width * texel_w / texel_h)
    canvas = Image.new("L", size)
    top = round(h * LOGO_CENTER_Y - lh / 2)
    canvas.paste(logo.resize((lw, lh), Image.LANCZOS), ((w - lw) // 2, top))
    return canvas


def banner_static(frames, icon_w, icon_h, seed):
    """Fresh static for each icon frame, its grains drawn from that frame's own grey levels,
    with the logo at the frame's darkest level as in the icon."""
    mask, lows = logo_mask(frames, icon_w, icon_h)
    w, h = BANNER_NOISE_SIZE
    coverage = logo_coverage((w, h))
    grains = (w // NOISE_GRAIN, h // NOISE_GRAIN)
    rng = random.Random(seed)
    out = []
    for levels, low in zip(frames, lows):
        pool = [v for v, m in zip(levels, mask.tobytes()) if not m]
        grain = Image.new("L", grains)
        grain.putdata([rng.choice(pool) * LEVEL for _ in range(grains[0] * grains[1])])
        noise = grain.resize((w, h), Image.NEAREST)
        frame = Image.composite(Image.new("L", (w, h), low * LEVEL), noise, coverage)
        out.append(frame.point(lambda v: round(v / LEVEL) * LEVEL))
    return out


def banner_sheen(data):
    """The icon's glass sheen, a vertical I8 gradient, resampled smoothly to banner height."""
    size = tpl_size(data)
    sheen = Image.frombytes("L", size, data[tpl.HEADER:tpl.HEADER + size[0] * size[1]])
    return sheen.resize(SHEEN_SIZE, Image.BICUBIC)


def banner_band(data):
    """The icon's scanline band, resampled with wraparound the way the GPU filters it."""
    old = i4_column(data)
    scale = BAND_ROWS / len(old)
    column = []
    for y in range(BAND_ROWS):
        u = (y + 0.5) / scale - 0.5
        i, f = math.floor(u), u - math.floor(u)
        column.append(round(old[i % len(old)] * (1 - f) + old[(i + 1) % len(old)] * f))
    band = Image.new("L", (8, BAND_ROWS))
    band.putdata([v for v in column for _ in range(8)])
    return band


def read(name):
    return open(os.path.join(SLOT, name), "rb").read()


def banner_layout(brlyt):
    out = bytearray(brlyt)
    for name, flags_at, _ in panes(brlyt):
        if name == SLOT_PANE:
            struct.pack_into(">ff", out, flags_at - 8 + 0x3C, *BANNER_SCALE)
            return bytes(out)
    raise SystemExit(f"{SLOT_PANE} not in {SLOT_LAYOUT}")


def banner_textures(icon_textures, seed=0):
    icon = dict(icon_textures)
    frames = [ia4_levels(icon[name]) for name in NOISE_FRAMES]
    static = banner_static(frames, *tpl_size(icon[NOISE_FRAMES[0]]), seed)
    drawn = {name: tpl.build(img, tpl.I4) for name, img in zip(NOISE_FRAMES, static)}
    drawn[SHEEN] = tpl.build(banner_sheen(icon[SHEEN]), tpl.I8)
    drawn[BAND] = tpl.build(banner_band(icon[BAND]), tpl.I8)
    assert set(drawn) == set(icon), set(icon) ^ set(drawn)
    return [(name, drawn[name]) for name, _ in icon_textures], static[0]


def main():
    brlyt = read(f"{SLOT_LAYOUT}.brlyt")
    brlan = read(f"{SLOT_LAYOUT}.brlan")
    names = open(os.path.join(SLOT, "textures.txt")).read().split()
    icon_textures = [(name, read(name)) for name in names]
    textures, preview = banner_textures(icon_textures)

    layouts = {
        "icon": (brlyt, icon_textures),
        "banner": (banner_layout(brlyt), textures),
    }
    os.makedirs(OUT, exist_ok=True)
    for kind, (layout, kind_textures) in layouts.items():
        u8 = layout_archive(kind, layout, brlan, kind_textures)
        packed = pack_lz77_imd5(u8)
        open(os.path.join(OUT, f"{kind}.bin"), "wb").write(packed)
        print(f"  {kind}.bin: {len(u8):,} bytes uncompressed, {len(packed):,} stored")
    preview.save(os.path.join(OUT, "banner_frame0.png"))


if __name__ == "__main__":
    main()
