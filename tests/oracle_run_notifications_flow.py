#!/usr/bin/env python3
"""Oracle for the gateway/run_notifications.py notification-delivery FLOW
cores and leaves (run_notifications_flow.c).

AST-extracts the live module functions and GatewayNotificationsMixin methods
(no heavy gateway import), seeds the module constants (Assign + AnnAssign),
registers the extraction namespace as a real module (py3.14 dataclasses), and
replaces every live I/O (transport resolve, identity probe, notice-key helper,
clock/backoff, durable-claim ops, queue puts, profile scopes, heartbeat /
injection delivery, logging) with recording stubs returning the SAME scripted
results the C harness feeds its universal seam. Compares the ordered seam-call
trace plus the return value / raised-exception spec / mutated dedup state.

Input: stdin JSON lines from t_port_run_notifications_flow.
Summary: ORACLE run_notifications_flow: N cases, M mismatches. Fail-closed on
0 cases.
"""
import ast
import asyncio
import collections
import copy
import enum
import json
import logging
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_notifications.py")

WANTED = {
    "_mark_internal_notification", "_raw_process_event_session_id",
    "_event_route_key", "_completion_delivery_identity",
    "_served_notice_target_key", "_refusal_retry_at", "_held_reason",
    "_safe_delivery_transport", "_completion_identity_seen",
    "_mark_completions_delivered_locked", "_record_coalesced_completion_siblings",
    "_requeue_refused_watch_event", "_back_off_refused_completions",
    "_settle_durable_claim", "_watch_notification_metadata",
    "_drain_watch_notifications",
}

STATE_FUNCS = {
    "completion_identity_seen", "mark_completions_delivered_locked",
    "record_coalesced_completion_siblings", "requeue_refused_watch_event",
    "back_off_refused_completions",
}

CUR = {"func": None, "in": {}, "io": {}, "cursors": {}, "trace": [],
       "scopes": {}, "stf": None}


def rec(ev):
    CUR["trace"].append(ev)


def deepcopy(x):
    return copy.deepcopy(x)


def nsd(x):
    """SimpleNamespace -> dict (recursively) for trace-arg recording."""
    if isinstance(x, types.SimpleNamespace):
        return {k: nsd(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: nsd(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [nsd(v) for v in x]
    return x


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": deepcopy([str(a) if isinstance(a, BaseException) else a
                               for a in args])})

    def debug(self, fmt, *a, **k):
        self._rec(10, fmt, a)

    def info(self, fmt, *a, **k):
        self._rec(20, fmt, a)

    def warning(self, fmt, *a, **k):
        self._rec(30, fmt, a)

    def error(self, fmt, *a, **k):
        self._rec(40, fmt, a)

    def exception(self, fmt, *a, **k):
        self._rec(40, fmt, a)

    def log(self, level, fmt, *a, **k):
        self._rec(level, fmt, a)


Platform = enum.Enum("Platform", {
    "LOCAL": "local", "TELEGRAM": "telegram", "DISCORD": "discord",
    "WHATSAPP": "whatsapp", "WHATSAPP_CLOUD": "whatsapp_cloud", "SLACK": "slack",
    "SIGNAL": "signal", "MATTERMOST": "mattermost", "MATRIX": "matrix",
    "EMAIL": "email", "SMS": "sms", "DINGTALK": "dingtalk",
    "API_SERVER": "api_server", "WEBHOOK": "webhook",
    "MSGRAPH_WEBHOOK": "msgraph_webhook", "FEISHU": "feishu", "WECOM": "wecom",
    "WECOM_CALLBACK": "wecom_callback", "WEIXIN": "weixin",
    "BLUEBUBBLES": "bluebubbles", "QQBOT": "qqbot", "RELAY": "relay",
})


def extract():
    src = open(SRC, encoding="utf-8").read()
    modname = "gateway_run_notifications_extracted"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["logging"] = logging
    ns["asyncio"] = asyncio
    ns["logger"] = RecLogger()
    ns["Platform"] = Platform
    tree = ast.parse(src)
    flags = __import__("__future__").annotations.compiler_flag
    # constants first (plain-Name targets only; no env mutation)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    ns["logger"] = RecLogger()  # override the logging.getLogger assignment
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "GatewayNotificationsMixin":
            for sub in node.body:
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in WANTED:
                    sub.decorator_list = []  # staticmethods -> plain fns, self explicit
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec",
                                 flags=flags), ns)
    need = {"INTERNAL_NOTIFICATION_FOOTER", "_DURABLE_CLAIM_OPS",
            "_MAX_COMPLETION_INJECTION_ATTEMPTS", "_WATCH_INJECTION_RETRY_BASE_DELAY",
            "_COMPLETION_INJECTION_RETRY_MAX_DELAY"}
    missing = (need | WANTED) - set(ns)
    if missing:
        raise RuntimeError("extract failed to bind: %s" % sorted(missing))
    return ns


