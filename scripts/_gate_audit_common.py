"""Shared instrument for the gate-audit scripts: the PORT's symbol set.

Both audits used to nm `tests/watercup/build/_all_objects.a` directly. That
archive is only rebuilt by the gate runner, so after a normal `make` it is STALE:
symbols defined in freshly built objects are absent, and the audit then reports a
harness as "missing"/"shadows" nothing. Measured: `backup_should_exclude` is `T`
in src/hermes_cli/backup.o and was reported absent because the archive predated it.

This rebuilds the archive when any member object is newer, exactly like
wc_run._watercup_object_archive(), and caches the (mtime,size) -> symbols JSON.
"""
from __future__ import annotations

import glob
import json
import os
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / "tests" / "watercup" / "build"
ARCHIVE = BUILD / "_all_objects.a"


def _members() -> list[str]:
    objs = sorted(glob.glob(str(REPO / "src" / "**" / "*.o"), recursive=True)
                  + glob.glob(str(REPO / "lib" / "**" / "*.o"), recursive=True))
    return [o for o in objs if os.path.basename(o) != "main.o"]


def ensure_archive() -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    objs = _members()
    newest = max((os.path.getmtime(o) for o in objs), default=0)
    if not (ARCHIVE.exists() and os.path.getmtime(ARCHIVE) >= newest):
        rsp = BUILD / "_ar_members_audit.rsp"
        rsp.write_text("\n".join(o.replace("\\", "/") for o in objs))
        ar = os.environ.get("AR") or "ar"
        subprocess.run([ar, "rcs", str(ARCHIVE), "@" + str(rsp)],
                       capture_output=True, stdin=subprocess.DEVNULL)
    return ARCHIVE


def port_symbols() -> set[str]:
    archive = ensure_archive()
    cache = BUILD / "_port_symbols_audit.json"
    key = [os.path.getmtime(archive), os.path.getsize(archive)]
    try:
        blob = json.loads(cache.read_text())
        if blob.get("key") == key:
            return set(blob["syms"])
    except (OSError, ValueError, KeyError):
        pass
    nm = os.environ.get("NM") or "nm"
    r = subprocess.run([nm, "-g", "--defined-only", str(archive)],
                       capture_output=True, text=True, errors="replace")
    syms = set()
    for line in r.stdout.splitlines():
        p = line.split()
        if len(p) >= 3:
            syms.add(p[2])
    if syms:
        cache.write_text(json.dumps({"key": key, "syms": sorted(syms)}))
    return syms
