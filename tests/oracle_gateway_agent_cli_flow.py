#!/usr/bin/env python3
"""Oracle for gateway_agent_cli_flow.c — the gateway agent-bridge FLOW cores +
leaves of hermes_cli/gateway_agent.py (LIVE authority: GatewayAgent + module
helpers) that the sibling gateway_agent_flow port does NOT claim.

Drives the LIVE methods/functions with SCRIPTED I/O: defs are AST-extracted
from the quarry (no heavy gateway import), module constants seeded (Assign AND
AnnAssign), and every connection.call / time.monotonic, the claimed siblings
(_correct / _give_back_turn_route), the seamed collaborators
(_reset_session_figures / _refresh_stats / apply_session_usage /
_anchor_display / _rate_limit_state), pending_lines, print / print_fn, the MoA
tool_progress_callback and the RateLimitBucket / RateLimitState constructors
replaced by recording monkeypatch stubs returning the SAME scripted results the
C harness feeds its universal seam. The injected self-model `st` is mirrored by
a SelfDouble whose attribute writes land back in `st`, so the mutated state is
compared too. Compares the ordered DECISION SEQUENCE + return + state.

Fail-closed: a missing/empty seam script returns a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; `except
Exception` sites in live swallow plain Exceptions but pass BaseException through.

Input: stdin JSON lines from t_port_gateway_agent_cli_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "st":...}}
Summary: ORACLE gateway_agent_cli_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.
"""
import ast
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "gateway_agent.py")

PY = {
    "review_summary_line": "review_summary_line",
    "summarize_api_error": "_summarize_api_error",
    "rate_limit_state": "_rate_limit_state",
    "moa_handler": "_moa_handler",
    "apply_info": "_apply_info",
    "tools": "tools",
    "valid_tool_names": "valid_tool_names",
    "apply_session_usage": "apply_session_usage",
    "reset_session_figures": "_reset_session_figures",
    "reset_session_state": "reset_session_state",
    "interrupt": "interrupt",
    "hard_interrupt": "hard_interrupt",
    "steer": "steer",
    "redirect": "redirect",
    "expired": "_expired",
    "stage_content_parts": "_stage_content_parts",
    "lost_turn_notice": "_lost_turn_notice",
    "user_row": "_user_row",
    "failed": "_failed",
    "unadmitted": "_unadmitted",
    "emit_print": "_emit_print",
    "on_print": "_on_print",
    "on_review_summary": "_on_review_summary",
    "owns": "_owns",
}
WANTED = set(PY.values())
ALL_DEFS = WANTED

# Methods composed REAL through self (both ported; no seam event).
REAL_BIND = {"_user_row", "_failed", "_emit_print"}
# Methods seamed through self (claimed siblings + unported collaborators).
SEAM_METHODS = {"_correct", "_give_back_turn_route", "_reset_session_figures",
                "_refresh_stats", "apply_session_usage", "_anchor_display", "interrupt"}
SEAM_PROJ = {
    "_correct": lambda a, k: {"verb": a[0], "text": a[1]},
    "_give_back_turn_route": lambda a, k: {},
    "_reset_session_figures": lambda a, k: {},
    "_refresh_stats": lambda a, k: {},
    "apply_session_usage": lambda a, k: {"usage": a[0]},
    "_anchor_display": lambda a, k: {},
    "interrupt": lambda a, k: {"message": a[0] if a else None},
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "st": None,
       "turn_obj": None, "turn_id": None}
OTHER_TURN = object()  # stable sentinel for a non-owned turn identity


class GatewayClientError(Exception):
    pass


class GatewayRPCError(GatewayClientError):
    pass


class CallStopped(Exception):
    pass


BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "GatewayClientError": GatewayClientError, "GatewayRPCError": GatewayRPCError,
    "CallStopped": CallStopped, "RuntimeError": RuntimeError, "ValueError": ValueError,
    "TypeError": TypeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "StopIteration": StopIteration, "OSError": OSError,
}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


# ── extraction ──