# ── stub modules for the in-function imports (leaf deps only) ──
STUB_MODULES = {
    "gateway.delivery": ["resolve_delivery_transport"],
    "gateway.session_identity": ["identity_of"],
    "agent.notification_presentation": ["diagnostic_process_event"],
    "tools.process_registry_notifications": [
        "completion_resumes_parent", "delegation_wakes_parent",
        "should_surface_notification"],
    "tools.async_delegation": [
        "drop_completion_delivery", "release_completion_delivery",
        "defer_completion_delivery", "complete_completion_delivery"],
    "gateway.run": ["_drain_gateway_watch_events", "_format_gateway_process_notification"],
    "gateway.run_profile_home": ["completion_event_scope_or_refuse"],
    "gateway.session_automation": ["deliver_heartbeat"],
}


def install_stub_modules():
    for pkg in ("gateway", "agent", "tools"):
        m = types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m
    for name, attrs in STUB_MODULES.items():
        m = types.ModuleType(name)
        for a in attrs:
            setattr(m, a, None)
        sys.modules[name] = m
        pkg, _, leaf = name.partition(".")
        setattr(sys.modules[pkg], leaf, m)


# ── scripted-seam machinery (mirrors the harness next_script/h_call) ──

def next_script(name):
    s = CUR["io"].get(name)
    if s is None:
        raise RuntimeError("no script for %s" % name)
    if isinstance(s, dict) and "const" in s:
        return deepcopy(s["const"])
    if isinstance(s, dict) and "queue" in s:
        q = s["queue"]
        cur = CUR["cursors"].get(name, 0)
        if not isinstance(q, list) or cur >= len(q):
            raise RuntimeError("script exhausted for %s" % name)
        CUR["cursors"][name] = cur + 1
        return deepcopy(q[cur])
    raise RuntimeError("bad script for %s" % name)


BUILTIN_EXC = {
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "RuntimeError": RuntimeError, "TypeError": TypeError, "ValueError": ValueError,
    "KeyError": KeyError, "LookupError": LookupError, "AttributeError": AttributeError,
    "OSError": OSError, "Exception": Exception,
}


def build_exc(cls, msg):
    klass = BUILTIN_EXC.get(cls)
    if klass is None:
        klass = type(cls, (Exception,), {})
    return klass(msg)


class ScopeDouble:
    def __init__(self, sid):
        self.id = sid

    async def __aenter__(self):
        spec = _record("scope_enter", {"id": self.id})
        return _materialize("scope_enter", spec)

    async def __aexit__(self, *exc):
        spec = _record("scope_exit", {"id": self.id})
        _materialize("scope_exit", spec)
        return False


class QueueDouble:
    def __init__(self, qid):
        self.id = qid
        self.puts = []

    def put(self, evt):
        spec = _record("queue_put", {"evt": deepcopy(evt)})
        self.puts.append(evt)
        return _materialize("queue_put", spec)


class LockDouble:
    def __init__(self):
        self.held = False

    def __enter__(self):
        self.held = True
        return self

    def __exit__(self, *a):
        self.held = False
        return False


def _record(name, args):
    try:
        spec = next_script(name)
    except RuntimeError as e:
        spec = {"__raise__": "RuntimeError", "message": str(e)}
    rec({"ev": name, "args": deepcopy(args), "ret": deepcopy(spec)})
    return spec


def _materialize(name, spec):
    if isinstance(spec, dict) and "__raise__" in spec:
        raise build_exc(spec["__raise__"], spec.get("message", ""))
    if spec is None:
        return None
    if name == "identity_of":
        return types.SimpleNamespace(**spec) if isinstance(spec, dict) else spec
    if name == "completion_event_scope_or_refuse":
        if isinstance(spec, str):
            return CUR["scopes"].setdefault(spec, ScopeDouble(spec))
        return spec
    if name == "_notice_target_key":
        return tuple(spec) if isinstance(spec, list) else spec
    return spec


