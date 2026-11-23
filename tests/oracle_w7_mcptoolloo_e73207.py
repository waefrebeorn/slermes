#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_loop.py (gate w7_mcptoolloo_e73207).

Loads the LIVE module with its asyncio/_core dependencies stubbed and replays
the three portable decisions: benign-loop-close suppression, bounded
ready-session polling, and direct vs thread-safe reconnect signalling.
"""
import asyncio, contextlib, importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = [Q + "/tools"]
sys.modules["tools"] = tools_pkg

common = types.ModuleType("tools.mcp_tool_common")
core = types.SimpleNamespace(
    _lock=contextlib.nullcontext(), _mcp_loop=None, _servers={},
    _server_connecting=set(), _LOCK_UNAVAILABLE=object(), _MCP_LOOP_DRAIN_TIMEOUT=5.0)
common._core = core
sys.modules["tools.mcp_tool_common"] = common
tools_pkg.mcp_tool_common = common

lifecycle = types.ModuleType("tools.mcp_tool_lifecycle")
lifecycle._drain_and_stop_mcp_loop = lambda *a, **k: None
lifecycle._drain_mcp_loop_tasks = lambda *a, **k: None
lifecycle._kill_orphaned_mcp_children = lambda *a, **k: None
sys.modules["tools.mcp_tool_lifecycle"] = lifecycle
tools_pkg.mcp_tool_lifecycle = lifecycle

SRC = Q + "/tools/mcp_tool_loop.py"
spec = importlib.util.spec_from_file_location("w7_mcptoolloo_e73207_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_mcptoolloo_e73207_live"] = live
spec.loader.exec_module(live)

# Freeze time so the bounded poll never really sleeps.
live.time = types.SimpleNamespace(sleep=lambda s: None, monotonic=lambda: 0.0)


def compute_forward(r):
    rec = []

    class Loop:
        def default_exception_handler(self, ctx):
            rec.append(1)

    ctx = {}
    if r.get("exc_is_runtime", 0):
        ctx["exception"] = RuntimeError(r.get("message", ""))
    elif r.get("has_exception", 0):
        ctx["exception"] = ValueError(r.get("message", ""))
    live._mcp_loop_exception_handler(Loop(), ctx)
    return bool(rec)


def compute_wait(r):
    steps = r.get("script", [])
    OLD = object()

    class Ready:
        def __init__(self, val, raises):
            self.val, self.raises = val, raises

        def is_set(self):
            if self.raises:
                raise RuntimeError("boom")
            return bool(self.val)

    class Srv:
        def __init__(self):
            self.i = 0

        @property
        def session(self):
            st = steps[self.i] if self.i < len(steps) else {}
            v = st.get("session", 0)
            if v == 1:
                return OLD
            if v == 2:
                return object()
            return None

        @property
        def _ready(self):
            st = steps[self.i] if self.i < len(steps) else {}
            self.i += 1
            if not st.get("ready_present", 1):
                raise AttributeError("_ready")
            return Ready(bool(st.get("ready", 1)), bool(st.get("ready_raises", 0)))

    return bool(live._wait_for_server_session_ready(
        Srv(), old_session=OLD, timeout=float(r.get("timeout", 0.75))))


def compute_signal(r):
    rec = []
    if r.get("has_event", 0):
        if r.get("event_is_asyncio", 0):
            class Ev(asyncio.Event):
                def set(self):
                    rec.append("set")
                    super().set()
            ev = Ev()
        else:
            class Plain:
                def set(self):
                    rec.append("plain")
            ev = Plain()
        srv = types.SimpleNamespace(_reconnect_event=ev)
    else:
        srv = types.SimpleNamespace()

    class Loop:
        def is_running(self):
            return True

        def call_soon_threadsafe(self, fn):
            rec.append("threadsafe")
            fn()

    core._mcp_loop = Loop() if r.get("loop_running", 0) else None
    ret = live._signal_reconnect(srv)
    return 0 if not ret else (2 if "threadsafe" in rec else 1)


def compute(r):
    func = r.get("func")
    if func == "forward":
        return compute_forward(r)
    if func == "wait_ready":
        return compute_wait(r)
    if func == "signal":
        return compute_signal(r)
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_mcptoolloo_e73207: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
