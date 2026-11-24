"""The preview -> native resource manifest, one record per owner picture (each picture is one screen state).

Every relationship comes from evidence, never from file names:
- matches.json: which emulator capture shows the screen of each picture (picked by pixel comparison with the owner's
  Default picture, learn.py);
- caps/id_<capture>.json + .png: the colour-ID capture. With uiTheme 3 the game draws every palette entry as a unique
  colour, so each pixel names the palette load it came from: archive (a/0/x/y), member, 256-colour block, entry;
- tools/rh/amoled/screens.json: what the theme learned for that screen (fingerprint, entries, tile / index patches).
Pictures without a matched capture are listed as unresolved with what is missing.
"""
from __future__ import annotations

import collections
import csv
import io
import json
import re
from pathlib import Path

from PIL import Image

from . import dscolor, imaging, paths
from .safety import sha256

ROW_FIELDS = ["picture", "state", "status", "confidence", "preview_amoled", "preview_amoled_sha256",
              "preview_default", "preview_default_sha256", "width", "height", "mode", "opaque", "off_grid_px",
              "kind", "capture", "screen_sig", "palette_resources", "learned_entries", "tile_patches",
              "index_patches", "related", "proof_wrong_px", "evidence"]


def narc_path(n: int) -> str:
    return "a/%d/%d/%d" % (n // 100, n // 10 % 10, n % 10)


def palette_resources(cap: str, amw: Path) -> tuple[list[dict], int, int]:
    """Palette loads seen on a capture with the pixels each one colours (from the colour-ID capture)."""
    jf, pf = amw / "caps" / f"id_{cap}.json", amw / "caps" / f"id_{cap}.png"
    if not jf.exists() or not pf.exists():
        return [], 0, 0
    log = json.loads(jf.read_text())
    count = collections.Counter()
    other = 0                      # pixels of palettes loaded outside the logged loaders (Pokemon icons...)
    for p in imaging.pixels(Image.open(pf).convert("RGB")):
        c = dscolor.pack(dscolor.rgb5_capture(p))
        slot = c >> 8
        if slot < min(64, log["count"]):
            count[slot] += 1
        else:
            other += 1
    out = collections.defaultdict(int)
    for slot, n in count.items():
        out[(log["narc"][slot], log["file"][slot] & 0xFFF, log["file"][slot] >> 12)] += n
    res = [{"archive": narc_path(a), "narc": a, "member": m, "block": b, "pixels": n}
           for (a, m, b), n in sorted(out.items())]
    return res, sum(count.values()), other


def read_proof(path: Path | None) -> dict[str, tuple[int, bool]]:
    """picture -> (wrong pixels, proven) from proof3.py output."""
    if path is None or not path.exists():
        return {}
    out = {}
    for line in path.read_text(errors="replace").splitlines():
        m = re.match(r"(\S+\.png)\s+\S+\s+situation\s+\d+\s+wrong\s+(\d+)\s+\S+%\s*(PROVEN)?", line)
        if m:
            out[m.group(1)] = (int(m.group(2)), bool(m.group(3)))
    return out


def build(proof_file: Path | None = None) -> list[dict]:
    amw = paths.amw()
    matches = json.loads((amw / "matches.json").read_text())
    screens = json.loads(paths.learned()[1].read_text())
    proof = read_proof(proof_file)
    pics = sorted(p.name for p in paths.previews_amoled().glob("*.png"))
    # screens that share a learned family / version marker (the same game screen in another state): evidence for
    # 'related' from the learner, which grouped them by their tile maps, not by name
    by_base = collections.defaultdict(list)
    for name, sc in screens.items():
        by_base[sc.get("base", sc.get("sig"))].append(name)
    rows = []
    for pic in pics:
        pa = paths.previews_amoled() / pic
        pd = paths.previews_default() / pic
        inf = imaging.info(pa)
        cap = matches.get(pic)
        sc = screens.get(pic, {})
        res, idpx, other = palette_resources(cap, amw) if cap else ([], 0, 0)
        related = sorted(set(by_base.get(sc.get("base", sc.get("sig")), [])) - {pic}) if sc else []
        wrong, proven = proof.get(pic, (None, False))
        if cap and sc:
            status = "proven" if proven else "implemented"
            conf = "high" if idpx else "medium"
            ev = (f"capture {cap} matched by pixel comparison; colour-ID capture: {idpx} px traced to palette files, "
                  f"{other} px to palettes loaded outside the logged loaders (sprites such as Pokemon icons, 3D)")
        elif cap:
            status, conf, ev = "mapped", "medium", f"capture {cap}; nothing learned yet"
        else:
            status, conf = "unresolved", "none"
            ev = ("no emulator capture of this screen state matched yet: needs a scripted route to the screen "
                  "(save + inputs) and a colour-ID capture")
            if re.match(r"(0[4-9]|1\d|20)_options", pic) or pic.startswith("01_"):
                ev = "drawn by the romhack's own code (src/rh_*), not by game graphics: themed in code, not here"
                status, conf = "own-code", "high"
        state = "pressed" if "pressed" in pic else ("cursor" if "cursor" in pic else "normal")
        rows.append({
            "picture": pic, "state": state, "status": status, "confidence": conf,
            "preview_amoled": str(pa), "preview_amoled_sha256": sha256(pa),
            "preview_default": str(pd) if pd.exists() else "", "preview_default_sha256": sha256(pd) if pd.exists() else "",
            "width": inf.size[0], "height": inf.size[1], "mode": inf.mode, "opaque": inf.opaque,
            "off_grid_px": inf.off_grid,
            "kind": "composite: both DS screens (BG layers, sprites, runtime text, blending; 3D on some screens)",
            "capture": cap or "", "screen_sig": "%#010x" % sc["sig"] if "sig" in sc else "",
            "palette_resources": res, "learned_entries": len(sc.get("cols", {})),
            "tile_patches": len(sc.get("vpatch", [])), "index_patches": len(sc.get("ipatch", [])),
            "related": related, "proof_wrong_px": wrong, "evidence": ev,
        })
    return rows


def to_csv(rows: list[dict]) -> str:
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=ROW_FIELDS, lineterminator="\n")
    w.writeheader()
    for r in rows:
        r2 = dict(r)
        r2["palette_resources"] = ";".join("%s[%d]b%d:%dpx" % (p["archive"], p["member"], p["block"], p["pixels"])
                                           for p in r["palette_resources"])
        r2["related"] = ";".join(r["related"])
        w.writerow(r2)
    return buf.getvalue()
