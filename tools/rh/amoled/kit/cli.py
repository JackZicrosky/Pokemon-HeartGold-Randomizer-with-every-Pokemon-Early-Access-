"""amoled-kit: inventory, data checks and comparison images for the AMOLED theme. Dry run unless --write.

  python -m kit manifest [--proof FILE] [--write]   preview -> native resource manifest (JSON + CSV)
  python -m kit check-data                          parse, validate and round-trip the theme's data files (read only)
  python -m kit preview PICTURE [--write]           original | owner preview | DS-quantised | AMOLED build | diff
(run from tools/rh/amoled). Outputs go to a new run folder under source/build/amoled_kit/; see README.md.
"""
from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path

from PIL import Image

from . import datafiles, imaging, manifest, paths
from .safety import Run, sha256

log = logging.getLogger("amoled-kit")


def cmd_manifest(a) -> int:
    run = Run("manifest", {"proof": a.proof}, dry=not a.write)
    proof = Path(a.proof) if a.proof else None
    rows = manifest.build(proof)
    for r in rows:
        run.inputs[r["preview_amoled"]] = r["preview_amoled_sha256"]
    run.write_json("manifest.json", rows)
    run.write_bytes("manifest.csv", manifest.to_csv(rows).encode())
    st = {}
    for r in rows:
        st[r["status"]] = st.get(r["status"], 0) + 1
    print("pictures:", len(rows), "by status:", st)
    for r in rows:
        if r["status"] == "unresolved":
            print("  unresolved:", r["picture"], "-", r["evidence"])
    return _done(run)


def cmd_check(a) -> int:
    bad = 0
    pal, pages = paths.theme_data()
    for p, parse, write, validate in ((pal, datafiles.parse_rha4, datafiles.write_rha4, datafiles.validate_rha4),
                                      (pages, datafiles.parse_rhpg, datafiles.write_rhpg, datafiles.validate_rhpg)):
        if not p.exists():
            print("MISSING", p, "(build the ROM first)")
            bad += 1
            continue
        buf = p.read_bytes()
        try:
            f = parse(buf)
        except datafiles.FormatError as e:
            print("FORMAT ERROR", p.name, e)
            bad += 1
            continue
        same = write(f) == buf
        probs = validate(f)
        print("%-22s sha256 %s  round trip %s  %s" % (p.name, sha256(p)[:16], "byte-identical" if same else "DIFFERS",
                                                      "valid" if not probs else "; ".join(probs[:5])))
        bad += (not same) + bool(probs)
    return 1 if bad else 0


def best_frame(cap: str, owner_pic: Path) -> tuple[Path | None, imaging.Diff | None]:
    d = paths.amw() / "rec" / f"am_px_{cap}"
    if not (d / "index.json").exists():
        still = paths.amw() / "caps" / f"am_{cap}.png"
        return (still, imaging.compare(owner_pic, still)) if still.exists() else (None, None)
    frames = sorted(set(json.loads((d / "index.json").read_text())["frames"]))
    best = None
    for f in frames:
        df = imaging.compare(owner_pic, d / f)
        if best is None or df.wrong < best[1].wrong:
            best = (d / f, df)
    return best


def cmd_preview(a) -> int:
    pic = a.picture if a.picture.endswith(".png") else a.picture + ".png"
    pa, pd = paths.previews_amoled() / pic, paths.previews_default() / pic
    if not pa.exists():
        print("no such preview:", pa)
        return 2
    matches = json.loads((paths.amw() / "matches.json").read_text())
    cap = matches.get(pic)
    run = Run("preview", {"picture": pic, "capture": cap}, dry=not a.write)
    for p in (pa, pd):
        if p.exists():
            run.note_input(p)
    q = imaging.quantised(pa)
    qd = imaging.compare(pa, pa, ref_owner=True, cand_owner=False)     # preview vs its own raw 8-bit (off-grid px)
    frame, diff = best_frame(cap, pa) if cap else (None, None)
    images = [Image.open(pd).convert("RGB") if pd.exists() else Image.new("RGB", imaging.SCREEN),
              Image.open(pa).convert("RGB"), q]
    report = {"picture": pic, "capture": cap, "off_grid_px_in_preview": len(qd.colour)}
    if frame is not None:
        run.note_input(frame)
        images += [Image.open(frame).convert("RGB"), imaging.overlay(q, diff)]
        run.write_image("heatmap.png", imaging.heatmap(diff))
        report.update({"amoled_frame": str(frame), "wrong_px": diff.wrong, "percent_ok": round(diff.percent_ok(), 4),
                       "alpha_diffs": len(diff.alpha),
                       "wrong": [{"x": x, "y": y, "want": p, "got": g} for x, y, p, g in diff.colour[:5000]]})
        print("%s: best AMOLED frame %s, %d wrong px (%.4f%%)" % (pic, frame.name, diff.wrong, diff.percent_ok()))
    else:
        print(pic, ": no AMOLED capture of this screen yet")
    run.write_image("sheet.png", imaging.sheet(images))
    run.write_image("quantised.png", q)
    run.write_json("report.json", report)
    return _done(run)


def _done(run: Run) -> int:
    rec = run.finish()
    if run.dry:
        print("dry run: would write", len(rec["planned"]), "files into", run.folder, "(add --write)")
    else:
        print("wrote", len(rec["outputs"]), "files into", run.folder)
    return 0


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    ap = argparse.ArgumentParser(prog="amoled-kit", description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("manifest")
    m.add_argument("--proof", help="proof3.py output to take proven/wrong counts from")
    m.add_argument("--write", action="store_true")
    sub.add_parser("check-data")
    p = sub.add_parser("preview")
    p.add_argument("picture")
    p.add_argument("--write", action="store_true")
    a = ap.parse_args(argv)
    return {"manifest": cmd_manifest, "check-data": cmd_check, "preview": cmd_preview}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
