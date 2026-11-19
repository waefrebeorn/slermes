#!/usr/bin/env python3
"""Oracle for the gateway/run_busy.py busy/queue-admission FLOW cores
(src/gateway/run_busy_flow.c).

AST-extracts the live module slot helpers and the ENTIRE GatewayBusySessionMixin
class (future-annotations compile, so no heavy gateway import), seeds the
class constants (_BUSY_REJECT_TEXT is read live from gateway/run.py), and
replaces every unported collaborator (_peek/_session_state, adapter lookup,
pinned inputs, MessageEvent construction, steer-origin text, ack compose/send,
env/clock, authority probes, agent verbs, logging) with recording stubs that
return the SAME scripted results the C harness feeds its universal seam.

State is injected as a JSON mirror (events/agents/sessions/adapters keyed by
id); the oracle materializes per-id object doubles so live `is` comparisons and
in-place mutations agree with the C mirror, then norms the final state back to
JSON. Compares ordered seam trace + return + final state. BaseException is
caught per case (scripted SystemExit/CancelledError propagate through live
`except Exception` exactly as in the port).

Input: stdin JSON lines from t_port_run_busy_flow.
Summary: ORACLE run_busy_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import asyncio
import builtins
import contextlib
import contextvars
import functools
import json
import logging
import os
import sys
import time
import types
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_busy.py")
RUN_SRC = os.path.join(QUARRY, "gateway", "run.py")

WANTED_MOD = {"_strip_slot", "_tail_has_slot", "_same_chat_key_slots"}
SENTINEL = object()   # _AGENT_PENDING_SENTINEL double (identity-scoped)

CUR = {"func": None, "in": {}, "trace": [], "reg": {}, "io": {}, "pos": {}}


def rec(ev):
    CUR["trace"].append(ev)


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": [str(a) if isinstance(a, BaseException) else a for a in args]})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)


# ── script access (mirrors the harness stub semantics) ─────────────────────
def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if cls is None and kind == "CancelledError":
        cls = asyncio.CancelledError
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def next_script(name, optional=False):
    io = CUR["io"]
    if name not in io:
        if optional:
            return None
        raise RuntimeError("no script for " + name)
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            raise RuntimeError("script exhausted for " + name)
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam_rec(name, args, optional=False):
    r = next_script(name, optional)
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


# ── object doubles (identity by id; mutations reflect in the norm) ─────────
class ObjDouble:
    def __init__(self, fields, methods=None):
        self.__dict__["_fields"] = dict(fields)
        self.__dict__["_methods"] = dict(methods or {})

    def __getattr__(self, k):
        d = self.__dict__
        if k in d["_methods"]:
            return d["_methods"][k]
        f = d["_fields"]
        if k in f:
            return f[k]
        raise AttributeError("'%s' object has no attribute %r" % (type(self).__name__, k))

    def __setattr__(self, k, v):
        self.__dict__["_fields"][k] = v


class EventDouble(ObjDouble):
    _defaults = {"internal": False, "_admitted_before_drain": False, "_gateway_accepted": False}

    def __init__(self, eid, fields):
        super().__init__(fields)
        self.__dict__["id"] = eid

    def __getattr__(self, k):
        try:
            return super().__getattr__(k)
        except AttributeError:
            if k in EventDouble._defaults:
                return EventDouble._defaults[k]
            raise

    def get_command(self):
        return self.__dict__["_fields"].get("command")


class LockDouble:
    def __init__(self, aid):
        self.aid = aid

    def __enter__(self):
        r = next_script("lock_enter", optional=True)
        rec({"ev": "lock_enter", "args": {"agent_id": self.aid}, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            raise make_exc(r)
        return self

    def __exit__(self, *a):
        rec({"ev": "lock_exit", "args": {"agent_id": self.aid}, "ret": None})
        return False


class AgentDouble(ObjDouble):
    def __init__(self, aid, fields, reg):
        super().__init__(fields)
        self.__dict__["id"] = aid
        self.__dict__["reg"] = reg
        self.__dict__["_cache"] = {}

    def __getattr__(self, k):
        d = self.__dict__
        if k == "_active_children":
            if k not in d["_fields"]:
                raise AttributeError("'AgentDouble' object has no attribute %r" % k)
            if k in d["_cache"]:
                return d["_cache"][k]
            conv = self._convert(d["_fields"][k])
            d["_cache"][k] = conv
            return conv
        if k == "_active_children_lock":
            return LockDouble(d["id"]) if d["_fields"].get(k) else None
        return super().__getattr__(k)

    def _convert(self, raw):
        if raw is None:
            return None
        kind = raw.get("kind")
        items = raw.get("items") or []
        ref = lambda i: agent_ref(i)   # noqa: E731
        if kind == "list":
            return [ref(i) for i in items]
        if kind == "tuple":
            return tuple(ref(i) for i in items)
        if kind == "set":
            return {ref(i) for i in items}
        if kind == "dict":
            return {i: None for i in items}
        if kind == "str":
            return raw.get("items", "")
        return None


def agent_ref(aid):
    if aid is None:
        return None
    if aid == "__sentinel__":
        return SENTINEL
    return CUR["reg"]["agents"][aid]


def mat(x):
    if isinstance(x, dict):
        return SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(v) for v in x]
    if isinstance(x, tuple):
        return tuple(mat(v) for v in x)
    return x


def norm(x):
    if isinstance(x, EventDouble):
        return x.id
    if isinstance(x, ObjDouble):
        return {k: norm(v) for k, v in x.__dict__["_fields"].items()}
    if isinstance(x, SimpleNamespace):
        return {k: norm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    return x


def norm_event_fields(ev):
    return {k: norm(v) for k, v in ev.__dict__["_fields"].items()}


def norm_state(reg):
    return {
        "events": {eid: norm_event_fields(ev) for eid, ev in reg.get("events", {}).items()},
        "agents": {aid: norm(ag) for aid, ag in reg.get("agents", {}).items()},
        "sessions": {k: norm(s) for k, s in reg.get("sessions", {}).items()},
        "adapters": {aid: norm(ad) for aid, ad in reg.get("adapters", {}).items()},
    }


# ── extraction ──────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("run_busy_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({"logger": RecLogger(), "t": lambda key, **kw: key,
               "contextlib": contextlib, "asyncio": asyncio, "os": os, "time": time,
               "json": json, "functools": functools, "contextvars": contextvars,
               "logging": logging, "MessageEvent": None})
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_MOD:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayBusySessionMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
    # _BUSY_REJECT_TEXT lives on GatewayRunner (gateway/run.py); read it live.
    rtree = ast.parse(open(RUN_SRC, encoding="utf-8").read())
    for node in ast.walk(rtree):
        if isinstance(node, ast.ClassDef) and node.name == "GatewayRunner":
            for sub in node.body:
                tgt = None
                if isinstance(sub, ast.Assign) and any(
                        isinstance(t, ast.Name) and t.id == "_BUSY_REJECT_TEXT" for t in sub.targets):
                    tgt = sub
                elif isinstance(sub, ast.AnnAssign) and isinstance(sub.target, ast.Name) \
                        and sub.target.id == "_BUSY_REJECT_TEXT" and sub.value:
                    tgt = sub
                if tgt is not None:
                    tmp = {}
                    exec(compile(ast.Module(body=[tgt], type_ignores=[]), RUN_SRC, "exec"), tmp)
                    ns["GatewayBusySessionMixin"]._BUSY_REJECT_TEXT = tmp["_BUSY_REJECT_TEXT"]
    for name in ("_strip_slot", "_same_chat_key_slots", "GatewayBusySessionMixin"):
        assert name in ns, "extraction missed %s" % name
    assert hasattr(ns["GatewayBusySessionMixin"], "_BUSY_REJECT_TEXT"), "reject table missing"
    return ns


def install_globals(ns):
    ns["logger"] = RecLogger()

    def message_event(**kw):
        args = drop_none({k: norm(v) for k, v in kw.items()})
        r = next_script("MessageEvent")
        if isinstance(r, dict) and "__raise__" in r:
            rec({"ev": "MessageEvent", "args": args, "ret": r})
            raise make_exc(r)
        eid = r.get("id") if isinstance(r, dict) else None
        rec({"ev": "MessageEvent", "args": args, "ret": eid})
        ev = EventDouble(eid, {k: mat(v) for k, v in r.items()})
        CUR["reg"]["events"][eid] = ev
        return ev
    ns["MessageEvent"] = message_event

    class EnvDouble:
        def get(self, key, default=None):
            return seam_rec("env", {"key": key, "default": default})
    ns["os"] = SimpleNamespace(environ=EnvDouble())
    ns["time"] = SimpleNamespace(time=lambda: seam_rec("time", {}))

    gateway_pkg = types.ModuleType("gateway")
    run_mod = types.ModuleType("gateway.run")
    run_mod._AGENT_PENDING_SENTINEL = SENTINEL
    gateway_pkg.run = run_mod
    auth_mod = types.ModuleType("gateway.session_authorities")
    auth_mod.active_authority = lambda runner: (
        lambda r: SimpleNamespace(**r) if isinstance(r, dict) else None
    )(seam_rec("active_authority", {}))
    ingress_mod = types.ModuleType("gateway.session_ingress")
    ingress_mod.executing_admission = SimpleNamespace(
        get=lambda: seam_rec("executing_admission_get", {}))
    gateway_pkg.session_authorities = auth_mod
    gateway_pkg.session_ingress = ingress_mod
    sys.modules["gateway"] = gateway_pkg
    sys.modules["gateway.run"] = run_mod
    sys.modules["gateway.session_authorities"] = auth_mod
    sys.modules["gateway.session_ingress"] = ingress_mod
# ── per-case materialization + runner wiring ───────────────────────────────
def make_session(key, f, reg):
    conv = f.get("conversation") or {}
    qe = [reg["events"][eid] for eid in conv.get("queued_events", [])]
    cfields = {**conv, "queued_events": qe}
    turn = ObjDouble(dict(f.get("turn") or {}))

    def running_event():
        r = seam_rec("running_event", {"session_key": key})
        return reg["events"][r] if r else None
    turn.__dict__["_methods"]["running_event"] = running_event
    sess = ObjDouble({"conversation": ObjDouble(cfields), "turn": turn})
    sess.__dict__["_key"] = key
    return sess


def materialize(inp):
    st = inp.get("state") or {}
    reg = {"events": {}, "agents": {}, "adapters": {}, "sessions": {}}
    for eid, f in (st.get("events") or {}).items():
        reg["events"][eid] = EventDouble(eid, {k: mat(v) for k, v in f.items()})
    for aid, f in (st.get("agents") or {}).items():
        reg["agents"][aid] = AgentDouble(aid, f, reg)
    for aid, f in (st.get("adapters") or {}).items():
        fields = dict(f)
        pm = fields.get("_pending_messages")
        if isinstance(pm, dict):
            fields["_pending_messages"] = {k: reg["events"][v] for k, v in pm.items()}
        ad = ObjDouble(fields)
        ad.__dict__["id"] = aid
        reg["adapters"][aid] = ad
    for k, f in (st.get("sessions") or {}).items():
        reg["sessions"][k] = make_session(k, f, reg)
    return reg


def install_agent_methods(reg, inp, func):
    child_ids = set()
    for ag in reg["agents"].values():
        ch = ag.__dict__["_fields"].get("_active_children")
        if isinstance(ch, dict):
            child_ids.update(ch.get("items") or [])
    verb = inp.get("verb")
    for aid, ag in reg["agents"].items():
        f = ag.__dict__["_fields"]
        m = ag.__dict__["_methods"]
        is_child = aid in child_ids
        # Children: steer installed only when callable (live callable() gate).
        # Parents: always installed; a "missing" attribute is a scripted
        # AttributeError raise so both sides record the seam event.
        if not is_child or f.get("steer_callable"):
            seam = "child_steer" if is_child else "agent_steer"
            idkey = "child_id" if is_child else "agent_id"

            def steer(text, aid=aid, seam=seam, idkey=idkey):
                return seam_rec(seam, {idkey: aid, "text": text})
            m["steer"] = steer

        def interrupt(text, aid=aid):
            return seam_rec("agent_interrupt", {"agent_id": aid, "text": text})
        m["interrupt"] = interrupt
        if func == "_try_agent_verb" and verb and verb != "steer":
            def verb_fn(text, aid=aid, verb=verb):
                return seam_rec("agent_verb", {"agent_id": aid, "verb": verb, "text": text})
            m[verb] = verb_fn


def make_runner(ns, reg):
    runner = ns["GatewayBusySessionMixin"]()

    def peek(key):
        s = reg["sessions"].get(key)
        rec({"ev": "_peek_session_state", "args": {"session_key": key},
             "ret": norm(s) if s is not None else None})
        return s

    def sess(key):
        s = reg["sessions"].get(key)
        if s is None:
            s = make_session(key, {"conversation": {"queued_events": []}, "turn": {}}, reg)
            reg["sessions"][key] = s
        rec({"ev": "_session_state", "args": {"session_key": key}, "ret": norm(s)})
        return s
    runner._peek_session_state = peek
    runner._session_state = sess

    def adapter_for(source):
        r = seam_rec("_delivery_adapter_for", drop_none({"source": norm(source)}))
        return reg["adapters"][r] if r else None
    runner._delivery_adapter_for = adapter_for

    def pinned(session_key, arg2, source, internal=True):
        r = seam_rec("_pinned_channel_inputs", drop_none(
            {"session_key": session_key, "source": norm(source), "internal": internal}))
        return (r[0], mat(r[1]))
    runner._pinned_channel_inputs = pinned
    runner._steer_text_with_origin = lambda text, event: seam_rec(
        "steer_text_with_origin", {"text": text, "event_id": event.id})
    runner._busy_interrupt_text = lambda event: seam_rec(
        "busy_interrupt_text", {"event_id": event.id})
    runner._busy_steer_ack_enabled = lambda event, session_key: seam_rec(
        "busy_steer_ack_enabled", {"event_id": event.id, "session_key": session_key})

    def compose(event, now, busy_state, running_agent, **kw):
        bs_key = busy_state.__dict__["_key"] if busy_state is not None else None
        ag_id = None
        if running_agent is SENTINEL:
            ag_id = "__sentinel__"
        elif isinstance(running_agent, AgentDouble):
            ag_id = running_agent.id
        args = drop_none({"event_id": event.id, "now": now, "busy_state_key": bs_key,
                          "running_agent_id": ag_id, **kw})
        return seam_rec("compose_busy_ack_message", args)
    runner._compose_busy_ack_message = compose

    async def send_ack(event, adapter, message):
        return seam_rec("send_busy_ack_reply", drop_none(
            {"event_id": event.id,
             "adapter_id": adapter.id if adapter is not None else None,
             "message": message}))
    runner._send_busy_ack_reply = send_ack
    runner._gateway_plain_command_handlers = lambda: dict(
        seam_rec("gateway_plain_command_handlers", {}))
    return runner


def drive(ns, func, inp):
    reg = materialize(inp)
    CUR["reg"] = reg
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    install_agent_methods(reg, inp, func)
    runner = make_runner(ns, reg)

    def ev(eid):
        return reg["events"][eid] if eid else None

    def ad(aid):
        return reg["adapters"][aid] if aid else None
    key = inp.get("session_key")

    if func == "_strip_slot":
        return ns["_strip_slot"](inp.get("text", ""), inp.get("slot", ""))
    if func == "_same_chat_key_slots":
        return ns["_same_chat_key_slots"](inp.get("key", ""), prefix=inp.get("prefix", ""),
                                          chat_id=inp.get("chat_id", ""),
                                          scope_id=inp.get("scope_id"))
    if func == "_queue_during_drain_enabled":
        sj = inp.get("self") or {}
        runner._busy_input_mode = sj.get("_busy_input_mode")
        runner._restart_requested = sj.get("_restart_requested")
        return runner._queue_during_drain_enabled(inp.get("busy_input_mode"))
    if func == "_overflow_queue":
        return runner._overflow_queue(key)
    if func == "_enqueue_fifo":
        runner._enqueue_fifo(key, ev(inp.get("event_id")), ad(inp.get("adapter_id")))
        return None
    if func == "_requeue_event_at_front":
        runner._requeue_event_at_front(key, ev(inp.get("event_id")), ad(inp.get("adapter_id")))
        return None
    if func == "_take_wake_admitted_before_drain":
        return runner._take_wake_admitted_before_drain(key, ad(inp.get("adapter_id")))
    if func == "_queue_depth":
        return runner._queue_depth(key, adapter=ad(inp.get("adapter_id")))
    if func == "_merge_leftover_steer":
        return runner._merge_leftover_steer(inp.get("result"), ad(inp.get("adapter_id")),
                                            mat(inp.get("source")), key,
                                            ev(inp.get("pending_event_id")), inp.get("pending"))
    if func == "_defer_past_exhaustion_reset":
        runner._defer_past_exhaustion_reset(inp.get("result"), ad(inp.get("adapter_id")),
                                            mat(inp.get("source")), key,
                                            ev(inp.get("pending_event_id")))
        return None
    if func == "_requeue_at_depth_cap":
        runner._requeue_at_depth_cap(key, mat(inp.get("source")),
                                     ev(inp.get("pending_event_id")), inp.get("pending"))
        return None
    if func == "_carry_over_event":
        return runner._carry_over_event(key, mat(inp.get("source")), inp.get("text", ""))
    if func == "_is_goal_continuation_event":
        x = inp.get("event_or_text")
        if isinstance(x, dict):
            x = EventDouble("arg", {k: mat(v) for k, v in x.items()})
        return runner._is_goal_continuation_event(x)
    if func == "_steer_active_subagents":
        return runner._steer_active_subagents(agent_ref(inp.get("running_agent")),
                                              inp.get("text", ""))
    if func == "_try_agent_verb":
        return runner._try_agent_verb(agent_ref(inp.get("running_agent")), inp.get("verb", ""),
                                      inp.get("text", ""), key, event=ev(inp.get("event_id")))
    if func == "_busy_slash_route":
        return runner._busy_slash_route(SimpleNamespace(**(inp.get("cmd_def") or {})))
    if func == "_busy_interrupt_and_ack":
        steer = SimpleNamespace(**(inp.get("steer") or {}))
        bsk = inp.get("busy_state_key")
        bs = reg["sessions"].get(bsk) if bsk else None
        asyncio.run(runner._busy_interrupt_and_ack(
            ev(inp.get("event_id")), key, ad(inp.get("adapter_id")), bs,
            agent_ref(inp.get("running_agent")), steer))
        return None
    if func == "_command_turn_admitted":
        return runner._command_turn_admitted(ev(inp.get("event_id")), key)
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    ns = extract()
    install_globals(ns)
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
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["reg"] = {}
        try:
            ret = drive(ns, func, inp)
            want_ret = norm(ret)
        except BaseException as e:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
        want = {"ret": want_ret, "trace": CUR["trace"], "state": norm_state(CUR["reg"])}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_busy_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())