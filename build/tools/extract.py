"""Pull the empty-slot layout out of a System Menu, and fetch the donor WAD and vWii loader."""

import glob
import hashlib
import io
import os
import struct
import sys
import urllib.request
import zipfile

sys.path.insert(0, os.path.dirname(__file__))

from libWiiPy.archive.ash import decompress_ash

from lyt import animation_textures, layout_textures
from slot import (DONOR, DONOR_URL, EXTRACTED, SLOT_ARCHIVE, SLOT_LAYOUT, SYSMENU_CONTENT,
                  VWII_LOADER, VWII_LOADER_SHA256, VWII_LOADER_URL)
from wiilib import U8


def find_slot_archive(content_dir):
    for path in sorted(glob.glob(os.path.join(content_dir, "*.app"))):
        data = open(path, "rb").read()
        if data[:4] != U8.MAGIC:
            continue
        try:
            return path, U8.load(data).get(SLOT_ARCHIVE)
        except KeyError:
            continue
    raise SystemExit(f"no content in {content_dir} holds {SLOT_ARCHIVE}")


def menu_version(content_dir):
    tmd = open(os.path.join(content_dir, "title.tmd"), "rb").read()
    return struct.unpack(">H", tmd[0x1DC:0x1DE])[0]


def extract_slot(content_dir):
    path, packed = find_slot_archive(content_dir)
    arc = U8.load(decompress_ash(packed))
    brlyt = arc.get(f"arc/blyt/{SLOT_LAYOUT}.brlyt")
    brlan = arc.get(f"arc/anim/{SLOT_LAYOUT}.brlan")
    textures = sorted(set(layout_textures(brlyt)) | set(animation_textures(brlan)))

    out = os.path.join(EXTRACTED, "slot")
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, f"{SLOT_LAYOUT}.brlyt"), "wb").write(brlyt)
    open(os.path.join(out, f"{SLOT_LAYOUT}.brlan"), "wb").write(brlan)
    for name in textures:
        open(os.path.join(out, name), "wb").write(arc.get(f"arc/timg/{name}"))
    open(os.path.join(out, "textures.txt"), "w").write("\n".join(textures) + "\n")
    print(f"  System Menu v{menu_version(content_dir)}: {os.path.basename(path)} "
          f"-> {SLOT_LAYOUT} with {', '.join(textures)}")


def fetch_donor():
    if not os.path.exists(DONOR):
        os.makedirs(os.path.dirname(DONOR), exist_ok=True)
        urllib.request.urlretrieve(DONOR_URL, DONOR)
    print(f"  donor: {os.path.basename(DONOR)}")


def fetch_vwii_loader():
    if not os.path.exists(VWII_LOADER):
        with urllib.request.urlopen(VWII_LOADER_URL) as r:
            loader = zipfile.ZipFile(io.BytesIO(r.read())).read("00000001.app")
        if hashlib.sha256(loader).hexdigest() != VWII_LOADER_SHA256:
            raise SystemExit("the vWii NAND loader download isn't FIX94's r2 build")
        open(VWII_LOADER, "wb").write(loader)
    print(f"  vWii loader: {os.path.basename(VWII_LOADER)}")


def main():
    content_dir = sys.argv[1] if len(sys.argv) > 1 else SYSMENU_CONTENT
    os.makedirs(EXTRACTED, exist_ok=True)
    extract_slot(content_dir)
    fetch_donor()
    fetch_vwii_loader()


if __name__ == "__main__":
    main()
