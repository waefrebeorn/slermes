#!/usr/bin/env python3
"""sta_oracle_disk_status.py — Python twin for src/gateway/disk_status.c.

Imports the LIVE gateway/disk_status.py (spec-loaded directly: importing the
gateway package drags the whole platform stack) and mirrors the C harness ops:
  {"kind":"classify","free":<any>,"total":<any>}
  {"kind":"collect","total_bytes":N,"free_bytes":N}   # injected statvfs sample
Emits one JSON line per op (same order).
"""
import importlib.util
import json
import os
import sys
from collections import namedtuple
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


disk_status = _load("gateway.disk_status", "gateway/disk_status.py")

_usage = namedtuple("usage", "total free used")


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "classify":
            out = disk_status.classify_disk_pressure(c.get("free"), c.get("total"))
        elif kind == "collect":
            total = int(c.get("total_bytes", 0))
            free = int(c.get("free_bytes", 0))
            used = max(0, total - free)

            real = disk_status.shutil.disk_usage
            disk_status.shutil.disk_usage = lambda path: _usage(total, free, used)
            try:
                out = disk_status.collect_disk_status(None)
            finally:
                disk_status.shutil.disk_usage = real
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
