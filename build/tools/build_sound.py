"""Build sound.bin: a looping mono BNS of white noise.

With both predictor coefficients at zero, DSP ADPCM decodes each nibble on its
own, so random nibbles already are white noise and no encoder is needed.
"""

import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))

from slot import NOISE_DB, NOISE_SECONDS, OUT
from wiilib import imd5_wrap

RATE = 32000
SAMPLES_PER_FRAME = 14


def build_bns(adpcm, coefs, rate, total, ps, loop=False):
    info = bytearray(96)
    info[0:4] = b"INFO"
    struct.pack_into(">I", info, 4, 96)
    info[8] = 0                       # codec 0 = DSP ADPCM
    info[9] = 1 if loop else 0
    info[10] = 1                      # mono
    struct.pack_into(">H", info, 12, rate)
    struct.pack_into(">I", info, 16, 0)          # loop start
    struct.pack_into(">I", info, 20, total)      # loop end / total samples
    struct.pack_into(">I", info, 24, 0x18)       # -> channel table
    struct.pack_into(">I", info, 32, 0x1C)       # -> channel info
    struct.pack_into(">I", info, 40, 0x28)       # -> coefficients (INFO+0x30)
    for i, c in enumerate(coefs):
        struct.pack_into(">h", info, 48 + i * 2, c)
    struct.pack_into(">H", info, 82, ps)         # initial predictor/scale
    struct.pack_into(">H", info, 88, ps)         # loop predictor/scale
    data = b"DATA" + struct.pack(">I", len(adpcm) + 8) + adpcm
    head = bytearray(32)
    head[0:4] = b"BNS "
    head[4:8] = bytes.fromhex("feff0100")
    struct.pack_into(">I", head, 8, 32 + len(info) + len(data))
    struct.pack_into(">HH", head, 12, 32, 2)
    struct.pack_into(">II", head, 16, 32, len(info))
    struct.pack_into(">II", head, 24, 32 + len(info), len(data))
    return bytes(head) + bytes(info) + data


def noise_frames(count, rng):
    """DSP ADPCM frames of Gaussian noise at NOISE_DB. The frame scale sets loudness in 6 dB steps,
    and the nibbles' spread sets it between them."""
    target = 32768 * 10 ** (NOISE_DB / 20)
    scale = max(0, math.ceil(math.log2(target / 2.5)))  # keeps the spread small enough not to clip
    sigma = target / (1 << scale)
    out = bytearray()
    for _ in range(count):
        nibbles = [max(-8, min(7, round(rng.gauss(0, sigma)))) & 0xF
                   for _ in range(SAMPLES_PER_FRAME)]
        out.append(scale)
        out += bytes((a << 4) | b for a, b in zip(nibbles[::2], nibbles[1::2]))
    return bytes(out), scale


def main():
    frames = RATE * NOISE_SECONDS // SAMPLES_PER_FRAME
    total = frames * SAMPLES_PER_FRAME
    adpcm, scale = noise_frames(frames, random.Random(0))
    bns = build_bns(adpcm, [0] * 16, RATE, total, scale, loop=True)
    packed = imd5_wrap(bns)
    open(os.path.join(OUT, "sound.bin"), "wb").write(packed)
    print(f"  sound.bin: {total / RATE:.2f} s of looping static, {len(packed):,} bytes")


if __name__ == "__main__":
    main()
