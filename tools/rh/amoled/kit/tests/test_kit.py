"""Tests for the AMOLED kit. Synthetic data only (no game data is committed); the real theme data files are
round-tripped when a build has made them (skipped otherwise)."""
from __future__ import annotations

import json
import os
import struct
from pathlib import Path

import pytest
from PIL import Image

from kit import datafiles, dscolor, imaging, manifest, paths, safety


# ------------------------------------------------------------------ colours
def test_pack_unpack_all_colours():
    for w in range(0x8000):
        assert dscolor.pack(dscolor.unpack(w)) == w


def test_unpack_ignores_bit15():
    assert dscolor.unpack(0x8000 | 0x1234) == dscolor.unpack(0x1234)


def test_capture_rule_is_exact():
    for v in range(32):
        assert dscolor.chan_capture(v << 3) == v


def test_owner_rule_rounds_off_grid_values():
    assert dscolor.chan_owner(16) == 2              # on the grid: exact
    assert dscolor.chan_owner(255) == 31            # editor white
    assert dscolor.chan_owner(17) == 2              # off grid: nearest level (17 * 31 / 255 = 2.07)
    assert dscolor.chan_owner(250) == 30


@pytest.mark.parametrize("bad", [-1, 256])
def test_channel_range(bad):
    with pytest.raises(ValueError):
        dscolor.chan_owner(bad)


def test_pack_range():
    with pytest.raises(ValueError):
        dscolor.pack((32, 0, 0))


# ------------------------------------------------------------------ data files
def sample_rha4() -> datafiles.Rha4:
    e = [datafiles.PalEntry(7, 246, 0, 0, [0, 0x7FFF, 0x1234]),
         datafiles.PalEntry(71, 23, 39659, 192, [0x2646] * 16)]
    mask = [0] * 10
    for x in e:
        mask[x.narc >> 5] |= 1 << (x.narc & 31)
    return datafiles.Rha4(mask, e)


def sample_rhpg() -> datafiles.Rhpg:
    rows = [tuple(range(16)), tuple(range(16, 32))]
    pg = datafiles.Page(pairs=[(0, 1)], vpatch=[0x0620EF30, 5], ipatch=[207 << 16 | 0x2646], regs=[0x50 << 16 | 1],
                        rules=[datafiles.RowRule(1 | 2 << 24, 0x1F | 0b111 << 16, [1, 2, 3, 0])], objc=[(3, 4)],
                        rows_inline=struct.pack('<32H', *range(32)))
    return datafiles.Rhpg(sigs=[(0x10, 0), (0x20, 0x80000000 | 12), (0x30, 0)], rows=rows,
                          pages={0x10: pg, 0x30: datafiles.Page()}, fuzzy=[(0x10, 7) + (0x9070,) * 48])


def test_rha4_round_trip():
    f = sample_rha4()
    buf = datafiles.write_rha4(f)
    assert datafiles.write_rha4(datafiles.parse_rha4(buf)) == buf
    assert datafiles.parse_rha4(buf) == f
    assert datafiles.validate_rha4(f) == []


def test_rhpg_round_trip():
    f = sample_rhpg()
    buf = datafiles.write_rhpg(f)
    g = datafiles.parse_rhpg(buf)
    assert datafiles.write_rhpg(g) == buf
    assert g.pages[0x10].rules[0].cols == [1, 2, 3, 0]
    assert datafiles.validate_rhpg(g) == []


@pytest.mark.parametrize("cut", [3, 20, 60])
def test_truncated_files_fail_loudly(cut):
    with pytest.raises(datafiles.FormatError):
        datafiles.parse_rha4(datafiles.write_rha4(sample_rha4())[:cut])
    with pytest.raises(datafiles.FormatError):
        datafiles.parse_rhpg(datafiles.write_rhpg(sample_rhpg())[:cut])


def test_bad_magic_and_trailing_bytes():
    with pytest.raises(datafiles.FormatError):
        datafiles.parse_rha4(b"NCLR" + bytes(60))
    with pytest.raises(datafiles.FormatError):
        datafiles.parse_rhpg(datafiles.write_rhpg(sample_rhpg()) + b"\0")


def test_validation_catches_bad_data():
    f = sample_rha4()
    f.entries.reverse()
    assert any("sorted" in p for p in datafiles.validate_rha4(f))
    f = sample_rha4()
    f.mask[0] = 0
    assert any("mask" in p for p in datafiles.validate_rha4(f))
    g = sample_rhpg()
    g.pages[0x10].ipatch.append(1024 << 16)
    g.pages[0x10].rows_inline = bytes(64)
    g.pages[0x30].vpatch = [0] * (datafiles.MAX_TILE_PATCHES + 1)
    probs = datafiles.validate_rhpg(g)
    assert any("1024" in p for p in probs) and any("tile patches" in p for p in probs)
    assert any("inline rows" in p for p in probs)


@pytest.mark.parametrize("which", [0, 1])
def test_real_theme_data_round_trip(which):
    p = paths.theme_data()[which]
    if not p.exists():
        pytest.skip("no build output")
    buf = p.read_bytes()
    parse, write, validate = ((datafiles.parse_rha4, datafiles.write_rha4, datafiles.validate_rha4),
                              (datafiles.parse_rhpg, datafiles.write_rhpg, datafiles.validate_rhpg))[which]
    f = parse(buf)
    assert write(f) == buf
    assert validate(f) == []


# ------------------------------------------------------------------ images
def img(path: Path, colour=(0, 0, 0), size=(8, 4), mode="RGB", alpha=255) -> Path:
    im = Image.new(mode, size, colour + ((alpha,) if mode == "RGBA" else ()))
    im.save(path)
    return path


