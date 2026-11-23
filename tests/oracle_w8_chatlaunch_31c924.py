#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/chat_launch.py
(w8_chatlaunch_31c924 gate).

Live module loaded via spec. Store lookups (resolve_id / get) and the store
failure are injected with deterministic stand-ins; the live _short_resume_id
and _branch_lineage bodies do the real work.
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


_mod("agent.i18n", t=lambda key, **kw: key)


class StoreFailure(Exception):
    pass


pres = _mod("persistence"); pres.__path__ = []
core = _mod("persistence.core"); core.__path__ = []
_mod("persistence.core.errors", STORE_FAILURES=(StoreFailure,), AuthorityRefusal=Exception)

SRC = Q + "/hermes_cli/chat_launch.py"
spec = importlib.util.spec_from_file_location("chatlaunch_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["chatlaunch_live"] = live
spec.loader.exec_module(live)


class _Sess:
    def __init__(self, sid, cfg):
        self.session_id = sid
        self.model_config = cfg


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
        if func == "short":
            class _Sessions:
                def resolve_id(self, short):
                    if r.get("resolve_error", False):
                        raise StoreFailure()
                    return r.get("resolve_map", {}).get(short)
            pers = types.SimpleNamespace(sessions=_Sessions())
            py = live._short_resume_id(pers, r.get("session_id", ""))
        elif func == "lineage":
            smap = {sid: _Sess(sid, v) for sid, v in r.get("sessions", {}).items()}
            pers = types.SimpleNamespace(
                sessions=types.SimpleNamespace(get=lambda sid: smap.get(sid)))
            rows = live._branch_lineage(pers, smap.get(r.get("start_id", "")))
            py = [row.session_id for row in rows]
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w8_chatlaunch_31c924: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
