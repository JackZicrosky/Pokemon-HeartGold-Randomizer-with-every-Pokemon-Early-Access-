# AMOLED UI Theme: from the owner's edited screenshots to the game

The owner designs the AMOLED theme by editing screenshots of the Default UI (`HeartGold\UI screenshots - AMOLED`,
same file names as `HeartGold\UI screenshots`). The game can't use pictures: it can only swap palettes. This folder
turns the pictures into palettes.

## How it works

1. **Colour-id captures** (test builds only, `include/rh/rh_test.h` `RH_TEST 1`). UI Theme value 3 (poked by the
   capture tool) makes `src/rh_theme.c` replace every palette colour it loads with an id: `slot << 8 | index`, and log
   which archive / file / part each slot is (`gRhPalLog`). A screenshot taken that way says, for every pixel, which
   palette entry it shows.
2. The same screens are captured in Default mode. The capture that looks most like each Default screenshot gives the
   matching colour-id picture.
3. `learn.py`: for every pixel whose palette entry really shows its file colour in the Default screenshot, the owner's
   colour at that pixel is a vote for that entry. Result: `learned.json` (`"narc,file,index": {colour: pixels}`). Owner colours are read back exactly
   when they are emulator values (multiples of 8), else rounded to the DS's 5-bit steps.
4. `tools/rh/gen_rh_amoled.py` builds `data/rh/rh_amoled.bin` (the ROM file `data/rh_amoled.bin`): every palette file
   of the UI archives, with the owner's colours where they are known and the rest filled from them (same original
   colour in the same file / archive / anywhere, or the nearest recoloured colour, shifted). Never inverted. Art
   archives only change where the owner changed them.
5. At runtime (`src/rh_theme.c`), with UI Theme = AMOLED, every palette read (whole files, and the few screens that read
   a handful of colours from a palette file) takes the table's colours.

Extras in `src/rh_theme.c`:

- `sCodeColours`: colours some screens write from code (main menu backdrop / window fill), swapped in RAM while their
  overlay is loaded.
- The randomizer options screen has its own AMOLED palette (`sPalAmoled` in `src/rhmenu/rh_menu.c`), used as soon as
  the UI Theme row says AMOLED.

## Redoing it after new edits

The capture scripts (`cap.py`, `learn.py`, `verify.py`) are test tools; they need the WSL emulator setup
(`/root/dsvenv`, `tools/rh/test/dsharness.py`) and the test saves. `cap.py MODE SAVE TOKENS…` (MODE = `def`, `id` or
`am`) plays a key / touch script and saves `caps/MODE_NAME.png` at every `cap:NAME`. Then:

```
python3 learn.py learned.json && cp learned.json tools/rh/amoled/learned.json
python3 tools/rh/gen_rh_amoled.py && make
python3 verify.py       # owner picture | game | difference for every screen
```

## Known limits

- One palette entry has one colour. Where the owner gave the same entry different colours on different screens, the
  colour most pixels used wins (unless the screens belong to one overlay: contexts).
- Things drawn by blending (the dimmed background behind the party menu, the dimmed battle text box, the Mart menu over
  the field menu) follow from the normal colours, not from the owner's dimmed versions.
- The battle move buttons' type colours reach the screen without going through a palette read (still Default).
