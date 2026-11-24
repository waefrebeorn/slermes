#!/usr/bin/env python3
"""Differential oracle for persistence/core/context.py (gate w21_context_1bf504).

The live module imports only `os`, `dataclasses` and `pathlib`, so it is
imported as-is: `_canonical_dir`, `canonical_home_key` and `ProfileScope` are
the real implementations, and the three path steps they build on (pathlib's
Path text, `expanduser`, `resolve`, `normcase`) run for real on this host.

Paths are compared by `str()`: the C port carries a path as text. `$HOME` and
`$SLERMES_HOME` in a fixture name the isolated directories the harness is run
with, and both sides create `prof/` and `prof/sub/` under HOME first so the
"folder already on disk" cases resolve a real directory.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "core" / "context.py"
GATE = "w21_context_1bf504"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("persistence_core_context_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["persistence_core_context_live"] = live
_spec.loader.exec_module(live)


def expand(text):
    """The same two placeholders the harness expands, in the same order."""
    if not isinstance(text, str):
        text = "" if text is None else str(text)
    for prefix, var in (("$SLERMES_HOME", "SLERMES_HOME"), ("$HOME", "HOME")):
        if text.startswith(prefix):
            return os.environ.get(var, "") + text[len(prefix):]
    return text


def ensure_dirs():
    home = os.environ.get("HOME")
    if not home:
        return
    for sub in ("prof", "prof/sub"):
        try:
            os.makedirs(os.path.join(home, *sub.split("/")), exist_ok=True)
        except OSError:
            pass


def err_of(exc):
    return {"status": "error", "type": type(exc).__name__, "message": str(exc)}


def scope_of(profile_dir, name):
    scope = live.ProfileScope(profile_dir=profile_dir, name=name)
    return {"status": "ok", "profile_dir": str(scope.profile_dir),
            "name": str(scope.name), "hermes_home_key": scope.hermes_home_key}


def expected(r):
    f = r.get("func")
    path = expand(r.get("path", ""))
    if f == "expand_user":
        return str(Path(path).expanduser())
    if f == "canonical_dir":
        return str(live._canonical_dir(Path(path)))
    if f == "canonical_home_key":
        return live.canonical_home_key(Path(path))
    if f == "scope":
        try:
            return scope_of(expand(r.get("profile_dir", "")), r.get("name", ""))
        except Exception as exc:  # noqa: BLE001 - the harness shapes the error
            return err_of(exc)
    if f == "scope_equal":
        try:
            left = live.ProfileScope(profile_dir=expand(r.get("dir_a", "")), name=r.get("name_a", ""))
            right = live.ProfileScope(profile_dir=expand(r.get("dir_b", "")), name=r.get("name_b", ""))
            return {"status": "ok", "equal": left == right}
        except Exception as exc:  # noqa: BLE001
            return err_of(exc)
    raise AssertionError("unknown func %r" % (f,))


def main():
    ensure_dirs()
    cases = mism = 0
    wanted = {"expand_user", "canonical_dir", "canonical_home_key", "scope", "scope_equal"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
