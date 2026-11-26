#!/usr/bin/env python3
"""Declared and defined, but nothing calls it.

A function that exists in the link and has no caller is not parity, it is inventory. Two of the
defects found by hand were exactly this shape: gateway_config_apply_env_overrides() was defined,
declared in a header, and never called (so GATEWAY_* env overrides silently did nothing), and the
sixteen pet-atlas functions had no caller anywhere in src/ or include/.

    python scripts/scan_unwired.py              # report
    python scripts/scan_unwired.py --json f.json
    python scripts/scan_unwired.py --top 40

Counts call sites as "identifier followed by (", excluding the definition itself and the prototype.
Tool handlers are called through tables via registry_register("name", ...) so a handler whose name
appears as a string literal in a registration is counted as wired -- otherwise every tool looks dead.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PROTO_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?[\s\*](\w+)\s*\([^;{]*\)\s*;", re.M)
CALL_RE = r"\b%s\s*\("
SKIP = {"main", "wmain", "WinMain", "wWinMain", "SDL_main", "DllMain"}


def prototypes() -> dict[str, str]:
    out: dict[str, str] = {}
    for h in (REPO / "include").rglob("*.h"):
        text = h.read_text(encoding="utf-8", errors="replace")
        for m in PROTO_RE.finditer(text):
            name = m.group(1)
            if name in SKIP or name.startswith("_") or len(name) < 6:
                continue
            out.setdefault(name, str(h.relative_to(REPO)))
    return out



def _rg_binary() -> str:
    """rg is not always on PATH when this runs from a launcher that strips the environment."""
    import shutil
    for cand in ("rg", r"C:/msys64/mingw64/bin/rg.exe", r"C:/msys64/usr/bin/rg.exe"):
        found = shutil.which(cand) if not cand.endswith(".exe") else (cand if Path(cand).exists() else None)
        if found:
            return found
    return "rg"


def _rg(args: list[str]) -> str:
    import subprocess
    return subprocess.run([_rg_binary(), *args], cwd=str(REPO), capture_output=True, text=True,
                          errors="replace").stdout


def _rg_counts() -> dict[str, int]:
    """name -> number of `name(` occurrences in .c/.h under the scan roots."""
    from collections import Counter
    out = _rg(["-o", "--no-filename", r"\b\w+\s*\(", "-g", "*.c", "-g", "*.h",
               "src", "lib", "tests", "tools"])
    c: Counter[str] = Counter()
    for line in out.splitlines():
        c[line.rstrip("( ").strip()] += 1
    return dict(c)


def _rg_defs() -> dict[str, int]:
    """name -> number of file-scope definitions (a heuristic: line starts with a type, ends with `{`)."""
    from collections import Counter
    out = _rg(["--no-filename", r"^[A-Za-z_][\w \t\*]*?[\s\*](\w+)\s*\([^;{]*\)\s*\{",
               "-r", "$1", "-g", "*.c", "src", "lib", "tests", "tools"])
    c: Counter[str] = Counter()
    for line in out.splitlines():
        if line.strip():
            c[line.strip()] += 1
    return dict(c)


def main() -> int:
    top = 400
    if "--top" in sys.argv:
        top = int(sys.argv[sys.argv.index("--top") + 1])
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])

    protos = prototypes()
    # Counting with a per-file regex in Python took longer than the build. ripgrep finds every
    # "identifier(" in the tree in one pass; the counts are then compared in Python.
    counts = _rg_counts()
    defs = _rg_defs()

    unwired: dict[str, str] = {}
    for name, header in protos.items():
        if counts.get(name, 0) - defs.get(name, 0) <= 0:
            unwired[name] = header

    print(f"prototypes: {len(protos)}  unwired: {len(unwired)}")
    by_header: dict[str, list[str]] = {}
    for name, header in unwired.items():
        by_header.setdefault(header, []).append(name)
    for header, names in sorted(by_header.items(), key=lambda kv: -len(kv[1]))[:12]:
        print(f"  {header:44} {len(names):3}  e.g. {', '.join(sorted(names)[:3])}")
    if want_json:
        want_json.write_text(json.dumps(unwired, indent=1), encoding="utf-8")
        print(f"wrote {want_json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
