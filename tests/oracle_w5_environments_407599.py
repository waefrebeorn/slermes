#!/usr/bin/env python3
"""Differential oracle for pm/environments.py (noc_w5_environments_407599.h)."""
import hashlib
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/pm/environments.py"
sys.path.insert(0, QUARRY)

if "pm" not in sys.modules:
    _pkg = types.ModuleType("pm")
    _pkg.__path__ = [QUARRY + "/pm"]
    sys.modules["pm"] = _pkg

spec = importlib.util.spec_from_file_location("environments_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["environments_live"] = live
spec.loader.exec_module(live)


def norm(s):
    return s.replace("\\", "/")


def compute(r):
    f = r["func"]
    if f == "venv_bin_dir":
        w = r.get("windows", -1)
        return norm(str(live.venv_bin_dir(Path(r.get("venv", "")), windows=None if w == -1 else bool(w))))
    if f == "venv_python":
        w = r.get("windows", -1)
        return norm(str(live.venv_python(Path(r.get("venv", "")), windows=None if w == -1 else bool(w))))
    if f == "fish_quote":
        return live._fish_quote(r.get("value", ""))
    if f == "shell_exports":
        return live.shell_exports(dict(r.get("env") or {}), r.get("dialect", "sh"))
    if f == "install_key":
        return live.hashlib.sha256(r.get("canonical", "").encode("utf-8")).hexdigest()[:16]
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE environments: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH environments {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE environments: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
