#!/usr/bin/env python3
"""Differential oracle for gateway/session_onboarding.py (w13_sessiononb_967c70).

The async/IO surface (start, skip, tracked writes, the run claim) is a seam; the
sidecar-side pure decisions are exercised directly against the live module:
handlers(), _onboarding_connection() and _run_id() with its load() seam stubbed.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessiononb_967c70"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap so `from gateway.x import y` resolves to our stubs.
for _p in ("gateway", "persistence", "persistence.core", "agent", "hermes_cli"):
    if _p not in sys.modules:
        stub(_p)


class AuthorityRefusal(Exception):
    def __init__(self, code, detail=None, **kw):
        super().__init__(code, detail)
        self.code, self.detail = code, detail


RECORD = [None]
stub("gateway.session_contract", SessionRef=object)
stub("gateway.session_persistence", authority_persistence=lambda a: a)
stub("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
stub("agent.onboarding_record", load=lambda *a, **k: RECORD[0])

SRC = QUARRY + "/gateway/session_onboarding.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)


class Conn:
    """Minimal asking connection: _sibling_for('default') yields the fixture sibling."""

    def __init__(self, sibling):
        self.sibling = sibling

    def _sibling_for(self, profile):
        assert profile == "default", profile
        return self.sibling


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
    if func == "handlers":
        return sorted(live.handlers(Conn(None)).keys())
    if func == "connection":
        conn = Conn(r.get("sibling"))
        res = live._onboarding_connection(conn)
        return "self" if res is conn else res
    if func == "run_id":
        RECORD[0] = r.get("record")
        return live._run_id(None, "sid")
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
