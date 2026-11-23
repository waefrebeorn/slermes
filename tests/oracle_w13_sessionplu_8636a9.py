#!/usr/bin/env python3
"""Differential oracle for gateway/session_plugin_rpc.py (w13_sessionplu_8636a9).

The off-loop worker and the live-session registry are seams: plugin_rpc_handler
is stubbed to project the resolved handler, and _live() runs against a fake
authority whose session map / resident agents come from the fixture.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessionplu_8636a9"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


for _p in ("gateway", "persistence", "persistence.core", "hermes_cli"):
    if _p not in sys.modules:
        stub(_p)


class AuthorityRefusal(Exception):
    def __init__(self, code, detail=None, **kw):
        super().__init__(code, detail)
        self.code, self.detail = code, detail


def SessionRef(profile_id, session_id):
    return ("ref", profile_id, session_id)


stub("gateway.session_contract", SessionRef=SessionRef)
stub("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
stub("hermes_cli.plugins_dispatch", resolve_plugin_command_result=lambda x: x)
stub("hermes_cli.plugin_events", PLUGIN_EVENT_PREFIX="plugin.")
stub("hermes_cli.plugin_rpc", plugin_rpc_handler=lambda m: "handled:" + m)

SRC = QUARRY + "/gateway/session_plugin_rpc.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)


async def _off(conn, func):
    return func()


live._off_loop = _off  # the worker seam: run the handler synchronously on the loop


class Authority:
    def __init__(self, sessions, agents):
        self.sessions = sessions
        self.agents = agents
        self.profile_id = "default"

    def agent(self, ref):
        key = ref[2]
        if key not in self.agents:
            raise KeyError(key)
        return types.SimpleNamespace(session_id=self.agents[key])


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
    if func == "resolve":
        return asyncio.run(live.resolve_plugin_rpc(None, r.get("method")))
    if func == "live_key":
        authority = Authority(r.get("sessions") or {}, r.get("agents") or {})
        res = live._live(authority, r.get("session_id"))
        return res[0] if res else None  # project (conversation key, live session) -> key
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
