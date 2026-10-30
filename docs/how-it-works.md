# How Empty Slot works

An Empty Slot channel is a forwarder in disguise. This page explains the
disguise, what happens when you press Start, and how the Creator makes a new
channel on the console itself. To build everything from source, see
[from-scratch.md](from-scratch.md).

## A few words first

- **WAD**: the file a channel is installed from.
- **Title ID**: the four-character code that tells channels apart. Two
  channels with the same ID replace each other.
- **Forwarder**: a channel whose only job is to start a homebrew app.
- **Banner**: everything the Wii Menu shows for a channel: the small picture
  in the grid (the icon), the full-screen picture after you click it, and its
  sound.

## The disguise

**The icon** is Nintendo's own. The Wii Menu draws every unused grid slot from
a small layout called `my_IplTop_b`: four frames of grey static that flicker in
turn, two bands of scanlines that roll slowly, and a soft glass sheen. Our icon
is that layout, copied byte for byte from the System Menu, so the Menu draws
our channel exactly as it draws a slot with nothing in it.

**The name** is blank, in all ten languages. Hovering over a channel normally
shows its name in a balloon; ours has nothing to show.

**The banner** is the same layout blown up to fill the screen. Scaling the icon
up as it is would look blocky, so every picture in it is redrawn at full size:
fresh static in the same shades of grey as Nintendo's, the official Wii logo
pressed into it, and the sheen and scanlines resampled smoothly. The
animation is still Nintendo's, so it flickers and rolls just like the icon.

**The sound** is four seconds of looping white noise. Banner sounds use a
compressed format called DSP-ADPCM, which normally needs an encoder. But the
format predicts each sample from the ones before it, and with prediction turned
off, every 4-bit value decodes on its own. Random values are then already white
noise.

## Pressing Start

A channel's WAD holds a few files called contents, and the TMD (the WAD's table
of contents) says which one the console starts.

**On a Wii**, that's our forwarder, a small program of about 200 lines. It:

1. looks for the app on the SD card, then on a USB drive (waiting a few seconds
   for a hard drive to spin up);
2. tries `boot.dol`, then `boot.elf`;
3. loads the app into memory, and hands it its own path the way the Homebrew
   Channel does, so the app can find its files;
4. jumps into it.

If the app isn't there, it says so on screen and goes back to the Wii Menu.

**On a Wii U**, the Wii mode (vWii) starts a channel differently. A Wii begins
at a fixed address where our forwarder keeps a little start-up routine; a Wii U
jumps straight to the program's entry point instead, skipping that routine,
and the console freezes. So the vWii WAD has one more content. The
console starts FIX94's tiny vWii NAND loader, a few kilobytes that read our
forwarder from the channel and start it the way the Homebrew Channel would.
USB Loader GX's official vWii forwarder works the same way.

## How the Creator makes a channel

Both finished WADs, one for the Wii and one for the vWii, are built into the
Creator. Making a channel means changing two things in a copy: which app the
forwarder starts, and the channel's title ID.

1. **Unlock it.** WAD contents are encrypted. The ticket holds the channel's
   key, itself encrypted with a key every Wii shares, so the Creator can
   recover it and decrypt the forwarder.
2. **Write the path.** The forwarder keeps the app's path as plain text after
   the marker `FWDPATH=`, with room for 247 characters. The Creator finds the
   marker and writes the new path after it.
3. **Give it its own ID.** The Creator makes a four-character ID from the app's
   path, starting with `E`, so the same app always gets the same ID. The
   ticket stores the channel's key scrambled with the title ID, so the key is
   re-encrypted to match.
4. **Lock it again.** The forwarder is re-encrypted, and its new SHA-1 goes into
   the TMD.
5. **Sign it.** A real signature needs Nintendo's private key. Instead, the
   WAD is *fakesigned*: the signature is zeroed, and a spare field is counted
   up until the signed data's SHA-1 starts with a zero byte. A patched IOS
   only checks that much, which is why installing needs a WAD manager running
   on one.

The AES and SHA-1 work runs on the console's own crypto hardware, through IOS.
`retarget.py` does the same steps on a PC, and both make exactly the same file.

The Creator only writes the WAD. Installing channels is left to WAD managers
like YAWMM ModMii Edition, which already do it well.

## Telling Wii from Wii U

A Wii WAD freezes a Wii U, so the Creator has to know which one it's running
on. The Wii U's Latte chip reports a chip ID starting with `CAFE` in a
hardware register; a Wii's Hollywood chip doesn't. The Homebrew Channel lets the
Creator read it, thanks to the `<ahb_access/>` line in its `meta.xml`. If it
can't read it, for example when started from another loader, the Creator asks
you to choose before it makes anything.
