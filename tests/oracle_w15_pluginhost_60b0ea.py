#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_host_wire.py (w15_pluginhost_60b0ea)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

SRC = Q + "/hermes_cli/plugin_host_wire.py"
spec = importlib.util.spec_from_file_location("phw_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["phw_live"] = live
spec.loader.exec_module(live)

EXC = {
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "NotImplementedError": NotImplementedError, "TimeoutError": TimeoutError,
    "LookupError": LookupError, "RuntimeError": RuntimeError,
    "OSError": OSError, "StopIteration": StopIteration,
    "IndexError": IndexError,
    "JSONDecodeError": lambda m: json.JSONDecodeError(m, "doc", 0),
    "PluginHostUnsupported": live.PluginHostUnsupported,
    "PluginHostError": live.PluginHostError,
    "PluginHostUnavailable": live.PluginHostUnavailable,
    "RetiredUpdaterHook": RuntimeError,
}


def as_json(v):
    if isinstance(v, live.Record):
        return {k: as_json(x) for k, x in v.items()}
    if isinstance(v, live.Opaque):
        return repr(v)
    if isinstance(v, (list, tuple)):
        return [as_json(x) for x in v]
    if isinstance(v, dict):
        return {k: as_json(x) for k, x in v.items()}
    return v


def compute(r):
    f = r.get("func")
    if f == "error_payload":
        cls = EXC.get(r.get("type"), ValueError)
        exc = cls(r.get("message", ""))
        return live.error_payload(exc)
    if f == "raise_remote":
        err = {}
        if r.get("type") is not None:
            err["type"] = r["type"]
        if r.get("message") is not None:
            err["message"] = r["message"]
        if r.get("base") is not None:
            err["base"] = r["base"]
        try:
            live.raise_remote(err)
            return {"cls": "NO_EXCEPTION", "message": ""}
        except BaseException as exc:
            return {"cls": type(exc).__name__, "message": str(exc)}
    if f == "decode":
        return as_json(live.decode(r.get("value")))
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
    expected = compute(r)
    got = r.get("out")
    if expected != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w15_pluginhost_60b0ea: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
