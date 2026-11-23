#!/usr/bin/env python3
"""Differential oracle for gateway/session_tui_config.py (w13_sessiontui_41eb5b).

Config-IO, authorization and the display registries are seams; _word,
_statusbar and _set_profile_leaf are exercised on the live module. Refusals
are projected as {"__raise__": <exception class>}.
"""
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessiontui_41eb5b"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for _p in ("persistence", "persistence.core"):
    if _p not in sys.modules:
        stub(_p)


class AuthorityRefusal(Exception):
    def __init__(self, code, detail=None, **kw):
        super().__init__(code, detail)
        self.code, self.detail = code, detail


stub("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = QUARRY + "/gateway/session_tui_config.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
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
    if func == "word":
        return live._word(r.get("value"))
    if func == "statusbar":
        return live._statusbar(r.get("raw"))
    if func == "leaf":
        cfg = copy.deepcopy(r.get("cfg"))
        live._set_profile_leaf(cfg, r.get("key"), r.get("value"))
        return cfg
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
        py = {"__raise__": type(exc).__name__}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
