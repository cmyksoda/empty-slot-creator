# Empty Slot Creator

A Homebrew Channel app that makes Empty Slot channels on the console itself.
Pick any homebrew app on your SD card or USB drive, and it saves a WAD to
`sd:/wad/` for a channel that looks like an unused slot on the Wii Menu and
opens that app. It doesn't install anything: install the WAD afterwards with
a WAD manager such as YAWMM ModMii Edition.

## Using it

1. Copy `boot.dol` and `meta.xml` to `sd:/apps/emptyslot/`.
2. Open **Empty Slot Creator** from the Homebrew Channel.
3. Pick an app with up and down (left and right jump a page), then press **A**.

| Button | Does |
|---|---|
| A | make the WAD |
| + / − | pick another channel ID |
| 1 | switch between Wii and vWii |
| HOME | back to the Homebrew Channel |

A GameCube controller works too (R and L for + and −, Y for 1, Start to quit).

**Wii or vWii.** A Wii U needs a different WAD from a Wii, and the wrong one
freezes it. The app checks which console it's on, which needs the AHB access
the Homebrew Channel grants it. If it can't tell (started from another
loader), it asks you to choose with 1 before it makes anything.

**Channel IDs.** Each channel needs its own 4-character ID. The app makes one
from the app's path, starting with `E`, so making a channel for the same app
again gives the same ID and installing it replaces the old one. If the ID is
already installed, the screen says so; press + or − for another.

**File names.** WADs are saved as
`Empty Slot - <app> - <ID>.wad`, or `... (vWii).wad`. On a card whose `wad`
folder has run out of room for long names, the app saves `ES<ID>.WAD`
(`ES<ID>V.WAD` for vWii) instead and says so.

## How it works

The two Empty Slot WADs from the parent project are built into the app
(`data/wii.wad`, `data/vwii.wad`). For the app you pick, `source/wad.c` does what
`retarget.py` does: decrypts the forwarder, writes the app's path into its
`FWDPATH=` field, sets the new title ID, re-encrypts, re-hashes and fakesigns.
The AES and SHA-1 run on the console's own crypto engine through IOS.

## Building

Needs devkitPPC, libogc and libfat (devkitPro).

```sh
make templates   # copy the parent project's freshly built WADs into data/
make
```

`test/retarget_test.sh` builds `wad.c` for the PC (with OpenSSL) and checks it
makes exactly the same bytes as `retarget.py` for both WADs.
