# Make an Empty Slot for any homebrew app (using our WAD)

The easiest way is the Empty Slot Creator: put it in `apps/` on your SD card,
open it from the Homebrew Channel, pick an app, and it saves a ready WAD to
`sd:/wad/`. This page is the PC way. It needs Python and an Empty Slot WAD
to start from. No compiler, no hex editor.

## 1. Get the tools

1. Install [Python 3](https://www.python.org/downloads/) (on Windows, tick
   "Add python.exe to PATH" in the installer).
2. Open a terminal (Windows: `cmd`) and run:
   ```
   pip install pycryptodome
   ```
3. Put these two files in one folder:
   - an Empty Slot WAD for your console: `Empty Slot - ESLT [cmyksoda].wad`
     for a Wii, or `Empty Slot - ESLT [cmyksoda] (vWii).wad` for a Wii U. The
     Wii one freezes a Wii U. The release only ships the Creator, so build
     them first (see [from-scratch.md](from-scratch.md)), or start from a WAD
     the Creator made.
   - `retarget.py`

## 2. Find your app's path

Look on your SD card or USB drive. Every Homebrew Channel app lives in
`apps/<folder>/boot.dol` (or `boot.elf`). Write the path down starting from the
root, with forward slashes:

| App | Path |
|---|---|
| USB Loader GX | `/apps/usbloader_gx/boot.dol` |
| WiiFlow | `/apps/wiiflow/boot.dol` |
| WiiXplorer | `/apps/WiiXplorer/boot.dol` |
| Nintendont | `/apps/Nintendont/boot.dol` |

Paths are case-sensitive on some setups, so copy the folder name exactly. Don't
put `sd:` or `usb:` in front: the forwarder checks SD first, then USB. If the
app ships a `boot.elf` instead, either name works.

## 3. Pick a title ID

Every channel needs its own 4-character ID (`A`–`Z`, `0`–`9`). Two channels with
the same ID overwrite each other, so:
- don't reuse `ESLT` if you also want the USB Loader GX one installed
- avoid IDs of real games and channels. Search the ID on
  [GameTDB](https://www.gametdb.com/Wii) to check.

Something like `ESW1`, `ESW2`, … is fine.

## 4. Run it

```
python retarget.py "Empty Slot - ESLT [cmyksoda].wad" /apps/wiiflow/boot.dol ESW1
```

You get `Empty Slot - ESW1.wad`. Add a fourth argument to choose the output name.
Run it again for as many apps as you like, with a new ID each time. For a Wii U,
start from the `(vWii)` WAD instead; `retarget.py` handles both.

## 5. Install it

Install the WAD like any other forwarder, with a WAD manager such as
YAWM ModMii Edition. Fakesigned WADs need a cIOS (d2x), which you already have
if you use USB Loader GX. As with any WAD, have a BootMii or Priiloader
backup before you install.

## Changing more than the target

The banner, icon and sound are in content 0. To swap any of them, use a banner
tool (e.g. CustomizeMii on Windows) on the retargeted WAD. After editing,
re-run `retarget.py` on its output to make sure the path is still right.

## How retarget.py works (if you're curious)

It decrypts the WAD's contents, finds the text `FWDPATH=`, writes your path
after it (the field is 256 bytes, so paths up to 247 characters fit), changes
the title ID in the ticket and TMD, re-hashes, fakesigns, and saves. You could do
the same by hand with any WAD unpacker and a hex editor, but the script is
quicker and checks your input.
