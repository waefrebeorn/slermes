#!/usr/bin/env python3
"""sta_oracle_memory_status.py — Python twin for src/gateway/memory_status.c.

Imports the LIVE gateway/memory_status.py (spec-loaded directly: importing
the gateway package drags the whole platform stack) and mirrors the C ops:
  {"kind":"classify","avail":<any>,"total":<any>}
  {"kind":"collect","home":"<subdir-of-fixture-dir>","now":<epoch float>}
Emits one JSON line per op (same order).
"""
import importlib.util
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

PY_ROOT = str(Path.home() / ".hermes" / "hermes-agent")
if not os.path.isdir(PY_ROOT):
    PY_ROOT = "/home/wubu/hermes-agent-dev"
sys.path.insert(0, PY_ROOT)


def _load(name, relpath):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(PY_ROOT, relpath))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


memory_status = _load("gateway.memory_status", "gateway/memory_status.py")


def main():
    fixture = os.path.abspath(sys.argv[1])
    fixdir = os.path.dirname(fixture)
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "classify":
            out = memory_status.classify_pressure(c.get("avail"), c.get("total"))
        elif kind == "collect":
            home = os.path.join(fixdir, c.get("home", ""))
            now = c.get("now")
            moment = (
                datetime.fromtimestamp(now, tz=timezone.utc)
                if now is not None
                else None
            )
            out = memory_status.collect_memory_status(Path(home), now=moment)
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
