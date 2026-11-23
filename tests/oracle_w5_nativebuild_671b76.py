#!/usr/bin/env python3
"""Differential oracle for pm/native_build.py (noc_w5_nativebuild_671b76.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/pm/native_build.py"
sys.path.insert(0, QUARRY)

if "pm" not in sys.modules:
    _pkg = types.ModuleType("pm")
    _pkg.__path__ = [QUARRY + "/pm"]
    sys.modules["pm"] = _pkg
# Stub the progress helper: native_build only references it inside the side-effecting
# prepare_windows_environment, which the seams replace here.
_pg = types.ModuleType("pm.progress")
_pg.run_contained = lambda *a, **k: None
sys.modules["pm.progress"] = _pg

spec = importlib.util.spec_from_file_location("nativebuild_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["nativebuild_live"] = live
spec.loader.exec_module(live)


def _invalid(prepared):
    return (not isinstance(prepared, dict) or
            any(not isinstance(k, str) or not isinstance(v, str) for k, v in prepared.items()))


def compute(r):
    f = r["func"]
    prepared = r.get("prepared")
    if f == "prepare_validate":
        if _invalid(prepared):
            return "ValueError"
        return prepared
    if f == "source_build_environment":
        if r.get("target") != "win32-arm64" or not r.get("provider_present"):
            return None
        if _invalid(prepared):
            return "ValueError"
        merged = dict(prepared)
        merged.update(r.get("bridged") or {})
        return merged
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
        print(f"ORACLE nativebuild: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH nativebuild {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE nativebuild: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
