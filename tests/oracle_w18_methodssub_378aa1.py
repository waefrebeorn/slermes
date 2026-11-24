#!/usr/bin/env python3
"""Differential oracle for gate w18_methodssub_378aa1 (tui_gateway/methods_subagents.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [Q + "/" + name.replace(".", "/")]
    sys.modules[name] = m


_pkg("tui_gateway")
ctx = types.ModuleType("tui_gateway.method_ctx")


class HandlerRegistry:
    def __init__(self):
        self._pending = []

    def method(self, name):
        def dec(fn):
            self._pending.append((name, fn))
            return fn
        return dec

    def profile_scoped(self, fn):
        return fn


ctx.HandlerRegistry = HandlerRegistry
ctx.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = ctx

NAME = "tui_gateway.methods_subagents_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/methods_subagents.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w18_methodssub_378aa1"


class _Sessions:
    def __init__(self, ids):
        self.ids = set(ids)

    def get(self, k):
        return "row" if k in self.ids else None


class _Routing:
    def __init__(self, routes):
        self.routes = routes

    def latest_session_for_peer(self, source, session_key):
        sid = self.routes.get(session_key)
        if not sid:
            return None
        found = types.SimpleNamespace()
        found.session = types.SimpleNamespace(session_id=sid)
        return found


def compute(r):
    func = r.get("func")
    inp = r.get("in") or {}
    if func == "child_session_id":
        record = inp.get("record") or {}
        agent = record.get("agent")
        if isinstance(agent, dict):
            agent = types.SimpleNamespace(**agent)
        return live._child_session_id({"agent": agent})
    if func == "stored_owner":
        dbj = inp.get("db") or {}
        db = types.SimpleNamespace(
            sessions=_Sessions(dbj.get("sessions") or []),
            routing=_Routing(dbj.get("routing") or {}))
        return live._stored_owner(db, tuple(inp.get("owner_ids") or []),
                                  set(inp.get("namespaces") or []))
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
