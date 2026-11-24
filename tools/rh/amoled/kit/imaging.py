"""Exact, no-resampling image work: DS-quantised previews, pixel comparison, difference images.

Nothing here resizes, filters or smooths. Every comparison is in 5-bit DS levels, never in 8-bit values.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PIL import Image

from . import dscolor

SCREEN = (256, 384)


def pixels(im: Image.Image):
    """All pixels in row order (Pillow 12 renamed getdata)."""
    return getattr(im, 'get_flattened_data', im.getdata)()        # both DS screens stacked (top 0-191, bottom 192-383)


@dataclass
class ImageInfo:
    size: tuple[int, int]
    mode: str
    opaque: bool           # every pixel alpha 255 (or no alpha channel)
    off_grid: int          # pixels whose 8-bit colour is not one the DS shows exactly


def info(path: Path) -> ImageInfo:
    im = Image.open(path)
    opaque = True
    if im.mode in ("RGBA", "LA") or "transparency" in im.info:
        a = im.convert("RGBA").getchannel("A")
        opaque = a.getextrema() == (255, 255)
    rgb = im.convert("RGB")
    off = sum(1 for p in pixels(rgb) if not dscolor.on_grid(p))
    return ImageInfo(im.size, im.mode, opaque, off)


def load5(path: Path, owner: bool) -> tuple[Image.Image, Image.Image | None]:
    """An image as 5-bit levels (stored in an RGB image, values 0-31) and its alpha (None if fully opaque).
    owner=True uses the preview rule (rounding), False the capture rule (exact)."""
    im = Image.open(path)
    alpha = None
    if im.mode in ("RGBA", "LA") or "transparency" in im.info:
        a = im.convert("RGBA").getchannel("A")
        if a.getextrema() != (255, 255):
            alpha = a
    conv = dscolor.chan_owner if owner else dscolor.chan_capture
    lut = [conv(v) for v in range(256)] * 3
    return im.convert("RGB").point(lut), alpha


def quantised(path: Path) -> Image.Image:
    """What the DS can actually show of an owner preview: each pixel at its 5-bit level, shown as the emulator does."""
    lv, _ = load5(path, owner=True)
    return lv.point(([v << 3 for v in range(32)] + [0] * 224) * 3)


@dataclass
class Diff:
    size: tuple[int, int]
    colour: list[tuple[int, int, tuple[int, int, int], tuple[int, int, int]]]   # (x, y, ref level, candidate level)
    alpha: list[tuple[int, int]]                                                # transparency differs here

    @property
    def wrong(self) -> int:
        return len(self.colour) + len(self.alpha)

    def percent_ok(self) -> float:
        w, h = self.size
        return 100.0 * (w * h - self.wrong) / (w * h)


def compare(ref: Path, cand: Path, ref_owner: bool = True, cand_owner: bool = False,
            region: tuple[int, int, int, int] | None = None) -> Diff:
    """Every pixel of ref vs cand in DS levels. Sizes must match exactly (no alignment guessing)."""
    a, aa = load5(ref, ref_owner)
    b, ba = load5(cand, cand_owner)
    if a.size != b.size:
        raise ValueError(f"size mismatch: {ref} {a.size} vs {cand} {b.size} (no resizing is done)")
    x0, y0, x1, y1 = region or (0, 0, a.size[0], a.size[1])
    pa, pb = a.load(), b.load()
    col, alp = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            ta = aa.getpixel((x, y)) if aa else 255
            tb = ba.getpixel((x, y)) if ba else 255
            if (ta == 0) != (tb == 0):
                alp.append((x, y))
            elif ta and pa[x, y] != pb[x, y]:
                col.append((x, y, pa[x, y], pb[x, y]))
    return Diff(a.size, col, alp)


def heatmap(d: Diff) -> Image.Image:
    """Black = equal; red brightness = colour distance in DS levels; cyan = transparency differs."""
    im = Image.new("RGB", d.size)
    px = im.load()
    for x, y, p, q in d.colour:
        dist = max(abs(p[i] - q[i]) for i in range(3))
        px[x, y] = (min(255, 96 + dist * 5), 0, 0)
    for x, y in d.alpha:
        px[x, y] = (0, 255, 255)
    return im


def sheet(images: list[Image.Image]) -> Image.Image:
    """Side by side, same size, no scaling."""
    w, h = images[0].size
    for im in images:
        if im.size != (w, h):
            raise ValueError("sheet images differ in size")
    out = Image.new("RGB", (w * len(images), h))
    for k, im in enumerate(images):
        out.paste(im.convert("RGB"), (w * k, 0))
    return out


def overlay(base: Image.Image, d: Diff) -> Image.Image:
    """The reference with every wrong pixel painted pure red (transparency differences cyan)."""
    im = base.convert("RGB").copy()
    px = im.load()
    for x, y, _, _ in d.colour:
        px[x, y] = (255, 0, 0)
    for x, y in d.alpha:
        px[x, y] = (0, 255, 255)
    return im
