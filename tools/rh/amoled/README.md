# AMOLED UI Theme: from the owner's edited screenshots to the game, pixel for pixel

The owner designs the AMOLED theme by editing screenshots of the Default UI (`HeartGold\UI screenshots - AMOLED`,
same file names as `HeartGold\UI screenshots`). Rule: the game must show those pictures exactly; the only allowed
difference is the DS's 5-bit colour depth (a colour like 0d0d0d becomes the nearest DS shade).

The DS draws with palettes, tiles and hardware effects, so the pictures are turned into layers of changes, all only
in the AMOLED theme (Default is never touched):

1. **Palette files** (`data/rh_amoled.bin`, built by `tools/rh/gen_rh_amoled.py`): every palette file of the UI
   archives, replaced as the game loads it (`src/rh_theme.c` ThemePalette / ThemeRead). A palette can have a version
   per *screen context* (the palette loaded just before it), when the owner coloured it differently on different
   screens.
2. **Screens** (`data/rh_amoled_pages.bin`, same generator): a fingerprint of both screens' background maps tells
   which screen is showing (`ScreenSig`, every frame on the main task queue). For a known screen:
   - palette rows another screen loaded become this screen's version (found by their colours in palette RAM and
     the game's palette buffers);
   - **tile patches**: where pixels sharing a palette entry got different colours from the owner on this screen,
     their tile pixels (BG or sprite VRAM, 4bpp) point at a spare entry of the row (undone when the screen
     changes);
   - **palette index patches**: every palette RAM entry the screen shows gets the owner's colour (colours set by
     code, partial reads, etc.), worked back through the hardware dimming / blending (undone when the screen
     changes);
   - **register patches**: where the hardware dimming / blending can't give the owner's colours, it is switched off
     for that screen and the colours are set directly.
3. A few colours the main menu writes from code (`sCodeColours`) and the options screen's own palette
   (`sPalAmoled`, `src/rhmenu/rh_menu.c`).

## How the data is made (test builds only)

- `cap.py MODE SAVE TOKENS…` plays a key / touch script in the emulator (WSL, `/root/dsvenv`,
  `tools/rh/test/dsharness.py`) and saves `caps/MODE_NAME.png` (+ `.json` log and `.vram2` dump: registers with
  scroll from a savestate, palettes, OAM, BG + OBJ VRAM) at every `cap:NAME`. MODE: `def` (Default), `id` (UI Theme
  3: every palette colour replaced by an id, so a pixel tells its palette entry and screen context), `am` (AMOLED).
- `dsrender.py` re-renders a dump (text BGs, sprites, priorities, colour effects): for each pixel its palette RAM
  entry, tile pixel address and effect.
- `learn.py` matches the owner's pictures with the captures and writes `learned.json` (owner colours per palette
  entry and screen context) and `screens.json` (per screen: fingerprint, colours, tile / index / register patches).
- `gen_rh_amoled.py` (run by `make`) builds both data files from them.
- `strict.py` lists, per screen, every pixel the game still shows differently from the owner's picture, by cause.

- Battles: `cap.py def r30b.sav battle ssave:bpre` makes a savestate right as a wild battle starts (`give4:MOVE`
  first gives the lead Pokemon a 4th move); `sload:bpre` then replays exactly the same battle in every mode. The
  emulator clock decides the encounter, so walking into a battle per mode gives different battles. A savestate only
  works with the ROM build that made it: remake it after every build.
- `bdiff.py OWNER CAP`, `why.py OWNER CAP` (wrong pixels grouped by palette entry), `allscore.py` (every screen):
  the AMOLED capture against the owner's picture. `sigpy.py`: the screen fingerprint worked out from a dump.
- More patch kinds: **map patches** (a background cell pointed at a palette row of its own, where the owner coloured
  a tile differently in different places; the fingerprint hashes the maps as the game drew them) and the battle move
  buttons (`BattleMoves` in `src/rh_theme.c`: the per-type colour table of battle overlay 6, and the Check Moves
  buttons' type borders).

Order: id captures → am captures → learn → make → am captures → learn → make … (the index patches use the AMOLED
build's own capture) → strict.

## Known limits (being worked on)

- Screens using affine / extended backgrounds (trainer card) aren't re-rendered, so get no tile / index patches.
- Short button press animations (battle FIGHT / BAG pressed, 67-69) are colours the game computes per frame; they
  aren't matched yet.
- Where a tile is drawn with one palette entry both on a border and inside a button (65 SHIFT), a few hundred pixels
  stay off.
