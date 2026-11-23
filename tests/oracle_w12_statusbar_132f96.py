#!/usr/bin/env python3
"""Oracle for the w12_statusbar_132f96 port of hermes_cli/status_bar_git.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration).  For each case a real temp git dir with
the fixture HEAD is materialised and the LIVE current_git_branch() result is
compared to the C label computation.
"""
import importlib
import importlib.util
import json
import os
import shutil
import sys
import tempfile

QUARRY = "C:/Users/eman5/hermes-agent"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
importlib.import_module("hermes_cli")            # parent-package bootstrap
_SPEC = importlib.util.spec_from_file_location(
    "_w12_statusbar_live", QUARRY + "/hermes_cli/status_bar_git.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)


def want(r):
    fn = r.get("func")
    if fn != "branch_label":
        return None
    raw = r.get("head", "")
    d = tempfile.mkdtemp(prefix="w12sbg_")
    try:
        gd = os.path.join(d, ".git")
        os.makedirs(gd, exist_ok=True)
        with open(os.path.join(gd, "HEAD"), "w", encoding="utf-8") as fh:
            fh.write(raw)
        live._cache.clear()                       # no stale per-dir entry
        return live.current_git_branch(d)
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if r.get("func") is None:
            continue
        n += 1
        got = r.get("out")
        try:
            expected = want(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case %d: live raised %r" % (r.get("func"), n, exc))
            continue
        if got != expected:
            mism += 1
            if mism <= 12:
                print("MISMATCH %s case %d C=%r PY=%r" % (r.get("func"), n, got, expected))
    print("ORACLE w12_statusbar_132f96: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
