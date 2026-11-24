#!/usr/bin/env python3
"""Differential oracle for gateway/session_stored_reads.py (gate w16_sessionsto_8c91af)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_sessionsto_8c91af"
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


for _n in ("persistence.core.errors", "gateway.session_busy_controls",
           "gateway.session_persistence", "gateway.session_transcript_rows"):
    _boot(_n)

_spec = importlib.util.spec_from_file_location(
    "gateway.session_stored_reads_live", QUARRY + "/gateway/session_stored_reads.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)


def _compute(r):
    f = r.get("func")
    latest = bool(r.get("latest", False))
    if f == "take":
        return live._take(r.get("roles", []), latest, int(r.get("limit", 0)))
    if f == "cut":
        return live._cut(r.get("roles", []), latest, int(r.get("taken", 0)))
    if f == "sent":
        return live._sent(r.get("handoffs", []), latest, r.get("item", [0, 0]))
    return None


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