def extract():
    import threading
    import time
    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.gateway_agent_cli_extracted",
        "json": json, "os": os, "sys": sys, "threading": threading,
        "time": time, "Path": Path, "logger": RecLogger(),
        "GatewayClientError": GatewayClientError, "GatewayRPCError": GatewayRPCError,
        "CallStopped": CallStopped, "VOICE_STOPPED": "voice_stopped",
    }
    tree = ast.parse(src)
    const_nodes = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            const_nodes.append(node)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            const_nodes.append(node)
    for node in const_nodes:
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in ALL_DEFS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # GatewayAgent methods (tools / valid_tool_names are @property getters).
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "GatewayAgent")
    for node in cls.body:
        if isinstance(node, ast.FunctionDef) and node.name in ALL_DEFS:
            node.decorator_list = []  # strip @property / @setter wrappers
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # Re-seed logger AFTER the Assign loop (logger = logging.getLogger is an Assign).
    ns["logger"] = RecLogger()
    missing = ALL_DEFS - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


# ── self / state / connection doubles ──

class StateDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def __getattr__(self, name):
        return object.__getattribute__(self, "_d").get(name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_d")[name] = value


class ConnDouble:
    def __init__(self, st):
        self._st = st
        self.epoch = st.get("connection_epoch", 0)

    def call(self, method, **kwargs):
        return seam("call", {"method": method, **kwargs})

    def state(self, sid):
        return StateDouble(self._st.setdefault("state", {}))


class SelfDouble:
    def __init__(self, st, ns):
        object.__setattr__(self, "_st", st)
        object.__setattr__(self, "_ns", ns)
        object.__setattr__(self, "connection", ConnDouble(st))

    def __getattr__(self, name):
        ns = object.__getattribute__(self, "_ns")
        st = object.__getattribute__(self, "_st")
        if name == "tools":  # property access evaluates the getter
            return ns["tools"](self)
        if name == "context_compressor":  # _CompressorView attribute double
            return StateDouble(st.setdefault("context_compressor", {}))
        if name == "_print_fn":
            if st.get("_print_fn"):
                return lambda text: seam("print_fn", {"text": text})
            return None
        if name == "_turn":  # identity modeled by id, st stays pristine
            tid = st.get("_turn")
            if tid is not None and tid == CUR.get("turn_id"):
                return CUR["turn_obj"]
            return OTHER_TURN
        if name in REAL_BIND:
            f = ns[name]
            return lambda *a, **k: f(self, *a, **k)
        if name in SEAM_METHODS:
            proj = SEAM_PROJ[name]
            return lambda *a, **k: seam(name, proj(a, k))
        if name == "_state":
            return lambda: self.connection.state(st.get("session_id"))
        return st.get(name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_st")[name] = value


# ── recording stubs ──

def rec_print(*args, **kw):
    text = args[0] if args else ""
    seam("print", {"text": text})


def install_stubs(ns):
    ns["print"] = rec_print
    ns["time"] = types.SimpleNamespace(monotonic=lambda: seam("monotonic", {}))
    # agent.rate_limit_tracker constructors
    agent = sys.modules.get("agent") or types.ModuleType("agent")
    agent.__path__ = []
    sys.modules["agent"] = agent
    rlt = types.ModuleType("agent.rate_limit_tracker")

    def RateLimitBucket(**kw):
        return seam("RateLimitBucket", {"kwargs": kw})

    def RateLimitState(**kw):
        # live splats **buckets into individual kwargs; reconstruct the map the
        # C seam records as args.buckets (fixed bucket order).
        buckets = {k: kw[k] for k in ("requests_min", "requests_hour", "tokens_min", "tokens_hour") if k in kw}
        return seam("RateLimitState", {"buckets": buckets,
                                       "captured_at": kw.get("captured_at"),
                                       "provider": kw.get("provider")})

    rlt.RateLimitBucket = RateLimitBucket
    rlt.RateLimitState = RateLimitState
    sys.modules["agent.rate_limit_tracker"] = rlt
    # hermes_cli.gateway_chat_view.pending_lines
    hcli = sys.modules.get("hermes_cli") or types.ModuleType("hermes_cli")
    hcli.__path__ = []
    sys.modules["hermes_cli"] = hcli
    chatview = types.ModuleType("hermes_cli.gateway_chat_view")
    chatview.pending_lines = lambda pending: seam("pending_lines", {"pending": pending})
    sys.modules["hermes_cli.gateway_chat_view"] = chatview


# ── drive ──

def drive(ns, func, inp):
    name = PY[func]
    CUR["st"] = None
    if func == "review_summary_line":
        return ns[name](inp.get("payload")), None
    if func == "summarize_api_error":
        exc = inp.get("exc") or {}
        e = make_exc(exc.get("kind", "RuntimeError"), exc.get("msg", ""))
        return ns[name](None, e), None  # method(self, exc); self unused
    if func == "rate_limit_state":
        return ns[name](inp.get("data")), None
    if func == "moa_handler":
        cb = None
        if inp.get("cb_present"):
            def cb(kind, label, text, _none, **extra):
                return seam("tool_progress_callback",
                            {"kind": kind, "label": label, "text": text, "extra": extra})
        agent = types.SimpleNamespace(tool_progress_callback=cb)
        handler = ns[name](inp.get("kind", "moa.reference"))
        handler(agent, inp.get("payload") or {}, None, {})
        return None, None

    st = json.loads(json.dumps(inp.get("st") or {}))
    CUR["st"] = st
    self = SelfDouble(st, ns)
    f = ns[name]
    if func == "apply_info":
        f(self, inp.get("info")); ret = None
    elif func == "tools":
        ret = f(self)
    elif func == "valid_tool_names":
        ret = sorted(f(self))
    elif func == "apply_session_usage":
        f(self, inp.get("usage")); ret = None
    elif func == "reset_session_figures":
        f(self); ret = None
    elif func == "reset_session_state":
        f(self); ret = None
    elif func == "interrupt":
        f(self, inp.get("message")); ret = None
    elif func == "hard_interrupt":
        f(self, inp.get("message")); ret = None
    elif func == "steer":
        ret = f(self, inp.get("text"))
    elif func == "redirect":
        ret = f(self, inp.get("text"))
    elif func == "expired":
        ret = f(self)
    elif func == "stage_content_parts":
        ret = f(self, inp.get("parts"))
    elif func == "lost_turn_notice":
        ret = f(self)
    elif func == "user_row":
        ret = f(self, inp.get("user_message"))
    elif func == "failed":
        ret = f(self, inp.get("history"), inp.get("user_message"), inp.get("error"),
                failure_reason=inp.get("failure_reason"), retryable=inp.get("retryable", False))
    elif func == "unadmitted":
        ret = f(self, inp.get("history"), inp.get("user_message"), inp.get("error"))
    elif func == "emit_print":
        f(self, inp.get("text", "")); ret = None
    elif func == "on_print":
        f(self, inp.get("payload"), None, {}); ret = None
    elif func == "on_review_summary":
        f(self, inp.get("payload"), None, {}); ret = None
    elif func == "owns":
        ti = inp.get("turn") or {}
        turn = types.SimpleNamespace(id=ti.get("id", ""), session_id=ti.get("session_id", ""))
        CUR["turn_obj"] = turn
        CUR["turn_id"] = turn.id
        ret = f(self, turn)
    else:
        raise ValueError("unknown func: %s" % func)
    return ret, st


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def main():
    ns = extract()
    install_stubs(ns)
    REAL = {nm: ns[nm] for nm in WANTED}
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["turn_obj"] = None
        CUR["turn_id"] = None
        # Restore every target def to real, then install the module seamed helper
        # recorder unless it IS the current target.
        for nm in REAL:
            ns[nm] = REAL[nm]
        if func != "rate_limit_state":
            ns["_rate_limit_state"] = lambda data: seam("_rate_limit_state", {"data": data})
        try:
            ret, st = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            st = CUR["st"]
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if st is not None:
            want["st"] = norm(st)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_agent_cli_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_agent_cli_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()