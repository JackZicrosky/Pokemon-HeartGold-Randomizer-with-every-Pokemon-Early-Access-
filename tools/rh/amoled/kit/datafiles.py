"""Readers / writers for the AMOLED theme's own data files (written by tools/rh/gen_rh_amoled.py, read by the theme
code in rhtheme/rh_theme_code.c). These are the only files the theme pipeline produces; the game's own NARC/NCLR/NCGR
files are never rewritten (the theme recolours at runtime, only when UI Theme = AMOLED, so Default stays vanilla).

RHA4 (data/rh/rh_amoled.bin): palette replacements
  'RHA4', u32 count, u32 mask[10]                      (mask: bit per archive number that has entries)
  count x { u32 key (narc << 16 | file), u32 ctx, u32 offset, u32 len | start << 16 }
  colour data: u16 BGR555 words, one block per entry, in entry order

RHPG (data/rh/rh_amoled_pages.bin): per-screen pages
  'RHPG', u32 nsigs, u32 nrows, u32 fuzzy_offset
  nsigs x { u32 sig, u32 offset }                      (offset bit 31 set: a screen-version marker, no page)
  nrows x 16 u16                                        (whole palette rows)
  pages, in sig order: pairs, tile patches, index patches, registers, row rules, sprite copies (see Page)
  u32 nfuzzy, nfuzzy x { u32 sig, u32 cfg, 48 u16 region hashes }
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field


class FormatError(ValueError):
    pass


def _need(buf: bytes, end: int, what: str) -> None:
    if end > len(buf):
        raise FormatError(f"truncated {what}: need {end} bytes, have {len(buf)}")


# ---------------------------------------------------------------- RHA4
@dataclass
class PalEntry:
    narc: int
    file: int
    ctx: int
    start: int
    colours: list[int]


@dataclass
class Rha4:
    mask: list[int]
    entries: list[PalEntry]


def parse_rha4(buf: bytes) -> Rha4:
    _need(buf, 48, "RHA4 header")
    if buf[:4] != b"RHA4":
        raise FormatError(f"not RHA4: magic {buf[:4]!r}")
    (count,) = struct.unpack_from("<I", buf, 4)
    mask = list(struct.unpack_from("<10I", buf, 8))
    _need(buf, 48 + 16 * count, "RHA4 entry table")
    entries = []
    for k in range(count):
        key, ctx, off, ls = struct.unpack_from("<IIII", buf, 48 + 16 * k)
        n = ls & 0xFFFF
        _need(buf, off + 2 * n, f"RHA4 entry {k} colours")
        cols = list(struct.unpack_from(f"<{n}H", buf, off))
        entries.append(PalEntry(key >> 16, key & 0xFFFF, ctx, ls >> 16, cols))
    return Rha4(mask, entries)


def write_rha4(f: Rha4) -> bytes:
    base = 48 + 16 * len(f.entries)
    head = bytearray(b"RHA4" + struct.pack("<I", len(f.entries)) + struct.pack("<10I", *f.mask))
    data = bytearray()
    for e in f.entries:
        if len(e.colours) >= 0x10000 or e.start >= 0x10000:
            raise FormatError("entry too long")
        head += struct.pack("<IIII", e.narc << 16 | e.file, e.ctx, base + len(data), len(e.colours) | e.start << 16)
        data += struct.pack(f"<{len(e.colours)}H", *e.colours)
    return bytes(head + data)


def validate_rha4(f: Rha4) -> list[str]:
    """Problems that would make the theme read wrong data (empty list = fine)."""
    out = []
    want = [0] * 10
    keys = []
    for e in f.entries:
        if e.narc >> 5 >= 10:
            out.append(f"archive {e.narc} beyond the mask")
        else:
            want[e.narc >> 5] |= 1 << (e.narc & 31)
        keys.append((e.narc, e.file, e.ctx))
        for c in e.colours:
            if not 0 <= c <= 0xFFFF:
                out.append(f"colour word out of range in {e.narc}/{e.file}")
    if want != f.mask:
        out.append("archive mask does not match the entries")
    if keys != sorted(keys):
        out.append("entries not sorted (the theme binary-searches them)")
    return out


# ---------------------------------------------------------------- RHPG
@dataclass
class RowRule:
    w0: int
    w1: int
    cols: list[int]


@dataclass
class Page:
    pairs: list[tuple[int, int]] = field(default_factory=list)
    vpatch: list[int] = field(default_factory=list)       # tile patches (address, nibble, from, to packed)
    ipatch: list[int] = field(default_factory=list)       # palette index patches: index << 16 | colour
    regs: list[int] = field(default_factory=list)         # register writes: addr << 16 | value
    rules: list[RowRule] = field(default_factory=list)
    objc: list[tuple[int, int]] = field(default_factory=list)


@dataclass
class Rhpg:
    sigs: list[tuple[int, int]]                           # (sig, marker or 0); pages follow for unmarked sigs
    rows: list[tuple[int, ...]]
    pages: dict[int, Page]
    fuzzy: list[tuple[int, ...]]


def parse_rhpg(buf: bytes) -> Rhpg:
    _need(buf, 16, "RHPG header")
    if buf[:4] != b"RHPG":
        raise FormatError(f"not RHPG: magic {buf[:4]!r}")
    nsig, nrow, fzoff = struct.unpack_from("<III", buf, 4)
    p = 16
    _need(buf, p + 8 * nsig + 32 * nrow, "RHPG tables")
    raw = [struct.unpack_from("<II", buf, p + 8 * k) for k in range(nsig)]
    p += 8 * nsig
    rows = [struct.unpack_from("<16H", buf, p + 32 * k) for k in range(nrow)]
    p += 32 * nrow
    sigs, pages = [], {}
    for sg, off in raw:
        if off >> 31:
            sigs.append((sg, off))
            continue
        if off != p:
            raise FormatError(f"page of {sg:#x} at {off}, expected {p} (pages must follow in sig order)")
        pg = Page()

        def u32() -> int:
            nonlocal p
            _need(buf, p + 4, "page")
            (v,) = struct.unpack_from("<I", buf, p)
            p += 4
            return v

        pg.pairs = [(u32(), u32()) for _ in range(u32())]
        pg.vpatch = [u32() for _ in range(u32())]
        pg.ipatch = [u32() for _ in range(u32())]
        pg.regs = [u32() for _ in range(u32())]
        for _ in range(u32()):
            w0, w1 = u32(), u32()
            n = bin(w1 >> 16).count("1")
            n += n & 1
            _need(buf, p + 2 * n, "row rule colours")
            pg.rules.append(RowRule(w0, w1, list(struct.unpack_from(f"<{n}H", buf, p))))
            p += 2 * n
        pg.objc = [(u32(), u32()) for _ in range(u32())]
        sigs.append((sg, 0))
        pages[sg] = pg
    if p != fzoff:
        raise FormatError(f"fuzzy section at {fzoff}, pages end at {p}")
    _need(buf, p + 4, "fuzzy count")
    (nfz,) = struct.unpack_from("<I", buf, p)
    p += 4
    _need(buf, p + 104 * nfz, "fuzzy table")
    fuzzy = [struct.unpack_from("<II48H", buf, p + 104 * k) for k in range(nfz)]
    p += 104 * nfz
    if p != len(buf):
        raise FormatError(f"{len(buf) - p} trailing bytes")
    return Rhpg(sigs, rows, pages, fuzzy)


def write_rhpg(f: Rhpg) -> bytes:
    base = 16 + 8 * len(f.sigs) + 32 * len(f.rows)
    data = bytearray()
    offs = []
    for sg, mark in f.sigs:
        if mark:
            offs.append(mark)
            continue
        offs.append(base + len(data))
        pg = f.pages[sg]
        data += struct.pack("<I", len(pg.pairs)) + b"".join(struct.pack("<II", a, b) for a, b in pg.pairs)
        for lst in (pg.vpatch, pg.ipatch, pg.regs):
            data += struct.pack("<I", len(lst)) + b"".join(struct.pack("<I", v) for v in lst)
        data += struct.pack("<I", len(pg.rules))
        for r in pg.rules:
            data += struct.pack("<II", r.w0, r.w1) + struct.pack(f"<{len(r.cols)}H", *r.cols)
        data += struct.pack("<I", len(pg.objc)) + b"".join(struct.pack("<II", a, b) for a, b in pg.objc)
    out = bytearray(b"RHPG" + struct.pack("<III", len(f.sigs), len(f.rows), base + len(data)))
    for (sg, _), off in zip(f.sigs, offs):
        out += struct.pack("<II", sg, off)
    for r in f.rows:
        out += struct.pack("<16H", *r)
    out += data
    out += struct.pack("<I", len(f.fuzzy)) + b"".join(struct.pack("<II48H", *v) for v in f.fuzzy)
    return bytes(out)


MAX_TILE_PATCHES = 8192    # rhtheme/rh_theme_code.c loads at most this many per screen


def validate_rhpg(f: Rhpg) -> list[str]:
    out = []
    keys = [sg for sg, _ in f.sigs]
    if keys != sorted(keys):
        out.append("fingerprints not sorted (the theme binary-searches them)")
    if len(set(keys)) != len(keys):
        out.append("duplicate fingerprints")
    for sg, pg in f.pages.items():
        if len(pg.vpatch) > MAX_TILE_PATCHES:
            out.append(f"{sg:#x}: {len(pg.vpatch)} tile patches, the theme loads {MAX_TILE_PATCHES}")
        for v in pg.ipatch:
            if v >> 16 >= 1024:
                out.append(f"{sg:#x}: palette index {v >> 16} beyond the 1024 2D palette entries")
                break
    for sg, mark in f.sigs:
        if mark and (mark & 0x7FFFFFFF) >= 1024:
            out.append(f"{sg:#x}: version key entry {mark & 0x7FFFFFFF} out of range")
    return out
