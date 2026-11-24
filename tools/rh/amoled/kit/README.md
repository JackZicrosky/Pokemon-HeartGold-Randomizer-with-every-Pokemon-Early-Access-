# AMOLED theme kit

Inventory, checks and comparison images for the HeartGold AMOLED UI theme. Part of the theme pipeline in
`tools/rh/amoled/`; it reads that pipeline's files and never changes them.

## How the theme works (and why no game graphics file is edited)

The UI Theme is chosen in-game (Default / Randomizer / AMOLED). The Default theme must stay exactly vanilla, so the
game's own graphics (NCLR / NCGR / NSCR / NCER / NANR in NARC archives) are **never rewritten**. Instead the AMOLED
theme recolours at runtime, only while AMOLED is selected:

| stage | tool | what it does |
|---|---|---|
| capture | `build/amw/cap.py` (py-desmume, headless) | drives the game to each screen; records frames, VRAM, palette RAM, registers; **colour-ID mode** draws every palette entry as a unique colour so each pixel names its palette load (archive, member, block, entry) |
| map | `learn.py` (`matches.json`) | picks the capture showing each owner picture's screen (pixel comparison with the owner's Default picture) |
| solve | `learn.py` -> `screens.json`, `learned.json` | the colour each palette entry needs (per entry, never per pixel); a colour used two ways gets a spare entry + tile patch, a palette-row switch (map patch) or a tile copy; runtime-drawn text tiles are never patched |
| write | `gen_rh_amoled.py` -> `data/rh/rh_amoled.bin` (RHA4), `rh_amoled_pages.bin` (RHPG) | the theme's own data files, built into the ROM's file system next to the untouched originals |
| prove | `build/amw/proof3.py` | every pixel of the owner's picture vs the AMOLED build's frame and the Default frame of the same moment; proven = at most 9 wrong pixels |
| release | `build/amw/reltest.sh` | release build, xdelta against the clean ROM, apply + byte compare, then an emulator boot |

## Commands

Run from `tools/rh/amoled/` (Python 3.13, `pip install -r kit/requirements.txt`). Every command is a dry run unless
`--write` is given; with `--write` the output goes to a **new** run folder `source/build/amoled_kit/<time>-<command>/`
with `run.json` (input and output SHA-256 hashes, settings, tool versions). `source/build/amoled_kit/CHANGELOG.jsonl`
lists every run.

```
python -m kit check-data                    # parse + validate + byte-identical round trip of RHA4 / RHPG (read only)
python -m kit manifest --proof ../../../build/amw/p16.txt --write
python -m kit preview 40_party_menu --write
python -m pytest -q -p no:cacheprovider kit   # tests
```

- `manifest`: `manifest.json` + `manifest.csv`, one record per owner picture (each picture is one state; pressed /
  cursor / normal stay separate records): preview hashes, size, mode, transparency, pixels not on the DS colour grid,
  matched capture, screen fingerprint, the palette resources seen on the screen (archive path, member, block, pixel
  count, from the colour-ID capture), learned entries / tile patches / index patches, learner-grouped related states,
  proof result, status (`proven`, `implemented`, `mapped`, `unresolved`, `own-code`), confidence and evidence.
- `preview PICTURE`: `sheet.png` = owner Default | owner AMOLED | DS-quantised | AMOLED build frame | quantised with
  wrong pixels in red; `heatmap.png` (red brightness = distance in DS levels, cyan = transparency differs);
  `quantised.png`; `report.json` with every wrong pixel's coordinates and wanted / shown DS levels.

## Colour rules

- DS 2D colours are BGR555. Emulator captures are `level << 3`, read back exactly with `>> 3`.
- The owner's previews were painted in an editor: a channel that is a multiple of 8 is read exactly, any other value
  rounds to the nearest of 32 levels (`dscolor.chan_owner`). `quantised.png` shows exactly what the DS can display.
- Palette-constrained matching is the learner's job: per palette entry (shared by many tiles and screens), never per
  pixel. Previews are fully opaque (checked: all 74 have alpha 255 everywhere); transparency is still compared
  separately from colour.

## Safety

- Writes only inside a new run folder under the staging root; refuses the previews, the clean ROM, `data/`, `tools/`,
  `src/`, `rhtheme/`, `include/`, `base/`, `armhooks/` and `build/amw/` (tests prove it).
- Never overwrites (files opened exclusively; a second run gets a new folder).
- Unknown or truncated data files raise `FormatError`; nothing is skipped silently.
- Rollback: the kit changes nothing in the project, so there is nothing to roll back; delete a run folder to discard
  it. Theme changes themselves go through git (`tools/rh/amoled/*.json`, `learn.py`) and are reverted with git.

## Adding another menu or button state

1. Put the owner's Default and AMOLED pictures (same name, 256x384) in the two screenshot folders.
2. Add a capture route for the screen to `build/amw/caps_all.sh` (save + inputs; colour-ID and AMOLED modes) and a
   recording to `proofcaps.sh` / `proofcaps_b.sh`.
3. Learn, build, record, prove (`relearn.sh`, then `proof3.py PICTURE`); check with `python -m kit preview PICTURE`.
