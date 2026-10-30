"""Check the built WAD for the traps that only show up on a console."""

import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))

from lyt import panes
from slot import (BAND, BAND_ROWS, BANNER_NOISE_SIZE, BANNER_SCALE, BOOT_PATH, DONOR, EXTRACTED,
                  NOISE_FRAMES, ROOT, SHEEN, SHEEN_SIZE, SLOT_LAYOUT, SLOT_PANE, TITLE, TITLE_ID,
                  VWII_LOADER_SHA256, WAD_NAMES)
from tpl import I4, I8, NAMES
from wiilib import (CONTENT_RECORD, U8, WAD, imd5_unwrap, imet_titles, imet_verify,
                    unpack_lz77_imd5)

sys.path.insert(0, ROOT)
from retarget import MARKER  # noqa: E402

ICON_CAP = 0x19000      # a bigger icon.bin bricks the Menu after Health & Safety
BANNER_BUDGET = 600_000
IOS = 58
SLOT = os.path.join(EXTRACTED, "slot")

failures = []


def check(ok, msg):
    print(("  ok    " if ok else "  FAIL  ") + msg)
    if not ok:
        failures.append(msg)


def slot_file(name):
    return open(os.path.join(SLOT, name), "rb").read()


def check_container(wad, donor, console):
    tmd = wad.tmd
    check(wad.title_id == bytes.fromhex("00010001") + TITLE_ID.encode(),
          f"title ID is 00010001/{TITLE_ID}")
    check(wad.tik[0x1DC:0x1E4] == wad.title_id, "ticket and TMD title IDs match")
    if console == "vwii":
        check(len(wad.contents) == 3, "three contents: banner, NAND loader, forwarder")
        check(hashlib.sha256(wad.contents[1]).hexdigest() == VWII_LOADER_SHA256,
              "boot content is FIX94's r2 vWii NAND loader")
    else:
        check(len(wad.contents) == 2, "two contents: banner, forwarder")
    check(tmd[0x184:0x18C] == struct.pack(">II", 1, IOS), f"runs on IOS{IOS}")
    check(tmd[0x1D8:0x1DC] == donor.tmd[0x1D8:0x1DC], "access rights kept from the donor")
    check(struct.unpack(">H", tmd[0x1E0:0x1E2])[0] == 1, "boots content 1")
    for i, c in enumerate(wad.contents):
        e = 0x1E4 + i * CONTENT_RECORD
        check(struct.unpack_from(">Q", tmd, e + 8)[0] == len(c), f"content {i} size matches TMD")
        check(tmd[e + 16:e + 36] == hashlib.sha1(c).digest(), f"content {i} SHA-1 matches TMD")
    check(hashlib.sha1(tmd[0x140:]).digest()[0] == 0, "TMD fakesigned")
    check(tmd[0x19A:0x19C] == bytes(2), "TMD 0x19A left zero")
    check(hashlib.sha1(wad.tik[0x140:]).digest()[0] == 0, "ticket fakesigned")
    forwarder = wad.contents[-1]
    check(MARKER + BOOT_PATH.encode() + b"\0" in forwarder, f"forwarder boots {BOOT_PATH}")


def check_layouts(icon, banner):
    brlyt = slot_file(f"{SLOT_LAYOUT}.brlyt")
    brlan = slot_file(f"{SLOT_LAYOUT}.brlan")
    names = open(os.path.join(SLOT, "textures.txt")).read().split()
    check(icon.get("arc/blyt/icon.brlyt") == brlyt, "icon: layout is the Menu's own")
    check(all(icon.get(f"arc/timg/{n}") == slot_file(n) for n in names),
          f"icon: all {len(names)} textures present and unchanged")
    for kind, arc in (("icon", icon), ("banner", banner)):
        check(arc.get(f"arc/anim/{kind}.brlan") == brlan, f"{kind}: animation is the Menu's own")

    layout = banner.get("arc/blyt/banner.brlyt")
    at = next(flags_at - 8 for name, flags_at, _ in panes(layout) if name == SLOT_PANE)
    changed = {i for i in range(len(brlyt)) if layout[i] != brlyt[i]}
    check(len(layout) == len(brlyt) and changed <= set(range(at + 0x3C, at + 0x44)),
          f"banner differs from the slot only in {SLOT_PANE}'s scale")
    check(layout[at + 0x3C:at + 0x44] == struct.pack(">ff", *BANNER_SCALE),
          f"banner scales {SLOT_PANE} by {BANNER_SCALE[0]:.2f} x {BANNER_SCALE[1]:.2f}")
    drawn = {**{n: (BANNER_NOISE_SIZE, I4) for n in NOISE_FRAMES},
             SHEEN: (SHEEN_SIZE, I8), BAND: ((8, BAND_ROWS), I8)}
    for n in names:
        tpl = banner.get(f"arc/timg/{n}")
        (w, h), fmt = drawn[n]
        check(tpl != slot_file(n) and struct.unpack(">HHI", tpl[0x14:0x1C]) == (h, w, fmt),
              f"banner: {n} redrawn at {w}x{h} {NAMES[fmt]}")
    # GX only repeats textures whose size along that axis is a power of two.
    noise_w, band_h = BANNER_NOISE_SIZE[0], BAND_ROWS
    check(noise_w & (noise_w - 1) == 0 and band_h & (band_h - 1) == 0,
          "banner: repeating texture sides are powers of two")


def check_meta(app, donor):
    check(app[:0x40] == donor.contents[0][:0x40], "build tag matches the donor's")
    check(imet_verify(app), "IMET MD5 valid")
    off = app.find(b"IMET")
    sizes = struct.unpack(">III", app[off + 0x0C:off + 0x18])
    meta = U8.load(app[app.find(U8.MAGIC):])

    icon = unpack_lz77_imd5(meta.get("meta/icon.bin"))
    banner = unpack_lz77_imd5(meta.get("meta/banner.bin"))
    for i, (kind, u8) in enumerate((("icon", icon), ("banner", banner))):
        check(sizes[i] == len(u8), f"IMET {kind} size matches ({len(u8):,})")
        check(not U8.load(u8).check_tree(), f"{kind} U8 tree sound")
    check(len(icon) <= ICON_CAP, f"icon.bin {len(icon):,} <= {ICON_CAP:,}")
    check(len(banner) <= BANNER_BUDGET, f"banner.bin {len(banner):,} <= {BANNER_BUDGET:,}")
    check_layouts(U8.load(icon), U8.load(banner))

    bns = imd5_unwrap(meta.get("meta/sound.bin"))
    check(sizes[2] == len(bns), "IMET sound size matches")
    check(bns[:4] == b"BNS " and bns[0x29] == 1, "sound is a looping BNS")
    check(all(name == TITLE for name in imet_titles(app)[1]), f"all 10 names are {TITLE!r}")


def main():
    """verify.py checks both built WADs; verify.py <wad> checks one."""
    paths = sys.argv[1:] or [os.path.join(ROOT, name) for name in WAD_NAMES.values()]
    donor = WAD.load(DONOR)
    for path in paths:
        wad = WAD.load(path)
        console = "vwii" if len(wad.contents) == 3 else "wii"
        print(f"{os.path.basename(path)} ({console}):")
        check_container(wad, donor, console)
        check_meta(wad.contents[0], donor)
    print(f"\n  {'all checks passed' if not failures else f'{len(failures)} FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
