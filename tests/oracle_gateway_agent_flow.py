#!/usr/bin/env python3
"""Oracle for port_gateway_agent_flow.c — the gateway agent-bridge FLOW cores
of hermes_cli/gateway_agent.py (LIVE authority: GatewayAgent + module leaves).

Drives the LIVE methods/functions with SCRIPTED I/O: defs are AST-extracted
from the quarry (no heavy gateway import), module constants seeded (Assign AND
AnnAssign), and every connection.call/reattach/start, state.runs_route, i18n,
print, uuid4, pending_lines and clarify-callback dependency replaced by a
recording monkeypatch stub returning the SAME scripted results the C harness
feeds its universal seam. The injected self-model `st` is mirrored by a
SelfDouble whose attribute writes land back in `st`, so the mutated state is
compared too. Compares the ordered DECISION SEQUENCE + return + state.

Input: stdin JSON lines from t_port_gateway_agent_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "st":...}}
Summary: ORACLE gateway_agent_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
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
    "effort_word": "_effort_word",
    "route_off_session": "_route_off_session",
    "set_reasoning": "_set_reasoning",
    "sync_show_reasoning": "_sync_show_reasoning",
    "service_tier": "service_tier",
    "correct": "_correct",
    "take_turn_route": "_take_turn_route",
    "give_back_turn_route": "_give_back_turn_route",
    "prepare_submit": "_prepare_submit",
    "submit_params": "_submit_params",
    "clarify_reply": "clarify_reply",
}
WANTED = set(PY.values())
# Real siblings the targets call through self / module scope.
SIBLINGS = {
    "_effort_word", "_stage_content_parts", "_take_turn_route",
    "_give_back_turn_route", "_sync_show_reasoning", "_lost_turn_notice",
    "_emit_print", "_stop",
}
ALL_DEFS = WANTED | SIBLINGS
# Methods invoked via self.<name>(...) inside the targets.
SELF_METHODS = {
    "_stage_content_parts", "_take_turn_route", "_give_back_turn_route",
    "_sync_show_reasoning", "_lost_turn_notice", "_emit_print", "_stop",
}
# service_tier is a property setter; extracted as the fset function.
CONSTS = {
    "INTERRUPT_SETTLE_SECONDS", "_POLL", "VOICE_STOPPED", "_RESULT_DEFAULTS",
    "_COUNTER_FROM_USAGE",
}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


class GatewayClientError(Exception):
    pass


class GatewayRPCError(GatewayClientError):
    pass


class CallStopped(Exception):
    pass


EXC_MAP = {
    "GatewayClientError": GatewayClientError, "GatewayRPCError": GatewayRPCError,
    "CallStopped": CallStopped, "KeyError": KeyError, "TypeError": TypeError,
    "ValueError": ValueError, "RuntimeError": RuntimeError,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        cls = EXC_MAP.get(r["__raise__"], RuntimeError)
        raise cls(r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        r = sc[min(i, len(sc) - 1)] if sc else None
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
    return str(o)


# ── extraction ──

def extract():
    import logging
    import threading
    import time
    import uuid as _uuid
    src = open(SRC, encoding="utf-8").read()

    class RecLogger:
        def debug(self, *a, **k): pass
        def info(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def error(self, *a, **k): pass

    ns = {
        "__name__": "hermes_cli.gateway_agent_extracted",
        "json": json, "os": os, "sys": sys, "time": time, "threading": threading,
        "Path": Path, "logger": RecLogger(),
        "GatewayClientError": GatewayClientError, "GatewayRPCError": GatewayRPCError,
        "CallStopped": CallStopped,
        "VOICE_STOPPED": "voice_stopped",
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
    # GatewayAgent methods. service_tier is a property; extract its SETTER as a
    # plain function (strip the @service_tier.setter decorator) for the harness.
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "GatewayAgent")
    for node in cls.body:
        if isinstance(node, ast.FunctionDef) and node.name in ALL_DEFS:
            if node.name == "service_tier":
                is_setter = any(isinstance(d, ast.Attribute) and d.attr == "setter"
                                for d in node.decorator_list)
                if not is_setter:
                    continue  # skip the @property getter
                node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = ALL_DEFS - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    return ns


# ── self / state / connection doubles ──

class StateDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def runs_route(self, model, provider):
        return seam("runs_route", {"model": model, "provider": provider})

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name == "info":
            return d.get("info")
        return d.get(name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_d")[name] = value


class ConnDouble:
    def __init__(self, st):
        self._st = st
        self.epoch = st.get("connection_epoch", 0)

    def call(self, method, **kwargs):
        return seam("call", {"method": method, **kwargs})

    def reattach(self, session_id, abandon=None):
        return seam("reattach", {"session_id": session_id})

    def start(self):
        return seam("start", {})

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
        if name in SELF_METHODS:
            f = ns[name]
            return lambda *a, **k: f(self, *a, **k)
        if name == "_state":
            return lambda: self.connection.state(st.get("session_id"))
        return st.get(name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_st")[name] = value


# ── recording stubs ──

def rec_print(*args, **kw):
    text = kw.get("sep", " ").join(str(a) for a in args)
    seam("print", {"text": text})


def install_stubs(ns):
    ns["print"] = rec_print
    ns["uuid"] = types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex=seam("uuid4_hex", {})))
    # agent.i18n.t
    agent = sys.modules.get("agent") or types.ModuleType("agent")
    agent.__path__ = []
    sys.modules["agent"] = agent
    i18n = types.ModuleType("agent.i18n")
    i18n.t = lambda key, **kw: seam("t", {"key": key})
    sys.modules["agent.i18n"] = i18n
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
    if func == "effort_word":
        return ns[name](inp.get("config")), None
    if func == "clarify_reply":
        cb = None
        if inp.get("callback_present"):
            cb = lambda questions: seam("clarify_callback", {"questions": questions})
        return ns[name](cb, inp.get("params") or {}), None
    st = json.loads(json.dumps(inp.get("st") or {}))
    # Live type invariant: _show_reasoning_sent is only ever None or the 3-TUPLE
    # written by _sync_show_reasoning. JSON injects it as a list, and the live
    # dedup `sent[:2] == (self.session_id, show)` is False for a list (list !=
    # tuple), which would silently disable the dedup on the PY side. Restore the
    # tuple so the self-model mirrors the live attribute; norm() maps it back to
    # a list for comparison.
    if isinstance(st.get("_show_reasoning_sent"), list):
        st["_show_reasoning_sent"] = tuple(st["_show_reasoning_sent"])
    self = SelfDouble(st, ns)
    f = ns[name]
    if func == "route_off_session":
        ret = f(self, inp.get("model", ""), inp.get("provider", ""), inp.get("alias", ""))
    elif func == "set_reasoning":
        ret = f(self, inp.get("config"), explicit=inp.get("explicit", False))
    elif func == "sync_show_reasoning":
        f(self); ret = None
    elif func == "service_tier":
        # the property setter function takes (self, tier)
        f(self, inp.get("tier")); ret = None
    elif func == "correct":
        ret = f(self, inp.get("verb", "steer"), inp.get("text"))
    elif func == "take_turn_route":
        ret = f(self)
    elif func == "give_back_turn_route":
        f(self); ret = None
    elif func == "prepare_submit":
        turn = types.SimpleNamespace(session_id=inp.get("turn_session_id", ""))
        ret = f(self, turn)
    elif func == "submit_params":
        ret = f(self, inp.get("user_message"), inp.get("persist_user_message"),
                inp.get("turn_author"))
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
        try:
            ret, st = drive(ns, func, inp)
        except Exception as ex:
            mism += 1
            print("PYERR %s: %r" % (func, ex))
            continue
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if st is not None:
            want["st"] = norm(st)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE gateway_agent_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE gateway_agent_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()