def make_rec(name, argmap, async_=False):
    def body(a, k):
        return _materialize(name, _record(name, argmap(a, k)))
    if async_:
        async def stub(*a, **k):
            return body(a, k)
    else:
        def stub(*a, **k):
            return body(a, k)
    return stub


def evt_args(a, k):
    return {"evt": deepcopy(a[-1])}


ARGMAPS = {
    "_notice_target_key": lambda a, k: {"platform_value": a[0], "chat_id": a[1], "thread_id": a[2]},
    "monotonic": lambda a, k: {},
    "jittered_backoff": lambda a, k: dict({"attempt": a[0]}, **k),
    "identity_of": lambda a, k: {"source": nsd(a[0])},
    "resolve_delivery_transport": lambda a, k: {"platform": nsd(a[0]), "config": a[1], "adapters": a[2]},
    "drop_completion_delivery": lambda a, k: {"delegation_id": a[0], "claim_id": a[1]},
    "release_completion_delivery": lambda a, k: {"delegation_id": a[0], "claim_id": a[1]},
    "defer_completion_delivery": lambda a, k: {"delegation_id": a[0], "claim_id": a[1]},
    "complete_completion_delivery": lambda a, k: {"delegation_id": a[0], "claim_id": a[1]},
    "diagnostic_process_event": evt_args,
    "_local_completion_card": evt_args,
    "delegation_wakes_parent": evt_args,
    "completion_resumes_parent": evt_args,
    "should_surface_notification": evt_args,
    "format_gateway_process_notification": evt_args,
    "completion_event_scope_or_refuse": lambda a, k: {"evt": deepcopy(a[1]), "label": a[2]},
    "load_background_notifications_mode": lambda a, k: {},
    "deliver_heartbeat": lambda a, k: {"evt": deepcopy(a[1])},
    # instance-attribute plain fn: NO self is passed (synth_text, evt)
    "inject_watch_notification": lambda a, k: {"synth_text": a[0], "evt": deepcopy(a[1])},
    "_refusal_retry_at": lambda a, k: {"step": a[0]},
    "drain_gateway_watch_events": lambda a, k: {"queue": a[0].id},
}

ASYNC_SEAMS = {"completion_event_scope_or_refuse", "deliver_heartbeat",
               "inject_watch_notification"}

REAL = {}


def install_case(ns, func):
    # restore every ported def to REAL before installing this case's stubs
    for nm, fn in REAL.items():
        ns[nm] = fn
    ns["logger"] = RecLogger()
    recs = {name: make_rec(name, am, async_=name in ASYNC_SEAMS)
            for name, am in ARGMAPS.items()}
    # module-global seams (live name == seam name here)
    for nm in ("_notice_target_key", "jittered_backoff", "_local_completion_card",
               "completion_event_scope_or_refuse"):
        ns[nm] = recs[nm]
    if func in ("requeue_refused_watch_event", "back_off_refused_completions",
                "drain_watch_notifications"):
        ns["_refusal_retry_at"] = recs["_refusal_retry_at"]
    # in-function import seams: attribute is the LIVE name, recorder the SEAM name
    live_to_seam = {"_drain_gateway_watch_events": "drain_gateway_watch_events",
                    "_format_gateway_process_notification": "format_gateway_process_notification"}
    for modname, attrs in STUB_MODULES.items():
        for live in attrs:
            setattr(sys.modules[modname], live, recs[live_to_seam.get(live, live)])
    ns["time"] = types.SimpleNamespace(monotonic=recs["monotonic"])


def build_self(ns, inp):
    st = inp.get("state") or {}
    sd = types.SimpleNamespace()
    sd._completion_deliveries_inflight = {tuple(i) for i in st.get("inflight", [])}
    # live state is an OrderedDict: popitem(last=False) trims the OLDEST key
    sd._completion_deliveries_delivered = collections.OrderedDict(
        (tuple(i), None) for i in st.get("delivered", []))
    sd._completion_delivery_retention = st.get("retention", 100)
    sd._completion_delivery_lock = LockDouble()
    sd._completion_delivery_identity = ns["_completion_delivery_identity"]
    sd._mark_completions_delivered_locked = types.MethodType(
        ns["_mark_completions_delivered_locked"], sd)
    sd._completion_identity_seen = types.MethodType(ns["_completion_identity_seen"], sd)
    return sd


