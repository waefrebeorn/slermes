#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_scope.py (gate w7_mcptoolsco_6bf440).

Loads the LIVE module by path under its package name. The sibling
``tools.mcp_tool_common._core`` registry state is stubbed and, per case, driven
from fixture keys (current_scope / servers / lazy / shared) instead of any C
process state, so every case is deterministic.
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


core = _Attr(_mcp_registry_scope=lambda: None, _server_tool_scopes={},
             _servers=set(), _lazy_server_configs={})
pkg = _mod("tools")
pkg.__path__ = [Q + "/tools"]
_mod("tools.mcp_tool_common", _core=core)

SRC = Q + "/tools/mcp_tool_scope.py"
spec = importlib.util.spec_from_file_location("tools.mcp_tool_scope", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tools.mcp_tool_scope"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def key_to_py(k):
    if isinstance(k, list) and len(k) == 2:
        return (k[0], k[1])
    return k


def key_to_json(k):
    return list(k) if isinstance(k, tuple) else k


def compute(r):
    f = r.get("func")
    core._mcp_registry_scope = lambda: r.get("current_scope")
    if f == "key_name":
        return live._key_name(key_to_py(r.get("key")))
    if f == "key_scope":
        return live._key_scope(key_to_py(r.get("key")))
    if f == "key_visible":
        core._server_tool_scopes = {key_to_py(r.get("key")): set(r.get("adopted") or [])}
        return live._key_visible_in_scope(key_to_py(r.get("key")), r.get("scope"))
    if f == "server_key":
        return key_to_json(live._server_key(r.get("name", ""), r.get("scope"),
                                            current=bool(r.get("current", True))))
    if f == "resolve":
        core._servers = {key_to_py(k) for k in (r.get("servers") or [])}
        core._lazy_server_configs = {key_to_py(k) for k in (r.get("lazy") or [])}
        core._server_tool_scopes = {
            key_to_py(e.get("key")): set(e.get("scopes") or []) for e in (r.get("shared") or [])}
        return key_to_json(live._resolve_server_key(r.get("name", ""), r.get("scope"),
                                                    current=bool(r.get("current", True))))
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
print("ORACLE w7_mcptoolsco_6bf440: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
