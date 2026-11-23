#!/usr/bin/env python3
"""Differential oracle for gate w10_agentinit_2749c4 (agent/agent_init_session.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap for the in-function imports exercised by the seams
def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m
_pkg("gateway")
_pkg("agent")

NAME = "w10_agentinit_2749c4_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "agent_init_session.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def mat(v):
    if isinstance(v, dict):
        return types.SimpleNamespace(**{k: mat(x) for k, x in v.items()})
    if isinstance(v, list):
        return [mat(x) for x in v]
    return v

def canon(v):
    if isinstance(v, types.SimpleNamespace):
        return {k: canon(x) for k, x in vars(v).items()}
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, list):
        return [canon(x) for x in v]
    return v

def stand_in(r):
    reg = bool(r.get("registered", False))
    sys.modules.pop("hermes_state", None)
    sdb = r.get("session_db") or {}
    store = mat(sdb.get("_store"))
    ro = bool(sdb.get("read_only", False))
    if reg:
        hs = types.ModuleType("hermes_state")
        class _SessionDB(object):
            def __init__(self, st, readonly):
                object.__setattr__(self, "_store", st)
                object.__setattr__(self, "read_only", readonly)
        hs._SessionDB = _SessionDB
        sys.modules["hermes_state"] = hs
    if reg and bool(r.get("is_session_db", False)):
        db = hs._SessionDB(store, ro)
    else:
        db = types.SimpleNamespace(_store=store, read_only=ro)
    return live.stand_in_interface(db)

def publish(r):
    sid = r.get("session_id", "")
    bridge = bool(r.get("bridge_ok", False))
    child = bool(r.get("delegated_child", False))
    called = [False]
    def set_sid(s):
        if not bridge:
            raise RuntimeError("bridge down")
        called[0] = True
    def is_child():
        return child
    gsc = types.ModuleType("gateway.session_context")
    gsc.set_current_session_id = set_sid
    sys.modules["gateway"].session_context = gsc
    sys.modules["gateway.session_context"] = gsc
    dc = types.ModuleType("agent.delegation_context")
    dc.is_delegated_child_context = is_child
    sys.modules["agent"].delegation_context = dc
    sys.modules["agent.delegation_context"] = dc
    os.environ.pop("HERMES_SESSION_ID", None)
    live._publish_session_id(sid)
    if called[0]:
        return "contextvar"
    if os.environ.get("HERMES_SESSION_ID") == sid:
        return "env"
    return "skip"

def compute(r):
    f = r.get("func")
    if f == "writer_interface":
        return canon(live.writer_interface(mat(r.get("persistence")), mat(r.get("store"))))
    if f == "agent_interface":
        return canon(live.agent_interface(mat(r.get("agent", {}))))
    if f == "stand_in_interface":
        return canon(stand_in(r))
    if f == "publish_session_id":
        return publish(r)
    return None

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_agentinit_2749c4: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
