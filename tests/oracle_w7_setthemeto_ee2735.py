#!/usr/bin/env python3
"""Differential oracle for tools/set_theme_tool.py (gate w7_setthemeto_ee2735).

Loads the LIVE module by path under its package name. ``tools.registry`` is
stubbed with a faithful ``tool_error`` (from registry.py) and a no-op registry;
the renderer callback is a deterministic injected stub shared with the C side.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Attr:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def tool_error(message, **extra):
    t = str(message)
    if len(t) > 2048:
        t = t[:2048] + "\u2026 [truncated]"
    d = {"error": t}
    d.update(extra)
    return json.dumps(d, ensure_ascii=False)


pkg = _mod("tools")
pkg.__path__ = [Q + "/tools"]
_mod("tools.registry", registry=_Attr(register=lambda **kw: None), tool_error=tool_error)

SRC = Q + "/tools/set_theme_tool.py"
spec = importlib.util.spec_from_file_location("tools.set_theme_tool", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tools.set_theme_tool"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "accent_error":
        raw = live._accent_error(r.get("accent"))
        return json.loads(raw) if raw else None
    if f == "set_theme":
        cb = (lambda d: {"accepted": d}) if r.get("callback") else None
        raw = live.set_theme_tool(accent=r.get("accent"), mode=r.get("mode"),
                                  layout=r.get("layout"), picker=r.get("picker"), callback=cb)
        return json.loads(raw)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w7_setthemeto_ee2735: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
