#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/relay_cwd.py (gate w5_relaycwd_c03229).

All runtime lookups are stubbed to return the fixture's resolved facts; the
live resolve_relay_scope_cwds then runs unchanged against those facts.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")
_pkg("tools", QUARRY + "/tools")
_pkg("gateway", QUARRY + "/gateway")


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tt = _stub("tools.terminal_tool", get_session_cwd=lambda k: None)
rc = _stub("agent.runtime_cwd", scoped_session_cwd=lambda: None, resolve_agent_cwd=lambda: None)
sc = _stub("gateway.session_context", get_session_env=lambda n, d: d)
ts = _stub("tools.terminal_scope", terminal_env=lambda n, d: d)


class FakePath:
    def __init__(self, s):
        self.s = s or ""

    def is_absolute(self):
        return True

    def resolve(self):
        return self

    def __str__(self):
        return self.s


spec = importlib.util.spec_from_file_location("agent.relay_cwd", QUARRY + "/agent/relay_cwd.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.relay_cwd"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "clean":
        py = live._clean_cwd(r.get("value"))
    elif func == "resolve":
        f = r["facts"]
        recorded = f.get("recorded") or {}

        def gsc(key):
            if key == "task":
                return f.get("task_cwd")
            return recorded.get(key)

        tt.get_session_cwd = gsc
        rc.scoped_session_cwd = lambda: f.get("scoped_cwd")
        rc.resolve_agent_cwd = lambda: FakePath(f["resolved_cwd"])
        sc.get_session_env = lambda name, default: (
            f.get("session_key", "") if name == "HERMES_SESSION_KEY" else default)
        ts.terminal_env = lambda name, default: (
            f.get("terminal_env", default) if name == "TERMINAL_ENV"
            else f.get("terminal_cwd", default))
        agent = SimpleNamespace(session_cwd=f.get("agent_cwd"))
        py = list(live.resolve_relay_scope_cwds(
            agent, "task", "session-1", f.get("platform", "")))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_relaycwd_c03229: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
