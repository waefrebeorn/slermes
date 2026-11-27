"""Shared instrument for the gate-audit scripts: the PORT's symbol set.

The symbol set is read from EVERY built object, not from
`tests/watercup/build/_all_objects.a`. That archive is a convenience: GNU ar
stores members by basename, so two objects with the same basename collapse and
the archive silently under-reports the port. It is also stale unless the gate
runner rebuilt it (measured: `backup_should_exclude` is `T` in backup.o and was
reported absent).

nm is run in batches over the object list and the results merged, with a cache
keyed on (object count, newest mtime).
"""
from __future__ import annotations

import glob
import json
import os
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / "tests" / "watercup" / "build"
CACHE = BUILD / "_port_symbols_objs.json"


def _members() -> list[str]:
    objs = sorted(glob.glob(str(REPO / "src" / "**" / "*.o"), recursive=True)
                  + glob.glob(str(REPO / "lib" / "**" / "*.o"), recursive=True))
    return [o for o in objs if os.path.basename(o) != "main.o"]


def port_symbols() -> set[str]:
    objs = _members()
    if not objs:
        return set()
    key = [len(objs), max(os.path.getmtime(o) for o in objs)]
    try:
        blob = json.loads(CACHE.read_text())
        if blob.get("key") == key:
            return set(blob["syms"])
    except (OSError, ValueError, KeyError):
        pass
    nm = os.environ.get("NM") or "nm"
    syms: set[str] = set()
    for i in range(0, len(objs), 200):
        chunk = objs[i:i + 200]
        r = subprocess.run([nm, "-g", "--defined-only"] + chunk,
                           capture_output=True, text=True, errors="replace")
        for line in r.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3:
                syms.add(parts[2])
    if syms:
        BUILD.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps({"key": key, "syms": sorted(syms)}))
    return syms