def test_compare_identical_and_exact_coordinates(tmp_path):
    a = img(tmp_path / "a.png", (8, 16, 24))
    b = img(tmp_path / "b.png", (8, 16, 24))
    assert imaging.compare(a, b).wrong == 0
    im = Image.open(b)
    im.putpixel((5, 2), (248, 0, 0))
    im.save(b)
    d = imaging.compare(a, b)
    assert [(x, y) for x, y, _, _ in d.colour] == [(5, 2)]
    assert d.colour[0][2] == (1, 2, 3) and d.colour[0][3] == (31, 0, 0)


def test_transparency_is_separate_from_colour(tmp_path):
    a = img(tmp_path / "a.png", (0, 0, 0), mode="RGBA")
    b = tmp_path / "b.png"
    im = Image.new("RGBA", (8, 4), (0, 0, 0, 255))
    im.putpixel((1, 1), (0, 0, 0, 0))
    im.save(b)
    d = imaging.compare(a, b, cand_owner=True)
    assert d.alpha == [(1, 1)] and d.colour == []


def test_size_mismatch_is_an_error_not_a_resize(tmp_path):
    with pytest.raises(ValueError):
        imaging.compare(img(tmp_path / "a.png"), img(tmp_path / "b.png", size=(9, 4)))


def test_quantised_keeps_layout_and_on_grid_pixels(tmp_path):
    p = tmp_path / "p.png"
    im = Image.new("RGB", (4, 1))
    im.putdata([(8, 16, 24), (255, 255, 255), (17, 0, 0), (0, 0, 0)])
    im.save(p)
    q = imaging.quantised(p)
    assert q.size == (4, 1)
    assert list(imaging.pixels(q)) == [(8, 16, 24), (248, 248, 248), (16, 0, 0), (0, 0, 0)]
    assert imaging.info(p).off_grid == 2


# ------------------------------------------------------------------ safety
@pytest.fixture
def sandbox(tmp_path, monkeypatch):
    """A fake project: previews, a capture, learned data; staging inside tmp."""
    pa, pd, amw, ld = (tmp_path / "prevA", tmp_path / "prevD", tmp_path / "amw", tmp_path / "learned")
    for d in (pa, pd, amw / "caps", amw / "rec" / "am_px_c1", ld):
        d.mkdir(parents=True)
    for name in ("01_x_menu.png", "02_x_menu_pressed.png"):
        img(pa / name, (0, 0, 0), size=(256, 384))
        img(pd / name, (248, 248, 248), size=(256, 384))
    img(amw / "rec" / "am_px_c1" / "f000.png", (0, 0, 0), size=(256, 384))
    (amw / "rec" / "am_px_c1" / "index.json").write_text(json.dumps({"frames": ["f000.png"]}))
    (amw / "matches.json").write_text(json.dumps({"01_x_menu.png": "c1", "02_x_menu_pressed.png": "c2"}))
    (ld / "screens.json").write_text(json.dumps({"01_x_menu.png": {"sig": 1, "cols": {"a": 1}},
                                                 "02_x_menu_pressed.png": {"sig": 2}}))
    monkeypatch.setenv("RH_PREVIEWS_AMOLED", str(pa))
    monkeypatch.setenv("RH_PREVIEWS_DEFAULT", str(pd))
    monkeypatch.setenv("RH_AMW", str(amw))
    monkeypatch.setenv("RH_LEARNED_DIR", str(ld))
    monkeypatch.setenv("RH_KIT_STAGING", str(tmp_path / "staging"))
    return tmp_path


def tree_hashes(root: Path) -> dict:
    return {str(p): safety.sha256(p) for p in root.rglob("*") if p.is_file()}


def test_dry_run_writes_nothing(sandbox):
    from kit import cli
    before = tree_hashes(sandbox)
    assert cli.main(["preview", "01_x_menu"]) == 0
    assert cli.main(["manifest"]) == 0
    assert tree_hashes(sandbox) == before
    assert not (sandbox / "staging").exists()


def test_write_only_into_new_run_folders(sandbox):
    from kit import cli
    src = {k: v for k, v in tree_hashes(sandbox).items()}
    assert cli.main(["preview", "01_x_menu", "--write"]) == 0
    assert cli.main(["preview", "01_x_menu", "--write"]) == 0
    after = tree_hashes(sandbox)
    for k, v in src.items():
        assert after[k] == v                          # sources unchanged
    runs = [d for d in (sandbox / "staging").iterdir() if d.is_dir()]
    assert len(runs) == 2                             # second run did not overwrite the first
    assert all((d / "sheet.png").exists() and (d / "run.json").exists() for d in runs)


def test_refuses_protected_and_outside_paths(sandbox):
    run = safety.Run("t", {}, dry=False)
    with pytest.raises(safety.UnsafeWrite):
        safety.check_target(sandbox / "prevA" / "x.png", sandbox / "prevA")     # protected even if staging
    with pytest.raises(safety.UnsafeWrite):
        safety.check_target(sandbox / "elsewhere.png", run.staging)
    with pytest.raises(safety.UnsafeWrite):
        run.write_bytes("../../prevA/evil.png", b"x")


def test_no_overwrite_inside_a_run(sandbox):
    run = safety.Run("t", {}, dry=False)
    run.write_bytes("a.txt", b"1")
    with pytest.raises(safety.UnsafeWrite):
        run.write_bytes("a.txt", b"2")


def test_states_stay_separate_records(sandbox):
    rows = manifest.build()
    by = {r["picture"]: r for r in rows}
    assert by["01_x_menu.png"]["state"] == "normal"
    assert by["02_x_menu_pressed.png"]["state"] == "pressed"
    assert by["01_x_menu.png"]["preview_amoled_sha256"] != "" and len(rows) == 2
    assert by["02_x_menu_pressed.png"]["status"] == "implemented"
