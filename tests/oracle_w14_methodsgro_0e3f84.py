#!/usr/bin/env python3
"""Differential oracle for w14_methodsgro_0e3f84 (tui_gateway/methods_groups.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
S = Q + "/tui_gateway/methods_groups.py"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [Q + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg
_mc = types.ModuleType("tui_gateway.method_ctx")
class _HR:
    def method(self, name=None):
        def d(fn): return fn
        return d
_mc.HandlerRegistry = _HR
sys.modules["tui_gateway.method_ctx"] = _mc
_mf = types.ModuleType("tui_gateway.methods_groups_forward")
_mf.forward_bound = lambda *a, **kw: None
sys.modules["tui_gateway.methods_groups_forward"] = _mf

spec = importlib.util.spec_from_file_location("tui_gateway.methods_groups_w14", S)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "grant_expiry":
            py = live._grant_expiry(r.get("claims") or {})
        elif func == "include_disbanded":
            py = live._include_disbanded(r.get("params") or {})
        elif func == "methods":
            py = list(live._METHODS)
        elif func == "long_handlers":
            py = sorted(live.LONG_HANDLERS)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_methodsgro_0e3f84: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
