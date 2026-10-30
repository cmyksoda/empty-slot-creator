# Building an Empty Slot channel from scratch

This walks through every piece of the channel, and how `build/build.sh` makes
each one. Follow it to rebuild ours, or as a recipe for your own
"looks like something else" channel.

## What you need

- **devkitPro** with devkitPPC, libogc and libfat
  ([install guide](https://devkitpro.org/wiki/Getting_Started)). On Windows,
  the devkitPro installer with "Wii development" ticked. Then
  `pacman -S wii-dev` if anything is missing.
- **Python 3.** `build.sh` makes its own virtual environment and installs
  Pillow, pycryptodome and libWiiPy into it.
- **A System Menu dump.** The icon is Nintendo's own empty-slot artwork, which
  we can't redistribute separately, so the build reads it from your System
  Menu. Dolphin's NAND works (Dolphin → Tools → Perform Online System Update,
  or import a NAND backup). So does a BootMii NAND backup extracted with a NAND
  tool. You need the folder holding the System Menu's decrypted `.app` files and
  `title.tmd`: in Dolphin that's
  `Wii/title/00000001/00000002/content`.

On Linux or macOS run `build/build.sh [content folder]`. On Windows, run it from
the devkitPro MSYS2 shell. The steps below are what it does.

## 1. The forwarder (`forwarder/`)

A channel's boot content is a DOL that the Wii runs straight from NAND. Ours is
tiny:

1. `fatMountSimple("sd", …)`, then look for the path stored after `FWDPATH=`.
   If it's not there, mount USB (retrying while the drive spins up) and look
   there.
2. Read the file into MEM2 and work out where its sections go (DOL header or
   ELF program headers).
3. Copy a small position-independent stub to `0x93000000`, shut down
   SD/USB/libogc, and jump to the stub. The stub copies the app's sections into
   place, writes the `argv` block (the app's path), flushes caches and jumps to
   the app's entry point.

The stub in step 3 has to live in MEM2 because the app usually loads exactly
where the forwarder is running (`0x80004000`).

**The trap:** when the Wii boots a channel from NAND,
the CPU starts at `0x80003400` in real mode, not at the DOL's entry point.
Older libogc put a startup stub there; current libogc doesn't. Without one the
channel shows a black screen forever. `stub.S` is that stub: it sets HID0, maps
MEM1 and MEM2 with BATs, turns address translation on and jumps to
`__app_start`. The Makefile pins it with `-Wl,--section-start,.stub=0x80003400`.
If you build a channel DOL with current devkitPPC, you need the same.

Build: `make -C forwarder` → `forwarder/forwarder.dol`.

## 2. The icon (`build/tools/extract.py`, `build_layouts.py`)

The Wii Menu draws an empty grid slot with the layout `my_IplTop_b` from
`layout/common/chanSel.ash` (ASH-compressed U8) inside one of its contents.
`extract.py` finds it and saves the `.brlyt`, the `.brlan` and its six textures:
four static frames (`my_TV_c_p0`–`p3`, 128×96 IA4), the rolling band
(`my_TV_d`) and the glass sheen (`my_TVSpe_a`).

A channel icon is a U8 archive `arc/{anim,blyt,timg}` holding
`icon.brlan`, `icon.brlyt` and the textures, LZ77-compressed and wrapped in an
IMD5 header. Ours uses Nintendo's layout byte for byte, so the Menu draws it
exactly like a real empty slot. Keep `icon.bin` under 100 KiB uncompressed;
bigger icons can brick the Menu.

## 3. The banner (`build_layouts.py`)

The banner reuses the same layout with one change: the `Thu` pane (the icon's
whole picture) is scaled ×4.92 across and ×4.75 down, so its 170×96 picture
fills the Wii Menu's 836×456 in 16:9 (a 4:3 TV shows the middle 608). The
animation is unchanged, so the static flickers and the scanlines roll just as on
the icon.

Nintendo's material does the look on the GPU, so it helps to know what it adds
up: each static texel maps to grey 190–205, two scrolling copies of `my_TV_d`
(a 16-row band with every fourth row lit) add up to 12 and 8, and `my_TVSpe_a`
lays a white vertical gradient over the top. Every banner texture is drawn
fresh at banner size and slots into that material unchanged:
- **Static:** 512×384 I4, every grain drawn from that frame's own mix of grey
  levels, so each frame keeps Nintendo's tone. Its width has to be a power of
  two: the material repeats it sideways, and GX can only repeat those.
- **Logo:** the official Wii logo, `assets/wii-logo.svg` (Wikimedia's, minus
  the ™), rendered once to `assets/wii-logo.png` with
  `inkscape assets/wii-logo.svg --export-type=png --export-width=1600 --export-area-drawing`.
  It's laid into the static at the frame's darkest level, as in the icon,
  squeezed a little so it comes out true on the non-square banner texels. It
  sits centered in rows 0–342, the part the Menu's button panel leaves
  visible.
- **Gradients:** the sheen (8×384 I8) and the scanline band (8×64 I8) are
  Nintendo's, resampled smoothly to banner height.

The banner is 403 KB uncompressed.

## 4. The sound (`build_sound.py`)

Banner sounds are BNS files (DSP-ADPCM). Normally you'd need an encoder, but
with both predictor coefficients set to zero, every 4-bit sample decodes on its
own, so random nibbles *are* white noise. We write 4 s of them at 32 kHz with
the loop flag set. `NOISE_DB` in `slot.py` sets the loudness: each frame's
scale sets it in 6 dB steps, and the spread of the random nibbles sets it in
between.

## 5. The WADs (`build_wad.py`)

A WAD needs a ticket, a TMD and Nintendo's certificate chain. We borrow those
from the official USB Loader GX forwarder (`USBLoaderGX_forwarder[UNEO].wad`),
because its TMD already asks for what a forwarder wants: IOS58, AHB access,
two contents with content 1 booting. Then:

1. Content 0 = the donor's banner archive with `meta/icon.bin`,
   `meta/banner.bin` and `meta/sound.bin` replaced, and the IMET header updated
   (sizes, blank names, MD5).
2. Content 1 = `forwarder.dol` with `FWDPATH=` set to `BOOT_PATH`.
3. Title ID → `00010001-ESLT`, title key re-encrypted for the new ID.
4. Contents re-hashed into the TMD, and the TMD and ticket fakesigned.

**The vWii trap:** a Wii starts a channel at `0x80003400` (our stub from step 1);
a Wii U's vWii jumps straight to the DOL's entry point instead, and our
forwarder froze the whole console there. So the vWii WAD gets one more content:
content 1 becomes [FIX94's vWii NAND loader](https://github.com/FIX94/tiny-vwii-nand-loader)
(r2, the same file USB Loader GX's own vWii forwarder uses), and the forwarder
moves to content 2, where the loader finds it and boots it the way the
Homebrew Channel would. `extract.py` downloads the loader and checks its
SHA-256. The loader doesn't run on a Wii, nor in Dolphin, which starts channels
the Wii's way, so Dolphin can only test the Wii WAD all the way through.

`verify.py` then checks both WADs, plus the traps that only show up on a
console: icon size cap, TMD offset `0x19A` left zero, IMET MD5, power-of-two
texture widths, and so on.

## 6. Make it yours

Everything you'd want to change is in `build/tools/slot.py`: `BOOT_PATH`,
`TITLE_ID`, `TITLE` (the hover name), `NOISE_DB`, `NOISE_SECONDS`,
`NOISE_GRAIN` (the static's grain, in texels) and the logo's size and place. For a different look entirely, swap `SLOT_LAYOUT` for
another layout, or replace `build_layouts.py` with your own art. The forwarder
and WAD steps don't care what the banner shows.
