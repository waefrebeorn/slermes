#!/usr/bin/env python3
"""Differential oracle for pm/pnpm_release_age.py (gate w7_pnpmrelease_bf14bc)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Parent-package bootstrap + stub of the one quarry collaborator imported at
# module scope. The helpers under test use only the standard library and the
# real hermes_yaml from the quarry.
_pm = types.ModuleType("pm")
_pm.__path__ = [QUARRY + "/pm"]
sys.modules.setdefault("pm", _pm)
_fs = types.ModuleType("pm.filesystem")
_fs.native = lambda p: str(p)
sys.modules.setdefault("pm.filesystem", _fs)

spec = importlib.util.spec_from_file_location(
    "w7_pnpmrelease_live", QUARRY + "/pm/pnpm_release_age.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_pnpmrelease_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "is_failure":
        return live.is_release_age_failure(r.get("output") or "")
    if func == "kept":
        locked = {k: set(v) for k, v in (r.get("locked") or {}).items()}
        return live._kept_versions(r.get("entry") or "", locked)
    if func == "yaml_scalar":
        return live._yaml_scalar(r.get("value") or "")
    if func == "exclude_hint":
        return live.exclude_hint(r.get("picks") or [])
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
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w7_pnpmrelease_bf14bc: live raised %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_pnpmrelease_bf14bc case=%s func=%s py=%r c=%r" %
              (r.get("case"), r.get("func"), py, got), file=sys.stderr)

print("ORACLE w7_pnpmrelease_bf14bc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
