#!/usr/bin/env python3
"""Differential oracle for w14_methodscon_14c7d9 (tui_gateway/methods_config.py)."""
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
S = Q + "/tui_gateway/methods_config.py"
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
    def profile_scoped(self, fn): return fn
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **kw: None
sys.modules["tui_gateway.method_ctx"] = _mc
_envm = types.ModuleType("tui_gateway._env")
_envm.env_int = lambda *a, **kw: 8
sys.modules["tui_gateway._env"] = _envm

spec = importlib.util.spec_from_file_location("tui_gateway.methods_config_w14", S)
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
        if func == "safe_client_label":
            py = live._safe_client_label(r.get("label") or "")
        elif func == "display_word":
            live._display_raw = lambda: (r.get("display_raw") or {})
            py = live._display_word(r.get("key", ""), r.get("default", ""),
                                    set(r.get("allowed") or []))
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

print(f"ORACLE w14_methodscon_14c7d9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
