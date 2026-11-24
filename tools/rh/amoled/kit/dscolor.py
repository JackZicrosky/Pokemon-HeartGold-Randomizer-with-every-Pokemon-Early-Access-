"""Nintendo DS colours: 15-bit BGR555 palette words (bit 15 unused by the 2D engines).

Two 8-bit sources need two different conversions to 5 bits:
- emulator captures (DeSmuME) output each 5-bit channel as v << 3, so v >> 3 is exact;
- the owner's previews were painted in an image editor, so a channel may hold any 8-bit value: a multiple of 8 is
  read as v >> 3 (it came from a capture), anything else is rounded to the nearest of the 32 levels.
Both rules are the ones tools/rh/amoled/proof3.py and learn.py use; this module is their single tested copy.
"""
from __future__ import annotations

RGB = tuple[int, int, int]


def chan_capture(v: int) -> int:
    """8-bit channel of an emulator capture -> 5-bit level."""
    if not 0 <= v <= 255:
        raise ValueError(f"channel out of range: {v}")
    return v >> 3


def chan_owner(v: int) -> int:
    """8-bit channel of an owner preview -> 5-bit level (exact for capture values, else nearest level)."""
    if not 0 <= v <= 255:
        raise ValueError(f"channel out of range: {v}")
    return v >> 3 if v % 8 == 0 else min(31, round(v * 31 / 255))


def rgb5_capture(p: RGB) -> RGB:
    return chan_capture(p[0]), chan_capture(p[1]), chan_capture(p[2])


def rgb5_owner(p: RGB) -> RGB:
    return chan_owner(p[0]), chan_owner(p[1]), chan_owner(p[2])


def pack(c: RGB) -> int:
    """5-bit (r, g, b) -> BGR555 word."""
    r, g, b = c
    for v in c:
        if not 0 <= v <= 31:
            raise ValueError(f"5-bit level out of range: {c}")
    return r | g << 5 | b << 10


def unpack(w: int) -> RGB:
    """BGR555 word -> 5-bit (r, g, b); bit 15 is ignored."""
    if not 0 <= w <= 0xFFFF:
        raise ValueError(f"not a 16-bit word: {w}")
    return w & 31, w >> 5 & 31, w >> 10 & 31


def to8(c: RGB) -> RGB:
    """5-bit (r, g, b) -> the 8-bit colour the emulator shows (v << 3), so captures and previews line up."""
    return c[0] << 3, c[1] << 3, c[2] << 3


def on_grid(p: RGB) -> bool:
    """True when an 8-bit colour is exactly one the DS can show (every channel a multiple of 8)."""
    return all(v % 8 == 0 for v in p)
