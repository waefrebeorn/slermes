#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/ffmpeg_utils.py
(gate w22_ffmpegutil_86b841).

`resolve_ffmpeg_executable` is split into the two pieces the fixture can drive
without a real ffmpeg install:

  expand   -- os.path.expandvars(os.path.expanduser(value)), verbatim stdlib.
  resolve  -- the live function with FFMPEG_PATH / LOCALAPPDATA injected through
              os.environ (the environment seam) and `_shared_find_ffmpeg`
              (the repo-wide PATH lookup seam) replaced by the fixture value.

The winget branch is a real scan, so a case may seed trees under LOCALAPPDATA;
both this oracle and the C harness create exactly the "seed" list. LOCALAPPDATA
values may name $SLERMES_HOME / ${SLERMES_HOME} / %SLERMES_HOME%; both sides
expand that with the same stdlib rule before using it, and fast_gates points
SLERMES_HOME at a fresh temp directory per gate run.
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
SRC = QUARRY / "plugins" / "platforms" / "discord" / "ffmpeg_utils.py"
GATE = "w22_ffmpegutil_86b841"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("ffmpeg_utils_live", SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"expand", "resolve"}

# The fixtures that name $SLERMES_HOME (and the seeded winget trees under it)
# only mean anything when the harness runs with the runner's isolated temp home.
if not os.environ.get("SLERMES_HOME") or not os.environ.get("HOME"):
    raise SystemExit("w22_ffmpegutil_86b841 needs SLERMES_HOME/HOME from the runner")


def seed_trees(local_appdata, seed):
    if not local_appdata or not isinstance(seed, list):
        return
    for rel in seed:
        if not isinstance(rel, str) or rel.startswith("/"):
            continue
        target = Path(local_appdata) / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.touch()


def expected(r):
    func = r.get("func")
    if func == "expand":
        return os.path.expandvars(os.path.expanduser(r.get("value") or ""))
    if func != "resolve":
        raise AssertionError("unknown func %r" % (func,))

    raw_local = r.get("local_appdata")
    local_appdata = os.path.expandvars(raw_local) if raw_local else raw_local
    seed_trees(local_appdata, r.get("seed"))

    ffmpeg_path = r.get("ffmpeg_path")
    shared = r.get("shared")
    saved = {name: os.environ.get(name) for name in ("FFMPEG_PATH", "LOCALAPPDATA")}
    orig_shared = live._shared_find_ffmpeg
    try:
        for name, value in (("FFMPEG_PATH", ffmpeg_path), ("LOCALAPPDATA", local_appdata)):
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value
        live._shared_find_ffmpeg = lambda: shared
        return live.resolve_ffmpeg_executable()
    finally:
        live._shared_find_ffmpeg = orig_shared
        for name, value in saved.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
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
