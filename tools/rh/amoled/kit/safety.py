"""Where the kit may write, hashes, and the change log.

Rules (enforced, not just documented):
- the kit only ever writes into a run folder under the staging root (default source/build/amoled_kit/);
- a run folder is new and uniquely named (timestamp + counter); an existing one is never reused or overwritten;
- nothing is written under a protected path (the owner's previews, the clean ROM, the source tree outside build/);
- without --write nothing is written at all (dry run): the plan of files is printed instead.
"""
from __future__ import annotations

import datetime
import hashlib
import json
import os
from dataclasses import dataclass, field
from pathlib import Path

from . import paths


class UnsafeWrite(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _inside(p: Path, root: Path) -> bool:
    try:
        p.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def check_target(p: Path, staging: Path) -> None:
    if not _inside(p, staging):
        raise UnsafeWrite(f"{p} is outside the staging root {staging}")
    for prot in paths.protected():
        if _inside(p, prot) or p.resolve() == prot.resolve():
            raise UnsafeWrite(f"{p} is inside protected {prot}")


@dataclass
class Run:
    """One kit run. dry=True: record the plan, write nothing."""
    command: str
    settings: dict
    dry: bool = True
    staging: Path = field(default_factory=paths.staging_root)
    folder: Path | None = None
    inputs: dict[str, str] = field(default_factory=dict)
    outputs: dict[str, str] = field(default_factory=dict)
    planned: list[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        base = self.staging / f"{stamp}-{self.command}"
        k, cand = 0, base
        while cand.exists():
            k += 1
            cand = Path(f"{base}-{k}")
        self.folder = cand
        check_target(self.folder, self.staging)

    def note_input(self, p: Path) -> None:
        self.inputs[str(p)] = sha256(p)

    def write_bytes(self, name: str, data: bytes) -> Path:
        target = self.folder / name
        check_target(target, self.staging)
        if self.dry:
            self.planned.append(str(target))
            return target
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            raise UnsafeWrite(f"{target} exists (outputs are never overwritten)")
        with open(target, "xb") as f:
            f.write(data)
        self.outputs[str(target)] = hashlib.sha256(data).hexdigest()
        return target

    def write_image(self, name: str, im) -> Path:
        import io
        buf = io.BytesIO()
        im.save(buf, "PNG")          # (PNG writing is deterministic for the same pixels and Pillow version)
        return self.write_bytes(name, buf.getvalue())

    def write_json(self, name: str, obj) -> Path:
        return self.write_bytes(name, (json.dumps(obj, indent=1, sort_keys=True) + "\n").encode())

    def finish(self) -> dict:
        rec = {"command": self.command, "settings": self.settings, "dry_run": self.dry,
               "folder": str(self.folder), "inputs": self.inputs, "outputs": self.outputs,
               "planned": self.planned, "time": datetime.datetime.now().isoformat(timespec="seconds"),
               "versions": paths.versions()}
        if not self.dry:
            self.write_json("run.json", rec)
            self.staging.mkdir(parents=True, exist_ok=True)
            with open(self.staging / "CHANGELOG.jsonl", "a", encoding="utf-8") as f:
                f.write(json.dumps({k: rec[k] for k in ("time", "command", "folder", "settings")}) + "\n")
        return rec
