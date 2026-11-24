#!/usr/bin/env python3
"""Differential oracle for gateway/session_local_editor.py (gate w16_sessionloc_eaa05a)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_sessionloc_eaa05a"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


class _Dummy(types.ModuleType):
    def __getattr__(self, n):
        v = lambda *a, **k: None
        setattr(self, n, v)
        return v


def _boot(full):
    parts = full.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        if nm not in sys.modules:
            m = _Dummy(nm)
            if i < len(parts):
                m.__path__ = []
            sys.modules[nm] = m
    if len(parts) > 1:
        setattr(sys.modules[".".join(parts[:-1])], parts[-1], sys.modules[full])


class AuthorityRefusal(Exception):
    def __init__(self, reason, field=None, detail=None):
        self.reason = reason
        self.field = field
        self.detail = detail
        super().__init__(detail or reason)


_boot("persistence.core.errors")
_boot("gateway.session_local_mcp")
sys.modules["persistence.core.errors"].AuthorityRefusal = AuthorityRefusal
sys.modules["gateway.session_local_mcp"].validate_servers = lambda servers: None

_spec = importlib.util.spec_from_file_location(
    "gateway.session_local_editor_live", QUARRY + "/gateway/session_local_editor.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)


def _compute(r):
    try:
        live.validate_editor(r.get("source"), r.get("editor"))
    except AuthorityRefusal as e:
        return {"ok": False, "reason": e.reason, "field": e.field, "detail": e.detail}
    return {"ok": True}


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
