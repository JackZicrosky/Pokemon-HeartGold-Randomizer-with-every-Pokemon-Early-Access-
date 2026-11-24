"""Project locations (all derived from this file's place in the repo; override with environment variables)."""
from __future__ import annotations

import os
import platform
from pathlib import Path

SOURCE = Path(os.environ.get("RH_SOURCE", Path(__file__).resolve().parents[4]))   # .../HeartGold/source
HG = SOURCE.parent


def previews_amoled() -> Path:
    return Path(os.environ.get("RH_PREVIEWS_AMOLED", HG / "UI screenshots - AMOLED"))


def previews_default() -> Path:
    return Path(os.environ.get("RH_PREVIEWS_DEFAULT", HG / "UI screenshots"))


def clean_rom() -> Path:
    return HG / "Pokemon HeartGold (USA) v1.0.nds"


def amw() -> Path:
    """The emulator test area (captures, recordings, proof): source/build/amw."""
    return Path(os.environ.get("RH_AMW", SOURCE / "build" / "amw"))


def theme_data() -> tuple[Path, Path]:
    return SOURCE / "data" / "rh" / "rh_amoled.bin", SOURCE / "data" / "rh" / "rh_amoled_pages.bin"


def learned() -> tuple[Path, Path]:
    d = Path(os.environ.get("RH_LEARNED_DIR", SOURCE / "tools" / "rh" / "amoled"))
    return d / "learned.json", d / "screens.json"


def staging_root() -> Path:
    return Path(os.environ.get("RH_KIT_STAGING", SOURCE / "build" / "amoled_kit"))


def protected() -> list[Path]:
    """Never written by the kit."""
    return [previews_amoled(), previews_default(), clean_rom(), SOURCE / "data", SOURCE / "tools", SOURCE / "src",
            SOURCE / "rhtheme", SOURCE / "include", SOURCE / "base", SOURCE / "armhooks", amw()]


def versions() -> dict:
    import PIL
    return {"python": platform.python_version(), "pillow": PIL.__version__}
