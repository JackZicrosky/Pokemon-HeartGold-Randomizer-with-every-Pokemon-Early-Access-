# Pokémon HeartGold Randomizer with every Pokémon (Early Access)

**HeartGold Expansion Randomizer** is a Pokémon HeartGold romhack with **all 1025 Pokémon and their forms**, Mega
Evolution, modern moves, abilities and items, and a **full randomizer built into the game**. You choose your
randomizer settings on a screen that opens when you start a New Game, before Professor Oak's talk, and the game
randomizes itself while you play: no separate randomizer program needed.

It is the HeartGold version of the **FireRed Expansion Randomizer** (same options, same style), built on
[hg-engine](https://github.com/BluRosie/hg-engine).

> **Early Access:** many randomizer options already work, some are still being built. See [Status](#status).

---

## How to play (patching the game yourself)

This project only shares a **patch**, never the game itself. You need your own copy of the game.

**What you need**

1. A **Pokémon HeartGold (USA) v1.0** ROM (`.nds`) dumped from your own cartridge.
   Its SHA-1 checksum should be `4fcded0e2713dc03929845de631d0932ea2b5a37`.
2. The patch file `HeartGold Expansion Randomizer vX.Y.Z.xdelta` from this repository's
   [**Releases**](../../releases) page (download the newest one).
3. **`HeartGold Randomizer Patcher.html`**, from the same Releases page (or
   [here in the repository](tools/rh/release/HeartGold%20Randomizer%20Patcher.html)). It's a one-page patcher that
   runs in your web browser, works offline and uploads nothing. Other `.xdelta` patchers work too.

**Steps (with the HeartGold Randomizer Patcher)**

1. Double-click `HeartGold Randomizer Patcher.html`: it opens in your web browser (Chrome, Edge, Firefox…).
2. **Choose ROM…**: your `Pokemon HeartGold (USA) v1.0.nds`. (The patcher checks that it's the right, unpatched
   game.)
3. **Choose patch…**: the `.xdelta` file you downloaded.
4. Click **Patch the game**. A few seconds later your browser saves the patched game (a new `.nds` file, about
   256 MB).
5. Open the patched `.nds` in a DS emulator such as **melonDS** or **DeSmuME** (or on a flashcart).

**Steps (with Rom Patcher JS)**

1. Open <https://www.marcrobledo.com/RomPatcher.js/>.
2. Next to **ROM file**, choose your `Pokemon HeartGold (USA) v1.0.nds`.
3. Next to **Patch file**, choose the `.xdelta` file you downloaded.
4. Click **Apply patch**. Your browser saves the patched game (a new `.nds` file, about 256 MB).
5. Open the patched `.nds` in a DS emulator such as **melonDS** or **DeSmuME** (or on a flashcart).

**Steps (with xdelta UI)**

1. Open xdelta UI and go to the **Patch** tab.
2. **Patch:** choose the `.xdelta` file. **Source File:** choose your HeartGold ROM.
   **Output File:** choose where to save the new game, e.g. `HeartGold Randomizer.nds`.
3. Click **Patch**, then open the new `.nds` in your emulator.

**Tips**

* Keep your original ROM: the patch always needs the clean, unpatched game.
* A newer version of the romhack may not load saves from an older version. Finish a run on one version, or start
  a new game.
* Choose **New Game**: the randomizer settings screen opens first. Press **SELECT** for the section menu,
  **L/R** to change section, **Left/Right** to change an option, **START** to begin.
* Your randomizer **Presets** stay on the cartridge (even after **Reset The Run**).

---

## Features

**Base game**

* All 1025 Pokémon and their regional / alternate forms, Mega Evolution, modern moves, abilities and items
  (from hg-engine).
* **Mom** gives you the **Mega Ring** after she explains the Pokégear. The Pokégear comes with its Map Card.
* You can **run** everywhere from the very start. The **Apricorn Box** is in your bag from the start; the Cherrygrove
  Guide Gent's tour and the Route 30 Apricorn Box scene are skipped.
* **Kurt** sells his surplus Poké Balls: every kind of Ball (and, without shop randomization, his one Master Ball).
* An **evolution item seller** in every Poké Mart, and special counters in the **Goldenrod and Celadon Dept. Stores**
  (battle gear, form items, Mega Stones, training items, Berries and snacks).
* Key items:
  * **Rando Settings** (a New Nintendo 3DS XL): opens the randomizer settings during a run, to change them or
    **Reset The Run** (erases your save, keeps your presets).
  * **HM Kit**: counts as every HM you own whose Badge you have; no Pokémon needs to know the move. Walk up to a
    tree, rock, boulder, whirlpool, waterfall, rock wall or water, or Fly from the bag.
  * With **Nuzlocke Mode**: the **Infinite Candy** (a Rare Candy that never runs out) and the **Healing Kit**
    (heals your whole party anywhere).

**Randomizer** (modelled on the Universal Pokémon Randomizer FVX, same option names and descriptions)

* Seed (type any text: same seed + same options = same game), Battle Mechanics generation, Pokémon pool.
* Pokémon traits (base stats, types, abilities, evolutions), type effectiveness, starters, static Pokémon, in-game
  trades, move data and movesets, trainer Pokémon, wild Pokémon, TMs/HMs and move tutors, field items, shops,
  misc. tweaks.
* **Settings Codes** to share your settings, and **3 Presets** kept on the cartridge.

## Status

Working and tested in the game: the options screen, presets and Reset The Run; random starters (your rival's too),
trainers, wild Pokémon, movesets, evolutions, field items, shops and Balance Shop Prices; the trade NPCs' text; the
shops and key items above.

Built but not fully tested in the game yet: static Pokémon (gifts, legendaries, roamers), the trades themselves,
TM / tutor moves, egg moves, move data, No Random Phone Calls.

Not finished yet: UI themes, custom player graphics, Pokémon palettes, Battle Style, pickup items, trainer names and
classes, wild held items, the Safari Zone generation selector, the Goldenrod herb shop, your rival keeping the exact
starter he stole for the whole game.

---

## Credits

* **[hg-engine](https://github.com/BluRosie/hg-engine)** by BluRosie and contributors: the engine this romhack is
  built on (Pokémon expansion, forms, Mega Evolution, moves, abilities, items and much more). Full list of hg-engine
  contributors and the resources they credit: [CREDITS.md](CREDITS.md). hg-engine's original README:
  [README_hg-engine.md](README_hg-engine.md).
* **[pret/pokeheartgold](https://github.com/pret/pokeheartgold)**: the HeartGold decompilation, used as the reference
  for the game's code, scripts and data.
* **[Universal Pokémon Randomizer FVX](https://github.com/upr-fvx/universal-pokemon-randomizer-fvx)**: the randomizer
  this romhack's options follow (behaviour, descriptions, balanced shop prices, trainer and static Pokémon lists).
* **HeartGold Generations**: the **Healing Kit** item icon.
* **JackZicrosky**: project owner, design, the **Rando Settings** (New Nintendo 3DS XL) item icon.
* **FireRed Expansion Randomizer** (JackZicrosky): the original version of this romhack, its rainbow **Infinite
  Candy** and **HM Kit** item icons, and the shop lists.
* Pokémon is © Nintendo / Creatures Inc. / GAME FREAK inc. This is a free, non-commercial fan project: it is not
  affiliated with them, and it must never be sold or put behind a paywall (as hg-engine's licence requires).

## Building it yourself (developers)

The source is a fork of hg-engine. Follow hg-engine's setup ([README_hg-engine.md](README_hg-engine.md)), put your
HeartGold ROM in the repository root as `rom.nds`, then run `make`. The output is `test.nds`. The romhack's code is
in `src/rh`, `src/field/rh_*.c`, `src/rhmenu`, and its generators in `tools/rh` (prefixes `rh_` / `RH_`).
