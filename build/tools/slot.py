"""Every knob for the Empty Slot forwarder channel."""

import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILD = os.path.join(ROOT, "build")
OUT = os.path.join(BUILD, "out")
EXTRACTED = os.path.join(BUILD, "extracted")
FORWARDER_DOL = os.path.join(ROOT, "forwarder", "forwarder.dol")

# Only the TMD, ticket and certs are reused: IOS58 with AHB access, no NAND loader.
DONOR = os.path.join(ROOT, "donor", "USBLoaderGX_forwarder[UNEO].wad")
DONOR_URL = ("https://raw.githubusercontent.com/wiidev/usbloadergx/updates/"
             "USBLoaderGX_forwarder%5BUNEO%5D.wad")

# The vWii can't boot the forwarder directly; FIX94's NAND loader boots it from content 2 there.
VWII_LOADER = os.path.join(ROOT, "donor", "vwii-nand-loader.app")
VWII_LOADER_URL = ("https://github.com/FIX94/tiny-vwii-nand-loader/raw/"
                   "2eed1fa0a5c88bda0c96d47cf31fcde88c6c344e/binary/vWii_NAND_Loader_svn_r2.zip")
VWII_LOADER_SHA256 = "69e4d631bee303c549aa813117fee623aa576f0d2693a4ee7c7679bf57993392"

# Any System Menu NAND dump works; Dolphin keeps its contents decrypted.
SYSMENU_CONTENT = os.path.expanduser(
    "~/.local/share/dolphin-emu/Wii/title/00000001/00000002/content")
# The Menu draws an empty grid slot with this layout from chanSel.ash.
SLOT_ARCHIVE = "layout/common/chanSel.ash"
SLOT_LAYOUT = "my_IplTop_b"
SLOT_PANE = "Thu"
NOISE_FRAMES = [f"my_TV_c_p{i}.tpl" for i in range(4)]
SHEEN = "my_TVSpe_a.tpl"
BAND = "my_TV_d.tpl"

# The banner is the icon's layout blown up until its 170x96 picture fills the 16:9 Menu's 836x456.
BANNER_SCALE = (836 / 170, 456 / 96)
# Every banner texture is drawn fresh. Widths that repeat must be powers of two on GX.
BANNER_NOISE_SIZE = (512, 384)
NOISE_GRAIN = 1                  # texels per grain; 4 would be the icon's grain, scaled up
SHEEN_SIZE = (8, 384)
BAND_ROWS = 64
LOGO = os.path.join(ROOT, "assets", "wii-logo.png")
LOGO_WIDTH = 40 / 128            # of the static's width, as in the icon
# The Menu's button panel covers the banner from row 342 down; just below the middle of what's left
# looked right on a TV.
LOGO_CENTER_Y = (342 / 2 + 8) / 456

# Loudness of the static, RMS in dBFS. At -23 it sounded harsh on a TV.
NOISE_DB = -28
NOISE_SECONDS = 4

# The hover balloon shows this; an empty name keeps the slot looking unused.
TITLE = ""
TITLE_ID = "ESLT"                # 00010001 45534C54; not in wiitdb or the installed NAND
BOOT_PATH = "/apps/usbloader_gx/boot.dol"

USERNAME = "cmyksoda"
WAD_NAMES = {"wii": f"Empty Slot - {TITLE_ID} [{USERNAME}].wad",
             "vwii": f"Empty Slot - {TITLE_ID} [{USERNAME}] (vWii).wad"}
