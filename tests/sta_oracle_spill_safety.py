#!/usr/bin/env python3
"""sta_oracle_spill_safety.py — Python twin for tools/spill_safety.py,
mirrored by src/tools/spill_safety.c.

Imports the LIVE Python module via spec_from_file_location (the module only
needs os/stat/pathlib, so it loads standalone). Reads a JSON fixture with
"ops", runs them inside tests/oracle/build/spill_safety_ws/<case>/ (wiped at
start), and emits one JSON line per op plus a final recursive lstat listing,
matching tests/t_port_spill_safety.c exactly.
"""
import json
import os
import stat as stat_mod
import sys
from pathlib import Path

import importlib.util

_DEV_ROOT = Path(__file__).resolve().parents[2]  # hermes-agent-dev
_spec = importlib.util.spec_from_file_location(
    "spill_safety_live", _DEV_ROOT / "tools" / "spill_safety.py"
)
spill = importlib.util.module_from_spec(_spec)
sys.modules["spill_safety_live"] = spill
assert _spec.loader is not None
_spec.loader.exec_module(spill)


def _status_name(fn, *args, **kw):
    """Map Python exception outcomes to the C port's status enum."""
    try:
        fn(*args, **kw)
        return "ok"
    except FileExistsError:
        return "exists"
    except OSError as exc:
        msg = str(exc)
        if "not a directory (symlink?)" in msg:
            return "not_dir"
        if "refusing to overwrite a directory" in msg:
            return "refuse_dir"
        return "io_error"


def _listing(root: Path):
    lines = ["--- listing ---"]
    st = root.lstat()
    lines.append(f".\tdir\t{stat_mod.S_IMODE(st.st_mode):o}")
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(dirnames + filenames):
            p = Path(dirpath) / name
            rel = str(p.relative_to(root))
            s = p.lstat()
            if stat_mod.S_ISLNK(s.st_mode):
                lines.append(f"{rel}\tlink\t{stat_mod.S_IMODE(s.st_mode):o}")
            elif stat_mod.S_ISDIR(s.st_mode):
                lines.append(f"{rel}\tdir\t{stat_mod.S_IMODE(s.st_mode):o}")
            else:
                content = p.read_text(encoding="utf-8", errors="replace") if p.is_file() else ""
                lines.append(f"{rel}\tfile\t{stat_mod.S_IMODE(s.st_mode):o}\t{content}")
    return lines


def main():
    fixture = Path(sys.argv[1]).resolve()
    casebase = fixture.stem
    root = Path("tests/oracle/build/spill_safety_ws") / casebase
    # wipe + recreate the sandbox (same state whichever side runs second)
    import shutil
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)

    with open(fixture) as f:
        cfg = json.load(f)
    out_lines = []
    for c in cfg.get("ops", []):
        kind = c.get("kind")
        p = root / c.get("path", "")
        if kind == "ensure_dir":
            status = _status_name(
                spill.ensure_spill_dir, p, private=c.get("private", True)
            )
        elif kind == "write_text":
            status = _status_name(
                spill.write_text_exclusive, p, c.get("text", ""),
                private=c.get("private", True), overwrite=c.get("overwrite", False),
            )
        elif kind == "open_exclusive":
            def _do():
                fh = spill.open_exclusive(
                    p, private=c.get("private", True),
                    overwrite=c.get("overwrite", False))
                fh.write(c.get("write", ""))
                fh.close()
            status = _status_name(_do)
        elif kind == "symlink":
            try:
                os.symlink(root / c.get("target", ""), root / c.get("link", ""))
                status = "ok"
            except OSError:
                status = "io_error"
        elif kind == "write_raw":
            try:
                (root / c.get("path", "")).write_text(c.get("text", ""))
                status = "ok"
            except OSError:
                status = "io_error"
        else:
            status = "unknown"
        out_lines.append(json.dumps({"kind": kind, "status": status},
                                    ensure_ascii=False, sort_keys=True))
    for line in out_lines:
        print(line)
    listing = _listing(root)
    for line in [listing[0]] + sorted(listing[1:]):
        print(json.dumps({"listing": line}, ensure_ascii=False))


if __name__ == "__main__":
    main()
