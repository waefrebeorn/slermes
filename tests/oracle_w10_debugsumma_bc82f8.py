#!/usr/bin/env python3
"""Differential oracle for hermes_cli/debug_summary.py (gate w10_debugsumma_bc82f8).

The live _toolset_names late-imports toolsets.TOOLSETS; the fixture injects that
set so the C port and the live function see the same allowlist.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/debug_summary.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location("w10_debugsumma_bc82f8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_debugsumma_bc82f8_live"] = live
spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "config_shape":
        return live._config_shape(r.get("raw") or {}, r.get("schema") or {})
    if func == "toolset_names":
        ts = types.ModuleType("toolsets")
        ts.TOOLSETS = set(r.get("toolsets") or [])
        sys.modules["toolsets"] = ts
        return live._toolset_names(r.get("raw_config") or {})
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_debugsumma_bc82f8: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_debugsumma_bc82f8 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_debugsumma_bc82f8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