def state_out(sd):
    return {
        "inflight": sorted([list(t) for t in sd._completion_deliveries_inflight]),
        "delivered": [list(k) for k in sd._completion_deliveries_delivered],
        "lock_held": sd._completion_delivery_lock.held,
    }


def drive(ns, func, inp):
    if func == "mark_internal_notification":
        return ns["_mark_internal_notification"](inp.get("text", ""))
    if func == "raw_process_event_session_id":
        return ns["_raw_process_event_session_id"](inp.get("evt") or {})
    if func == "event_route_key":
        return ns["_event_route_key"](inp.get("evt") or {}, tuple(inp.get("fields", [])))
    if func == "completion_delivery_identity":
        return ns["_completion_delivery_identity"](inp.get("evt") or {})
    if func == "served_notice_target_key":
        return ns["_served_notice_target_key"](
            inp.get("profile"), inp.get("platform_value", ""),
            inp.get("chat_id"), inp.get("thread_id"))
    if func == "refusal_retry_at":
        return ns["_refusal_retry_at"](inp.get("step", 0))
    if func == "held_reason":
        source = types.SimpleNamespace(**(inp.get("source") or {}))
        return ns["_held_reason"](source, inp.get("platform", ""),
                                  inp.get("multiplexed", False))
    if func == "safe_delivery_transport":
        p = inp.get("platform")
        platform = types.SimpleNamespace(**p) if isinstance(p, dict) else p
        return ns["_safe_delivery_transport"](platform, inp.get("config"),
                                              inp.get("adapters"),
                                              profile=inp.get("profile"))
    if func == "completion_identity_seen":
        sd = build_self(ns, inp)
        CUR["stf"] = lambda: state_out(sd)
        return ns["_completion_identity_seen"](sd, tuple(inp.get("identity", [])),
                                               claim=inp.get("claim", False))
    if func == "mark_completions_delivered_locked":
        sd = build_self(ns, inp)
        CUR["stf"] = lambda: state_out(sd)
        ns["_mark_completions_delivered_locked"](
            sd, [tuple(i) for i in inp.get("identities", [])])
        return None
    if func == "record_coalesced_completion_siblings":
        sd = build_self(ns, inp)
        CUR["stf"] = lambda: state_out(sd)
        ns["_record_coalesced_completion_siblings"](sd, inp.get("events", []))
        return None
    if func == "requeue_refused_watch_event":
        evt = deepcopy(inp.get("evt") or {})
        queue = QueueDouble(inp.get("queue_id", "q"))
        CUR["stf"] = lambda: {"evt": evt}
        ns["_requeue_refused_watch_event"](queue, evt)
        return None
    if func == "back_off_refused_completions":
        group = deepcopy(inp.get("group") or [])
        CUR["stf"] = lambda: {"group": group}
        ns["_back_off_refused_completions"](group)
        return None
    if func == "settle_durable_claim":
        ns["_settle_durable_claim"](inp.get("kind", ""), inp.get("delegation_id", ""),
                                   inp.get("claim_id", ""))
        return None
    if func == "watch_notification_metadata":
        src = inp.get("source") or {}
        source = types.SimpleNamespace(platform=Platform(src.get("platform", "local")))
        return ns["_watch_notification_metadata"](inp.get("evt") or {}, source)
    if func == "drain_watch_notifications":
        sd = types.SimpleNamespace()
        sd._load_background_notifications_mode = make_rec(
            "load_background_notifications_mode",
            ARGMAPS["load_background_notifications_mode"])
        sd._inject_watch_notification = make_rec(
            "inject_watch_notification", ARGMAPS["inject_watch_notification"],
            async_=True)
        sd._requeue_refused_watch_event = ns["_requeue_refused_watch_event"]
        queue = QueueDouble(inp.get("queue_id", "q"))
        asyncio.run(ns["_drain_watch_notifications"](sd, queue))
        return None
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
    install_stub_modules()
    for nm in WANTED:
        REAL[nm] = ns[nm]
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
        CUR.update(func=func, **{"in": inp}, io=inp.get("io") or {},
                   cursors={}, trace=[], scopes={}, stf=None)
        install_case(ns, func)
        st = None
        try:
            ret = drive(ns, func, inp)
        except BaseException as e:  # SystemExit/CancelledError must surface too
            ret = {"__raise__": type(e).__name__, "message": str(e)}
        if CUR["stf"] is not None:
            st = CUR["stf"]()
        want = {"ret": ret}
        if st is not None:
            want["st"] = st
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_notifications_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())