#!/usr/bin/env python3
"""Differential oracle for hermes_platform/declaration.py (gate w15_declaration_cc05e2)."""
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

NAME = "hermes_platform.declaration_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/hermes_platform/declaration.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "gpu_label":
        return live.gpu_label(r.get("gpu"))
    if f == "location_is_rooted":
        return live.location_is_rooted(r.get("location", ""), r.get("osf", ""))
    if f == "is_relative_name":
        return live._is_relative_name(r.get("value"), allow_dirs=bool(r.get("allow_dirs")))
    if f == "parse_requires":
        amap = r.get("app_per_os")
        app = None
        if isinstance(amap, dict):
            app = types.SimpleNamespace(
                per_os={k: types.SimpleNamespace(version_kind=v) for k, v in amap.items()})
        res = live.parse_requires(r.get("raw"), app, where=r.get("where", "oracle"))
        if res is None:
            return None
        return {"app": res.app, "min_version": res.min_version, "gpu": res.gpu}
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
        print(f"ORACLE w15_declaration_cc05e2: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_declaration_cc05e2 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_declaration_cc05e2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
