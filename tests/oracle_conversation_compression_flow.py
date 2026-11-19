#!/usr/bin/env python3
"""Oracle for port_conversation_compression_flow.c -- drives the LIVE
agent/conversation_compression.py compression-pipeline FLOW with SCRIPTED I/O
and compares the DECISION SEQUENCE against the C port.

The harness (tests/t_port_conversation_compression_flow.c) emits
{"func","in","out"} lines; this oracle replays the same scripted inputs
through the live Python (recording stubs at the I/O seams) and diffs.

Flow parity doctrine: the decision core performs no I/O; every live side
effect (summary call, clock, tokenizer estimate, DB write, telemetry, warning,
uuid, future wait) is monkeypatched with a recording stub fed by the same
script the C seams consume. logger.* calls are diagnostic and not traced
(the logger is disabled), EXCEPT the stall-retry/wait families where the log
branch IS the observable decision (classified by format string).

Fail-closed: unknown func / bad harness line / 0 cases -> FATAL exit 2.
"""
import os
import sys
import json
import copy
import types
import logging
import threading
import contextlib
import concurrent.futures
from types import SimpleNamespace as NS

logging.disable(logging.CRITICAL)

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(os.path.dirname(_REPO)))
os.environ.setdefault("USERPROFILE", os.path.dirname(os.path.dirname(_REPO)))
os.environ.setdefault("LOCALAPPDATA",
                      os.path.join(os.path.dirname(os.path.dirname(_REPO)), "AppData", "Local"))
os.environ.setdefault("HERMES_HOME", os.path.join(_REPO, ".testbin", "dbg_home"))

for _quarry in ("~/hermes-agent",):
    _p = os.path.expanduser(_quarry)
    if os.path.isdir(os.path.join(_p, "agent")):
        sys.path.insert(0, _p)
        break

# Stub the third-party packages the quarry import chain reaches at load time.
for _name in ("ruamel", "ruamel.yaml"):
    if _name not in sys.modules:
        sys.modules[_name] = types.ModuleType(_name)
sys.modules["ruamel"].yaml = sys.modules["ruamel.yaml"]


class _YAML:
    def __init__(self, *a, **k):
        raise RuntimeError("oracle stub: ruamel.yaml is not available")


sys.modules["ruamel.yaml"].YAML = _YAML
_err = types.ModuleType("ruamel.yaml.error")


class YAMLError(Exception):
    pass


_err.YAMLError = YAMLError
sys.modules["ruamel.yaml"].error = _err
sys.modules["ruamel.yaml.error"] = _err
for _sub in ("resolver", "parser", "composer", "constructor", "representer",
             "serializer", "emitter", "scanner", "loader", "dumper", "tokens",
             "events", "nodes", "cyaml", "main", "comments", "docinfo",
             "scalarstring", "scalarint", "scalarfloat", "scalarbool",
             "timestamp", "util", "compat", "configobj", "reader", "writer"):
    _full = "ruamel.yaml." + _sub
    if _full not in sys.modules:
        _sm = types.ModuleType(_full)
        setattr(sys.modules["ruamel.yaml"], _sub, _sm)
        sys.modules[_full] = _sm


class VersionedResolver:
    pass


sys.modules["ruamel.yaml.resolver"].VersionedResolver = VersionedResolver
_req = types.ModuleType("requests")


class _Resp:
    status_code = 500
    text = ""
    content = b""


_req.get = lambda *a, **k: _Resp()
_req.RequestException = type("RequestException", (Exception,), {})
sys.modules.setdefault("requests", _req)

try:
    from agent import conversation_compression as cc
    from agent import context_compressor as CX
    from agent.context_engine import CompactionProgressText
    import agent.auxiliary_client as AX
except ImportError:
    _venv = os.path.expanduser("~/hermes-agent/.venv/Scripts/python.exe")
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def fatal(msg):
    print(f"FATAL conversation_compression_flow: {msg}")
    sys.exit(2)


# ---------------------------------------------------------------- helpers

def jnorm(x):
    if isinstance(x, NS):
        return {k: jnorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {str(k): jnorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [jnorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(jnorm(v) for v in x)
    if isinstance(x, (threading.Lock,)):
        return True
    if isinstance(x, threading.local):
        d = {"__local__": True}
        d.update({k: jnorm(v) for k, v in vars(x).items()})
        return d
    return x


def norm_state(obj):
    try:
        return jnorm(vars(obj))
    except TypeError:
        return {}


def flow_state(obj, seams=()):
    """Agent/compressor state observation window: the instance's own attributes
    minus the oracle-injected seam doubles (scripted I/O handles, not state).
    Slotted minimal doubles (no __dict__) are read through their slots."""
    try:
        d = dict(vars(obj))
    except TypeError:
        d = {}
        for cls in type(obj).__mro__:
            for s in getattr(cls, "__slots__", ()):
                if isinstance(s, str) and hasattr(obj, s):
                    d[s] = getattr(obj, s)
    return {k: jnorm(v) for k, v in d.items() if k not in seams}


class Frozen:
    # Minimal double: vars() raises TypeError, but the timeout mirror attribute
    # is writable (the live fallback path sets it).
    __slots__ = ("_last_compression_timed_out",)


def mk_state_obj(d, frozen=False):
    if frozen:
        return Frozen()
    o = NS()
    for k, v in (d or {}).items():
        if v is True and k in ("_compression_attempt_serial_lock",
                               "_context_compression_timeout_state_lock"):
            setattr(o, k, threading.Lock())
        elif isinstance(v, dict) and v.get("__local__"):
            loc = threading.local()
            for lk, lv in v.items():
                if lk != "__local__":
                    setattr(loc, lk, lv)
            setattr(o, k, loc)
        else:
            setattr(o, k, copy.deepcopy(v))
    return o


def kind_value(v, kind):
    if kind == "int":
        return int(v)
    if kind == "float":
        return float(v)
    if kind == "bool":
        return bool(v)
    if kind == "str":
        return str(v)
    if kind == "null":
        return None
    return None  # missing


@contextlib.contextmanager
def stub_modules(mapping):
    """Temporarily register stub modules (with parent packages) in sys.modules."""
    saved = {}
    created = []
    links = []
    try:
        for name, attrs in mapping.items():
            parts = name.split(".")
            for i in range(1, len(parts) + 1):
                pn = ".".join(parts[:i])
                if pn not in sys.modules:
                    m = types.ModuleType(pn)
                    m.__path__ = []
                    sys.modules[pn] = m
                    created.append(pn)
                    if i > 1:
                        parent = sys.modules[".".join(parts[:i - 1])]
                        setattr(parent, parts[i - 1], m)
                        links.append((parent, parts[i - 1]))
            if name not in saved:
                saved[name] = sys.modules.get(name)
            if attrs is None:
                sys.modules[name] = None  # import raises ImportError
            else:
                m = sys.modules[name]
                if not isinstance(m, types.ModuleType):
                    m = types.ModuleType(name)
                    sys.modules[name] = m
                for k, v in attrs.items():
                    setattr(m, k, v)
        yield
    finally:
        for name, old in saved.items():
            if old is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = old
        for parent, attr in reversed(links):
            try:
                delattr(parent, attr)
            except AttributeError:
                pass
        for pn in reversed(created):
            if pn not in saved:
                sys.modules.pop(pn, None)


@contextlib.contextmanager
def patched(obj, name, value):
    had = hasattr(obj, name)
    old = getattr(obj, name, None)
    setattr(obj, name, value)
    try:
        yield
    finally:
        if had:
            setattr(obj, name, old)
        else:
            try:
                delattr(obj, name)
            except AttributeError:
                pass


def rec(ev, name, **kw):
    e = {"a": name}
    e.update(kw)
    ev.append(e)
    return e


def ret_ev(ev, **kw):
    rec(ev, "ret", **kw)


def telemetry_stub(ev, kind):
    def emit(agent, started_at, *a, **kw):
        if kind == "aborted":
            rec(ev, "telemetry_aborted", started_at=started_at,
                failure_class=a[0] if a else kw.get("failure_class"),
                seed=a[1] if len(a) > 1 else kw.get("attempt_seed"))
        else:
            rec(ev, "telemetry_bypassed",
                commit_status=kw.get("commit_status"),
                failure_class=kw.get("failure_class"),
                approx_tokens=kw.get("approx_tokens"),
                route=kw.get("route"),
                seed=kw.get("attempt_seed"))
    return emit


def existing_prompt_wrapper(ev, orig):
    def wrapped(agent, system_message):
        cached = getattr(agent, "_cached_system_prompt", None)
        built = not cached
        r = orig(agent, system_message)
        rec(ev, "existing_system_prompt", built=built, ret=r)
        return r
    return wrapped


def warning_agent(agent, ev, raises=False):
    def emit(msg):
        rec(ev, "emit_warning", msg=msg, raised=raises)
        if raises:
            raise RuntimeError("emit failed")
    agent._emit_warning = emit
    return agent


HANDLERS = {}


def handler(name):
    def deco(fn):
        HANDLERS[name] = fn
        return fn
    return deco


# ---------------------------------------------------------------- pure helpers

@handler("compaction_status_kind")
def h_status_kind(i):
    text = i.get("text")
    if i.get("marked"):
        text = CompactionProgressText(text or "")
    return cc.compaction_status_kind(i.get("kind", ""), text)


@handler("steer_markers")
def h_steer_markers(i):
    if i.get("import_error"):
        mods = {"agent.prompt_builder": None}
    else:
        mods = {"agent.prompt_builder": {"STEER_MARKER_OPEN": i.get("open", ""),
                                          "STEER_MARKER_CLOSE": i.get("close", "")}}
    with stub_modules(mods):
        return list(cc._steer_markers())



_FALLBACK_MARKERS = ("[OUT-OF-BAND USER MESSAGE", "[/OUT-OF-BAND USER MESSAGE]")


@handler("message_contains_busy_steer")
def h_contains_steer(i):
    markers = tuple(i["markers"]) if i.get("markers") else _FALLBACK_MARKERS
    with patched(cc, "_steer_markers", lambda: markers):
        return cc._message_contains_busy_steer(i.get("message"))


@handler("extract_steer_text_from_message")
def h_extract_steer(i):
    markers = tuple(i["markers"]) if i.get("markers") else _FALLBACK_MARKERS
    with patched(cc, "_steer_markers", lambda: markers):
        return cc._extract_steer_text_from_message(i.get("message"))


@handler("compressed_has_busy_steer")
def h_compressed_steer(i):
    markers = tuple(i["markers"]) if i.get("markers") else _FALLBACK_MARKERS
    with patched(cc, "_steer_markers", lambda: markers):
        return cc._compressed_has_busy_steer(i.get("messages") or [])


@handler("messages_match_scoped_identity")
def h_scoped_identity(i):
    return cc._messages_match_scoped_identity(i.get("left"), i.get("right"))


@handler("stamp_scoped_twins")
def h_stamp_twins(i):
    targets = copy.deepcopy(i.get("targets") or [])
    cc._stamp_scoped_twins(targets, i.get("source") or {},
                           exact_counts_stamped=bool(i.get("exact_counts_stamped")))
    return targets


@handler("todo_snapshot_is_only_content")
def h_todo_only(i):
    return cc._todo_snapshot_is_only_content(i.get("content"), i.get("stripped"))


@handler("direct_messages_for_pre_compress_memory")
def h_direct_messages(i):
    return cc._direct_messages_for_pre_compress_memory(i.get("messages"))


@handler("prior_timeout_failures")
def h_prior_failures(i):
    comp = NS()
    if i.get("kind") != "missing":
        comp._consecutive_timeout_failures = kind_value(i.get("count"), i.get("kind"))
    return cc._prior_timeout_failures(NS(context_compressor=comp))


@handler("request_exceeds_model_window")
def h_exceeds_window(i):
    comp = NS()
    if i.get("window_kind") != "missing":
        comp.context_length = kind_value(i.get("window"), i.get("window_kind"))
    tokens = kind_value(i.get("tokens"), i.get("tokens_kind")) \
        if i.get("tokens_kind") != "missing" else None
    return cc.request_exceeds_model_window(NS(context_compressor=comp), tokens)


@handler("stall_source_fingerprint")
def h_fingerprint(i):
    comp = NS(summary_model=i.get("summary_model"))
    agent = NS(model=i.get("agent_model"), context_compressor=comp)
    approx = kind_value(i.get("approx_tokens"), i.get("tokens_kind")) \
        if i.get("tokens_kind") not in (None, "missing") else i.get("approx_tokens")
    return cc._stall_source_fingerprint(agent, i.get("messages"), approx)


@handler("restore_prune_rearm_tokens")
def h_restore_prune(i):
    comp = mk_state_obj(i.get("state"))
    cc._restore_prune_rearm_tokens(comp, i.get("snapshot") or {})
    return norm_state(comp)


@handler("existing_system_prompt")
def h_existing_prompt(i):
    agent = NS(_cached_system_prompt=i.get("cached"),
               _build_system_prompt=lambda sm: i.get("build_result", ""))
    return cc._existing_system_prompt(agent, i.get("system_message", ""))


@handler("configured_compression_context_length")
def h_configured_ctx(i):
    def get_cfg(task):
        if i.get("config_error"):
            raise RuntimeError("unreadable")
        return {"context_length": kind_value(i.get("config_raw"), i.get("raw_kind", "int"))
                if i.get("config_raw") is not None else None}
    agent = NS(_aux_compression_context_length_config=kind_value(
        i.get("fallback"), i.get("fallback_kind", "int")) if i.get("fallback") is not None else None)
    with stub_modules({"agent.auxiliary_task_config": {"_get_auxiliary_task_config": get_cfg}}):
        return cc._configured_compression_context_length(agent)


@handler("context_engine_persistence")
def h_engine_persistence(i):
    with patched(cc, "accepts_keyword_argument", lambda hook, kw: bool(i.get("accepts"))):
        return cc.context_engine_persistence(object(), i.get("persistence"))


@handler("restore_messages_snapshot")
def h_restore_snapshot(i):
    messages = copy.deepcopy(i.get("messages") or [])
    before = copy.deepcopy(messages)
    snapshot = copy.deepcopy(i.get("snapshot")) if i.get("snapshot") is not None else None
    cc._restore_messages_snapshot(messages, snapshot)
    return {"restored": before != messages, "messages": messages}


@handler("pruned_skill_reload_notice")
def h_pruned_notice(i):
    extracted = list(i.get("extracted") or [])
    calls = {"n": 0}

    def extract(text):
        k = calls["n"]
        calls["n"] += 1
        return list(extracted[k]) if k < len(extracted) else []
    with patched(CX, "_extract_pruned_skill_names", extract), \
         patched(CX, "_MAX_PRUNED_SKILL_MARKERS", int(i.get("max_markers", 20))):
        return cc._pruned_skill_reload_notice(i.get("compressed") or [])


# ---------------------------------------------------------------- state machines

def _state_case(i, fn, ret_norm=lambda r: r):
    obj = mk_state_obj(i.get("state"), frozen=bool(i.get("frozen")))
    r = fn(obj)
    return {"seq": [], "state": norm_state(obj), "ret": ret_norm(r)}


@handler("compressor_attempt_serial_lock")
def h_serial_lock(i):
    obj = mk_state_obj(i.get("state"), frozen=bool(i.get("frozen")))
    r = cc._compressor_attempt_serial_lock(obj)
    kind = "lock" if isinstance(r, threading.Lock) else "nullcontext"
    return {"seq": [], "state": norm_state(obj), "ret": kind}


@handler("claim_compressor_attempt")
def h_claim(i):
    return _state_case(i, cc._claim_compressor_attempt)


@handler("compressor_attempt_is_current")
def h_is_current(i):
    obj = mk_state_obj(i.get("state"))
    r = cc._compressor_attempt_is_current(obj, int(i.get("generation", 0)))
    return {"seq": [], "state": norm_state(obj), "ret": r}


@handler("mark_compressor_working_attempt")
def h_mark_working(i):
    obj = mk_state_obj(i.get("state"), frozen=bool(i.get("frozen")))
    cc._mark_compressor_working_attempt(obj, int(i.get("generation", 0)))
    return {"seq": [], "state": norm_state(obj), "ret": None}


@handler("working_attempt_is_current")
def h_working_current(i):
    obj = mk_state_obj(i.get("state"))
    r = cc._working_attempt_is_current(obj, i.get("generation"))
    return {"seq": [], "state": norm_state(obj), "ret": r}


@handler("caller_attempt_is_current")
def h_caller_current(i):
    obj = mk_state_obj(i.get("state"))
    token = cc._COMPRESSOR_ATTEMPT_GENERATION.set(i.get("ctx_generation"))
    try:
        r = cc._caller_attempt_is_current(obj)
    finally:
        cc._COMPRESSOR_ATTEMPT_GENERATION.reset(token)
    return {"seq": [], "state": norm_state(obj), "ret": r}


@handler("raise_if_stale_attempt")
def h_raise_stale(i):
    obj = mk_state_obj(i.get("state"))
    token = cc._COMPRESSOR_ATTEMPT_GENERATION.set(i.get("ctx_generation"))
    raised = None
    try:
        cc._raise_if_stale_attempt(obj)
    except cc.AuxiliaryExplicitCancellation:
        raised = "AuxiliaryExplicitCancellation"
    finally:
        cc._COMPRESSOR_ATTEMPT_GENERATION.reset(token)
    return {"seq": [], "state": norm_state(obj), "ret": raised}


@handler("install_compression_cancelled_check")
def h_install_check(i):
    obj = mk_state_obj(i.get("state"), frozen=bool(i.get("frozen")))
    cc._install_compression_cancelled_check(obj, i.get("check"), int(i.get("generation", 0)))
    return {"seq": [], "state": norm_state(obj), "ret": None}


@handler("clear_compression_cancelled_check_if_owner")
def h_clear_check(i):
    obj = mk_state_obj(i.get("state"), frozen=bool(i.get("frozen")))
    r = cc._clear_compression_cancelled_check_if_owner(obj, int(i.get("generation", 0)))
    return {"seq": [], "state": norm_state(obj), "ret": r}


@handler("get_context_compression_timeout_state")
def h_get_timeout_state(i):
    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("no_vars")))
    ev = []
    had_lock = hasattr(agent, "_context_compression_timeout_state_lock")
    before = getattr(agent, "_context_compression_timeout_state", None)
    r = cc._get_context_compression_timeout_state(agent, create=bool(i.get("create")))
    if not had_lock and hasattr(agent, "_context_compression_timeout_state_lock"):
        rec(ev, "lock_created")
    if r is None:
        return {"seq": ev, "state": {}, "ret": None}
    lock, state = r
    if bool(i.get("create")) and not isinstance(before, threading.local):
        rec(ev, "local_created")
    return {"seq": ev, "state": norm_state(agent),
            "ret": {"lock": True, "state": jnorm(state) if state is not None else None}}


@handler("set_context_compression_timeout_outcome")
def h_set_timeout(i):
    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("no_vars")))
    ev = []
    before = getattr(agent, "_context_compression_timeout_state", None)
    cc._set_context_compression_timeout_outcome(agent, bool(i.get("timed_out")))
    if not bool(i.get("no_vars")) and not isinstance(before, threading.local):
        rec(ev, "local_created")
    return {"seq": ev, "state": flow_state(agent), "ret": None}


@handler("reset_context_compression_timeout_outcome")
def h_reset_timeout(i):
    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("no_vars")))
    ev = []
    before = getattr(agent, "_context_compression_timeout_state", None)
    cc.reset_context_compression_timeout_outcome(agent)
    if not bool(i.get("no_vars")) and not isinstance(before, threading.local):
        rec(ev, "local_created")
    return {"seq": ev, "state": flow_state(agent), "ret": None}


@handler("mark_context_compression_timed_out")
def h_mark_timeout(i):
    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("no_vars")))
    ev = []
    before = getattr(agent, "_context_compression_timeout_state", None)
    cc.mark_context_compression_timed_out(agent)
    if not bool(i.get("no_vars")) and not isinstance(before, threading.local):
        rec(ev, "local_created")
    return {"seq": ev, "state": flow_state(agent), "ret": None}


@handler("mark_compression_blocked_transient")
def h_mark_transient(i):
    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("frozen")))
    cc._mark_compression_blocked_transient(agent, NS(), i.get("reason", ""))
    return {"seq": [], "state": norm_state(agent), "ret": None}
# ---------------------------------------------------------------- small flows

@handler("swallow")
def h_swallow(i):
    ev = []
    with cc._swallow(i.get("message", ""), exc_info=bool(i.get("exc_info"))):
        if i.get("raises"):
            raise RuntimeError("scripted")
    ret_ev(ev, swallowed=bool(i.get("raises")))
    return ev


@handler("emit_feasibility_notice")
def h_feasibility_notice(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    warning_agent(agent, ev)
    cc._emit_feasibility_notice(agent, i.get("msg", ""))
    emitted = any(e["a"] == "emit_warning" for e in ev)
    ret_ev(ev, emitted=emitted, state=flow_state(agent, ("_emit_warning",)))
    return ev


@handler("warn_summary_or_aux_fallback")
def h_warn_summary(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    if isinstance(getattr(agent, "_last_aux_fallback_warning_key", None), list):
        agent._last_aux_fallback_warning_key = tuple(agent._last_aux_fallback_warning_key)
    agent.context_compressor = mk_state_obj(i.get("compressor"))
    warning_agent(agent, ev)
    cc._warn_summary_or_aux_fallback(agent)
    ret_ev(ev, state=flow_state(agent, ("context_compressor", "_emit_warning")))
    return ev


@handler("refresh_main_credential")
def h_refresh_credential(i):
    ev = []
    agent = NS(api_mode=i.get("api_mode"))
    if i.get("refresh_present"):
        def refresh():
            rec(ev, "refresh_credentials", raised=bool(i.get("refresh_raises")))
            if i.get("refresh_raises"):
                raise RuntimeError("refresh failed")
        agent._try_refresh_anthropic_client_credentials = refresh
    cc._refresh_main_credential(agent)
    ret_ev(ev)
    return ev


@handler("hand_off_metrics_segment")
def h_handoff_metrics(i):
    ev = []

    def rotate(old, new):
        rec(ev, "rotate_segment", old=old, new=new, raised=bool(i.get("raises")))
        if i.get("raises"):
            raise RuntimeError("metrics down")
    with stub_modules({"hermes_cli.observability.relay_shared_metrics": {"rotate_segment": rotate}}):
        cc._hand_off_metrics_segment(i.get("old", ""), i.get("new", ""))
    ret_ev(ev)
    return ev


@handler("reset_read_dedup_caches")
def h_reset_dedup(i):
    ev = []

    def mk(name, key, raises_key):
        def fn(v):
            rec(ev, name, **{key: v}, raised=bool(i.get(raises_key)))
            if i.get(raises_key):
                raise RuntimeError("dedup reset failed")
        return fn
    mods = {
        "tools.file_tools_read_tracking": {"reset_file_dedup": mk("reset_file_dedup", "task_id", "file_raises")},
        "tools.computer_use.tool": {"reset_screenshot_dedup": mk("reset_screenshot_dedup", "session_id", "shot_raises")},
        "tools.skills_tool": {"reset_skill_view_dedup": mk("reset_skill_view_dedup", "task_id", "skill_raises")},
    }
    with stub_modules(mods):
        cc._reset_read_dedup_caches(i.get("task_id", ""), session_id=i.get("session_id", ""))
    ret_ev(ev)
    return ev


@handler("rebind_session_context")
def h_rebind(i):
    ev = []
    sid = i.get("session_id", "")

    def set_sid(s):
        rec(ev, "set_current_session_id", session_id=s, raised=bool(i.get("set_raises")))
        if i.get("set_raises"):
            raise RuntimeError("no context")

    def set_log(s):
        rec(ev, "set_session_context", session_id=s, raised=bool(i.get("logging_raises")))
        if i.get("logging_raises"):
            raise RuntimeError("no logging")
    old_env = os.environ.get("HERMES_SESSION_ID")
    try:
        with stub_modules({"gateway.session_context": {"set_current_session_id": set_sid},
                           "hermes_logging": {"set_session_context": set_log}}):
            cc._rebind_session_context(sid)
        if i.get("set_raises"):
            # the env fallback is a state effect; record it in live order
            ev.insert(1, {"a": "env_set", "key": "HERMES_SESSION_ID", "value": sid})
    finally:
        if old_env is None:
            os.environ.pop("HERMES_SESSION_ID", None)
        else:
            os.environ["HERMES_SESSION_ID"] = old_env
    ret_ev(ev)
    return ev


@handler("aux_inherits_main_route")
def h_aux_route(i):
    ev = []
    nmap = i.get("normalize_map") or {}

    def normalize(url):
        r = nmap.get(url, "")
        rec(ev, "normalize", url=url, ret=r)
        return r
    agent = NS(model=i.get("agent_model"), base_url=i.get("agent_base_url"))
    with stub_modules({"hermes_cli.route_identity": {"normalize_route_base_url": normalize}}):
        r = cc._aux_inherits_main_route(agent, i.get("aux_model"), i.get("aux_base_url"))
    ret_ev(ev, v=r)
    return ev


@handler("compression_child_source")
def h_child_source(i):
    ev = []
    store_present = bool(i.get("store_present"))

    def read_source():
        rec(ev, "read_source", via="store" if store_present else "session_db",
            raised=bool(i.get("read_raises")))
        if i.get("read_raises"):
            raise RuntimeError("read failed")
        if store_present:
            src = i.get("record_source")
            return NS(source=src) if src is not None else None
        src = i.get("db_source")
        return {"source": src} if src is not None else None

    if store_present:
        store = NS(sessions=NS(get=lambda sid: read_source()))
    else:
        store = None
    agent = NS(platform=i.get("platform"),
               _session_db=NS(get_session=lambda sid: read_source()))

    def source_for(platform):
        r = i.get("source_for", "")
        rec(ev, "session_source_for", platform=platform, ret=r)
        return r
    with stub_modules({"agent.agent_init_session": {"agent_interface": lambda a: store},
                       "agent.session_source": {"session_source_for": source_for}}):
        r = cc._compression_child_source(agent, "parent")
    ret_ev(ev, v=r)
    return ev


@handler("held_watermark")
def h_held_watermark(i):
    ev = []
    messages = i.get("messages") or []
    tail = i.get("verbatim_tail")

    def seam(db, sid, watermark, msgs, verbatim_tail=None, **kw):
        rec(ev, "held_archive_watermark", watermark=watermark,
            n_messages=len(msgs) if isinstance(msgs, list) else 0,
            has_tail=bool(verbatim_tail), ret=i.get("seam_result"))
        return i.get("seam_result")
    agent = NS(_session_db=object(), session_id="s1")
    with patched(cc, "held_archive_watermark", seam), \
         stub_modules({"agent.agent_init_session": {"agent_interface": lambda a: None}}):
        r = cc._held_watermark(agent, i.get("watermark"), messages, tail)
    ret_ev(ev, v=r)
    return ev


# ---------------------------------------------------------------- gate / entry

@handler("automatic_compression_gate_blocks")
def h_gate_blocks(i):
    ev = []
    guards = i.get("guards") or {}
    blocked = i.get("blocked") or {}
    reason_fn = i.get("reason_fn") or {}
    bypass = bool(i.get("bypass_cooldown"))
    include_cooldown = bool(i.get("include_cooldown", True))

    attrs = {}
    order = (["get_active_compression_failure_cooldown"] if include_cooldown else []) + \
            ["_load_fallback_compression_streak", "_load_ineffective_compression_count"]
    for name in order:
        g = guards.get(name) or {}
        if not g.get("present"):
            continue

        def mk_method(nm, gg):
            def method(self, **kw):
                rec(ev, "guard_refresh", method=nm,
                    refresh_kwarg=("refresh" in kw), raised=bool(gg.get("raises")))
                if gg.get("raises"):
                    raise RuntimeError("guard refresh failed")
            return method
        attrs[name] = mk_method(name, g)

    if blocked.get("present"):
        if blocked.get("accepts_ignore"):
            def blocked_method(self, ignore_cooldown=False):
                rec(ev, "blocked_check", ignore_cooldown=bool(ignore_cooldown),
                    result=bool(blocked.get("result")))
                return bool(blocked.get("result"))
        else:
            def blocked_method(self):
                rec(ev, "blocked_check", ignore_cooldown=False,
                    result=bool(blocked.get("result")))
                return bool(blocked.get("result"))
        attrs["_automatic_compression_blocked"] = blocked_method

    Comp = type("Comp", (), attrs)
    comp = Comp()
    if reason_fn.get("present"):
        if reason_fn.get("accepts_ignore"):
            def reason(ignore_cooldown=False):
                r = None if reason_fn.get("raises") else reason_fn.get("result")
                rec(ev, "block_reason", ignore_cooldown=bool(ignore_cooldown),
                    raised=bool(reason_fn.get("raises")), ret=r)
                if reason_fn.get("raises"):
                    raise RuntimeError("reason read failed")
                return r
        else:
            def reason():
                r = None if reason_fn.get("raises") else reason_fn.get("result")
                rec(ev, "block_reason", ignore_cooldown=False,
                    raised=bool(reason_fn.get("raises")), ret=r)
                if reason_fn.get("raises"):
                    raise RuntimeError("reason read failed")
                return r
        comp._compression_block_reason = reason

    agent = mk_state_obj(i.get("agent"), frozen=bool(i.get("frozen")))
    agent.context_compressor = comp
    orig_mark = cc._mark_compression_blocked_transient

    def mark_wrapper(a, c, reason):
        marked = reason.startswith("cooldown") or reason.startswith("structural_backoff")
        rec(ev, "mark_transient", reason=reason, marked=marked)
        orig_mark(a, c, reason)
    with patched(cc, "_mark_compression_blocked_transient", mark_wrapper):
        r = cc._automatic_compression_gate_blocks(agent, bypass, include_cooldown=include_cooldown)
    ret_ev(ev, v=r, state=flow_state(agent, ("context_compressor",)))
    return ev


@handler("begin_compression_attempt")
def h_begin_attempt(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    comp = mk_state_obj(i.get("compressor"), frozen=bool(i.get("frozen")))
    agent.context_compressor = comp
    if i.get("pending_present"):
        agent._pending_context_engine_compression_notification = lambda: True

    def snapshot(c):
        rec(ev, "snapshot_attempt_state", ret=i.get("snapshot"))
        return copy.deepcopy(i.get("snapshot") or {})

    def claim(c):
        gen = int(i.get("generation", 0))
        rec(ev, "claim_attempt", ret=gen)
        if not i.get("frozen"):
            if not hasattr(c, "_compression_attempt_serial_lock"):
                c._compression_attempt_serial_lock = threading.Lock()
            c._compression_attempt_generation = gen
        return gen

    def metrics(trigger, tokens):
        rec(ev, "metrics_begin", trigger=trigger, tokens=tokens,
            raised=bool(i.get("metrics_raises")))
        if i.get("metrics_raises"):
            raise RuntimeError("metrics down")
    clock = iter([float(i.get("now", 0))])
    with patched(cc, "_snapshot_compressor_attempt_state", snapshot), \
         patched(cc, "_claim_compressor_attempt", claim), \
         patched(cc.time, "monotonic", lambda: next(clock)), \
         patched(cc.uuid, "uuid4", lambda: NS(hex=i.get("attempt_id", ""))), \
         stub_modules({"hermes_cli.observability.shared_metrics_events":
                       {"begin_compression_attempt": metrics}}):
        try:
            attempt = cc._begin_compression_attempt(
                agent, force=bool(i.get("force")),
                defer_notification=bool(i.get("defer_notification")),
                trigger=i.get("trigger"), approx_tokens=i.get("approx_tokens"),
                overflow_reason=i.get("overflow_reason"))
        except RuntimeError:
            ret_ev(ev, raised="RuntimeError",
                   state=flow_state(agent, ("context_compressor",
                                            "_pending_context_engine_compression_notification")),
                   compressor=norm_state(comp))
            return ev
    ret_ev(ev, attempt={"snapshot": jnorm(attempt.snapshot),
                        "generation": attempt.generation,
                        "started_at": attempt.started_at,
                        "seed": jnorm(attempt.seed)},
           state=flow_state(agent, ("context_compressor",
                                    "_pending_context_engine_compression_notification")),
           compressor=norm_state(comp))
    return ev


@handler("announce_compression_start")
def h_announce(i):
    ev = []
    agent = NS(session_id=i.get("session_id"), model=i.get("model"),
               context_compressor=NS())

    def emit_status(text):
        rec(ev, "emit_status", text=str(text))
    agent._emit_status = emit_status

    def auto_status(compressor, **kw):
        r = i.get("auto_status")
        rec(ev, "auto_status", phase=kw.get("phase"), ret=r)
        return r
    with patched(cc, "automatic_compaction_status_message", auto_status):
        lc = cc._announce_compression_start(
            agent, message_count=int(i.get("message_count", 0)),
            approx_tokens=i.get("approx_tokens"), focus_topic=i.get("focus_topic"),
            force=bool(i.get("force")))
    ret_ev(ev, lifecycle={"status_emitted": lc.status_emitted,
                          "commit_status": lc.commit_status,
                          "done_emitted": lc._done_emitted})
    return ev


# ---------------------------------------------------------------- lock flows

@handler("resolve_lock_api")
def h_resolve_lock_api(i):
    ev = []
    db_present = bool(i.get("db_present"))
    store_present = bool(i.get("store_present"))
    if not db_present:
        db = None
        rec(ev, "resolve_lock_api", via="none")
    elif store_present:
        db = object()
        rec(ev, "resolve_lock_api", via="store")
    else:
        rec(ev, "resolve_lock_api", via="db")
        if i.get("lookup_raises"):
            class DB:
                @property
                def try_acquire_compression_lock(self):
                    raise AttributeError("lookup failed")
            db = DB()
        elif i.get("not_callable"):
            db = NS(try_acquire_compression_lock="not-callable")
        else:
            db = NS(try_acquire_compression_lock=lambda *a, **k: True)
    fn, exc = cc._resolve_lock_api(db, object() if store_present else None)
    error = None
    if exc is not None:
        error = "not_callable" if isinstance(exc, TypeError) else "lookup"
    ret_ev(ev, api_present=fn is not None, error=error)
    return ev


@handler("release_compression_lock")
def h_release_lock(i):
    ev = []
    store_present = bool(i.get("store_present"))

    def release(*a, **kw):
        rec(ev, "release_compression_lock", via="store" if store_present else "db",
            sid=i.get("sid"), holder=i.get("holder"), raised=bool(i.get("raises")))
        if i.get("raises"):
            raise RuntimeError("release failed")
    if store_present:
        lease = NS(store=NS(coordination=NS(release_compression_lock=release)),
                   db=None, sid=i.get("sid"), holder=i.get("holder"))
    else:
        lease = NS(store=None, db=NS(release_compression_lock=release),
                   sid=i.get("sid"), holder=i.get("holder"))
    try:
        cc._release_compression_lock(lease)
    except Exception:
        pass  # callers wrap this seam in _swallow; the event is the decision
    ret_ev(ev)
    return ev


@handler("try_acquire_durable_lock")
def h_try_acquire(i):
    ev = []
    acq = i.get("acquire") or {}
    wm = i.get("watermark") or {}
    fence_present = bool(i.get("fence_present"))

    def try_acquire(sid, holder, ttl_seconds=None):
        raised = bool(acq.get("raises"))
        result = bool(acq.get("result"))
        rec(ev, "try_acquire", sid=sid, holder=holder, ttl=ttl_seconds,
            raised=raised, ret=False if raised else result)
        if raised:
            raise RuntimeError("acquire failed")
        return result

    def active_watermark(store, db, sid):
        raised = bool(wm.get("raises"))
        rec(ev, "active_watermark", raised=raised, ret=None if raised else wm.get("result"))
        if raised:
            raise RuntimeError("watermark failed")
        return wm.get("result")

    def release_stub(lease):
        rec(ev, "release_compression_lock",
            via="store" if i.get("store_present") else "db",
            sid=lease.sid, holder=lease.holder, raised=bool(i.get("release_raises")))
        if i.get("release_raises"):
            raise RuntimeError("cleanup failed")
    fence = None
    if fence_present:
        fence = NS()
        if i.get("fence_has_method", True):
            fence.mark_commit_watermark_fenced = lambda: rec(ev, "mark_watermark_fenced")
    lease = NS(sid=i.get("sid"), holder=i.get("holder"), ttl=float(i.get("ttl", 300)),
               store=object() if i.get("store_present") else None, db=object(),
               watermark=i.get("watermark_before"))
    with patched(cc, "_active_watermark", active_watermark), \
         patched(cc, "_release_compression_lock", release_stub):
        r = cc._try_acquire_durable_lock(lease, try_acquire, fence)
    ret_ev(ev, v=r, lease={"holder": lease.holder, "watermark": lease.watermark})
    return ev


def _mk_lifecycle(i, agent=None):
    lc = cc._CompactionLifecycle(agent if agent is not None else NS(),
                                 bool((i.get("lifecycle") or {}).get("status_emitted")))
    lc.commit_status = (i.get("lifecycle") or {}).get("commit_status", "aborted")
    lc._done_emitted = bool((i.get("lifecycle") or {}).get("done_emitted"))
    return lc


@handler("abort_lease")
def h_abort_lease(i):
    ev = []
    agent = NS(_cached_system_prompt=i.get("cached"),
               _build_system_prompt=lambda sm: i.get("build_result", ""))
    if i.get("status_callback_present"):
        def status_cb(kind, text):
            rec(ev, "emit_compaction_done", kind=kind, text=text,
                raised=bool(i.get("callback_raises")))
            if i.get("callback_raises"):
                raise RuntimeError("callback failed")
        agent.status_callback = status_cb
    lc = _mk_lifecycle(i, agent)
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {})
    with patched(cc, "_emit_aborted_attempt_telemetry", telemetry_stub(ev, "aborted")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        r = cc._abort_lease(agent, lc, "sys", attempt, i.get("failure_class", ""),
                            i.get("prompt"))
    ret_ev(ev, v=[r[0], r[1]],
           lifecycle={"status_emitted": lc.status_emitted,
                      "commit_status": lc.commit_status,
                      "done_emitted": lc._done_emitted})
    return ev


@handler("sit_out_lock_contention")
def h_sit_out(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    warning_agent(agent, ev, raises=bool(i.get("emit_raises")))
    store_present = bool(i.get("store_present"))

    def holder_lookup(*a, **kw):
        raised = bool(i.get("holder_raises"))
        r = None if raised else i.get("holder_result")
        rec(ev, "get_lock_holder", via="store" if store_present else "db",
            raised=raised, ret=r)
        if raised:
            raise RuntimeError("holder lookup failed")
        return r
    if store_present:
        lease = NS(store=NS(coordination=NS(get_compression_lock_holder=holder_lookup)),
                   db=None, sid=i.get("sid"), holder="h")
    else:
        lease = NS(store=None, db=NS(get_compression_lock_holder=holder_lookup),
                   sid=i.get("sid"), holder="h")
    agent._cached_system_prompt = i.get("cached")
    agent._build_system_prompt = lambda sm: i.get("build_result", "")
    lc = cc._CompactionLifecycle(agent, False)
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {})

    def abort_stub(a, lifecycle, sm, att, failure_class, prompt=None):
        rec(ev, "telemetry_aborted", started_at=att.started_at,
            failure_class=failure_class, seed=att.seed)
        return None, prompt
    with patched(cc, "_abort_lease", abort_stub), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        r = cc._sit_out_lock_contention(agent, lease, lc, "sys", None, attempt)
    ret_ev(ev, v=[r[0], r[1]],
           state=flow_state(agent, ("_emit_warning", "_cached_system_prompt",
                                    "_build_system_prompt")))
    return ev


@handler("acquire_compression_lease")
def h_acquire_lease(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    db_present = bool(i.get("db_present"))
    store_present = bool(i.get("store_present"))
    fence_present = bool(i.get("fence_present"))
    agent._session_db = object() if db_present else None
    api = i.get("lock_api") or {}
    api_present = bool(api.get("api_present"))
    api_error = api.get("error")

    def resolve_stub(db, store=None):
        rec(ev, "resolve_lock_api", api_present=api_present, error=api_error)
        if api_error:
            return None, RuntimeError("lookup")
        return (lambda *a, **k: True) if api_present else None, None

    def holder_stub(a):
        rec(ev, "mint_holder", ret=i.get("holder_id"))
        return i.get("holder_id")

    def acquire_stub(lease, try_acquire, fence):
        acq = i.get("acquire") or {}
        rec(ev, "try_acquire_durable", acquired=bool(acq.get("acquired")),
            watermark=acq.get("watermark"), holder_cleared=bool(acq.get("holder_cleared")))
        if acq.get("holder_cleared"):
            lease.holder = None
        if acq.get("acquired"):
            lease.watermark = acq.get("watermark")
        return bool(acq.get("acquired"))

    def sit_out_stub(a, lease, lc, sm, approx, att):
        rec(ev, "sit_out_lock_contention", prompt=(i.get("sit_out") or {}).get("prompt"))
        return None, (i.get("sit_out") or {}).get("prompt")

    def abort_stub(a, lc, sm, att, failure_class, prompt=None):
        rec(ev, "abort_lease", failure_class=failure_class,
            prompt=(i.get("abort") or {}).get("prompt"))
        return None, (i.get("abort") or {}).get("prompt")
    fence = None
    if fence_present:
        fence = NS()

        def begin_setup():
            r = bool(i.get("begin_lock_setup"))
            rec(ev, "begin_lock_setup", ret=r)
            return r

        def register_release(fn):
            r = bool(i.get("register_requested"))
            rec(ev, "register_cancelled_lock_release", requested=r)
            return r
        fence.begin_lock_setup = begin_setup
        fence.finish_lock_setup = lambda: rec(ev, "finish_lock_setup", entered=True)
        fence.register_cancelled_lock_release = register_release

    class RecLease(cc._CompressionLease):
        def release(self):
            rec(ev, "lease_release")
    if i.get("ttl_raw") is not None:
        agent._compression_lock_ttl_seconds = kind_value(i.get("ttl_raw"), i.get("ttl_kind", "int"))
    agent._cached_system_prompt = i.get("cached")
    agent._build_system_prompt = lambda sm: i.get("build_result", "")
    lc = cc._CompactionLifecycle(agent, False)
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {})
    with patched(cc, "_resolve_lock_api", resolve_stub), \
         patched(cc, "_compression_lock_holder", holder_stub), \
         patched(cc, "_try_acquire_durable_lock", acquire_stub), \
         patched(cc, "_sit_out_lock_contention", sit_out_stub), \
         patched(cc, "_abort_lease", abort_stub), \
         patched(cc, "_CompressionLease", RecLease), \
         patched(cc, "_emit_aborted_attempt_telemetry", telemetry_stub(ev, "aborted")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)), \
         stub_modules({"agent.agent_init_session":
                       {"agent_interface": lambda a: object() if store_present else None}}):
        lease, prompt = cc._acquire_compression_lease(
            agent, commit_fence=fence, lifecycle=lc, system_message="sys",
            approx_tokens=None, attempt=attempt)
    _acq_seams = ("_session_db", "_cached_system_prompt", "_build_system_prompt",
                  "_compression_lock_ttl_seconds")
    if lease is None:
        ret_ev(ev, v=[None, prompt], state=flow_state(agent, _acq_seams))
    else:
        ret_ev(ev, v=[{"db_present": lease.db is not None,
                       "store_present": lease.store is not None,
                       "sid": lease.sid, "ttl": lease.ttl,
                       "holder": lease.holder, "watermark": lease.watermark}, None],
               state=flow_state(agent, _acq_seams))
    return ev
# ---------------------------------------------------------------- adoption / summary

@handler("adopt_if_parent_rotated")
def h_adopt_rotated(i):
    ev = []
    db_present = bool(i.get("db_present"))
    rot = i.get("rotated") or {}

    def rotated_lookup(db, sid, store=None):
        raised = bool(rot.get("raises"))
        rec(ev, "session_rotated_lookup", raised=raised, ret=bool(rot.get("result")))
        if raised:
            raise RuntimeError("ownership lookup failed")
        return bool(rot.get("result"))

    def adopt_child(agent, db, sid, store=None):
        rec(ev, "adopt_live_child", ret=i.get("recovered"))
        return copy.deepcopy(i.get("recovered")) if i.get("recovered") is not None else None
    lease = NS(db=object() if db_present else None, sid=i.get("sid"), store=None)
    lease.release = lambda: rec(ev, "lease_release")
    agent = NS(session_id=i.get("sid"),
               _cached_system_prompt=i.get("cached"),
               _build_system_prompt=lambda sm: i.get("build_result", ""))
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {})
    messages = copy.deepcopy(i.get("messages") or [])
    with patched(cc, "_session_was_rotated_by_compression", rotated_lookup), \
         patched(cc, "_adopt_live_compression_child", adopt_child), \
         patched(cc, "_emit_bypassed_attempt_telemetry", telemetry_stub(ev, "bypassed")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        r = cc._adopt_if_parent_rotated(agent, lease, messages, "sys",
                                        attempt=attempt, approx_tokens=i.get("approx_tokens"))
    ret_ev(ev, v=jnorm(r) if r is not None else None)
    return ev


@handler("adopt_grown_durable_parent")
def h_adopt_grown(i):
    ev = []
    db_present = bool(i.get("db_present"))
    store_present = bool(i.get("store_present"))
    reads = {"d1": copy.deepcopy(i.get("durable1")), "d2": copy.deepcopy(i.get("durable2"))}

    def loader(db, sid, include_row_ids=False):
        r = reads["d2"] if include_row_ids else reads["d1"]
        rec(ev, "durable_load", include_row_ids=bool(include_row_ids), ret=copy.deepcopy(r))
        return r
    agent = NS(_persist_user_message_idx=i.get("preflush_idx"))

    def flush(messages, conversation_history=None):
        fl = i.get("flush") or {}
        raised = bool(fl.get("raises"))
        ok = bool(fl.get("result"))
        rec(ev, "preflush", idx=i.get("preflush_idx"), raised=raised, ret=ok)
        if raised:
            raise RuntimeError("flush failed")
        return ok
    agent._flush_messages_to_session_db = flush
    lease = NS(db=object() if db_present else None, sid=i.get("sid"),
               store=object() if store_present else None, watermark=i.get("watermark"))
    if not store_present:
        if i.get("loader_present"):
            class DB:
                get_messages_as_conversation = staticmethod(loader)
            lease.db = DB()
        else:
            class DB:
                pass
            lease.db = DB()
    messages = copy.deepcopy(i.get("messages") or [])
    with patched(cc, "_read_conversation", lambda store, sid, include_row_ids=False:
                 loader(None, sid, include_row_ids)):
        r = cc._adopt_grown_durable_parent(agent, lease, messages)
    ret_ev(ev, v=jnorm(r) if r is not None else None, watermark=lease.watermark)
    return ev


@handler("pre_compress_memory_context")
def h_memory_context(i):
    ev = []
    mm = None
    if i.get("mm_present"):
        mm = NS()
        if i.get("supports_present"):
            def supports(version):
                sup = i.get("supports") or {}
                raised = bool(sup.get("raises"))
                rec(ev, "supports_checkpoint", version=version, raised=raised,
                    ret=bool(sup.get("result")))
                if raised:
                    raise RuntimeError("probe failed")
                return bool(sup.get("result"))
            mm.supports_pre_compress_checkpoint = supports

        def on_pre_compress(messages, evidence_messages=None, require_checkpoint=False,
                            checkpoint_api_version=None):
            opc = i.get("on_pre_compress") or {}
            raised = bool(opc.get("raises"))
            rec(ev, "on_pre_compress", require_checkpoint=bool(require_checkpoint),
                raised=raised, ret=None if raised else opc.get("result"))
            if raised:
                raise RuntimeError("checkpoint failed")
            return opc.get("result")
        mm.on_pre_compress = on_pre_compress
    agent = NS(_memory_manager=mm, _persist_disabled=bool(i.get("persist_disabled")),
               _session_db=object() if i.get("db_present") else None)

    def sanitize(ctx):
        r = (i.get("sanitize") or {}).get("result", "")
        rec(ev, "sanitize_memory_context", ret=r)
        return r
    raised = None
    message = None
    with patched(cc, "sanitize_memory_context", sanitize):
        try:
            r = cc._pre_compress_memory_context(agent, i.get("messages") or [],
                                                bool(i.get("checkpoint_required")))
        except Exception as exc:
            raised = type(exc).__name__
            message = str(exc)
            rec(ev, "checkpoint_refused", raised=raised, message=message)
            r = ""
    if raised:
        ret_ev(ev, memory_context="", raised=raised, message=message)
    else:
        ret_ev(ev, memory_context=r, raised=None)
    return ev


@handler("resolve_compress_call")
def h_resolve_call(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    comp = NS(name=i.get("engine_name"), compress=lambda *a, **k: [])
    agent.context_compressor = comp

    def supported(fn, **kw):
        rec(ev, "supported_kwargs", ret=i.get("kwargs"))
        return copy.deepcopy(i.get("kwargs") or {})
    with patched(cc, "_supported_compression_kwargs", supported):
        fn, kwargs = cc._resolve_compress_call(
            agent, approx_tokens=i.get("approx_tokens"), focus_topic=i.get("focus_topic"),
            force=bool(i.get("force")), memory_context=i.get("memory_context", ""),
            bypass_cooldown=bool(i.get("bypass_cooldown")))
    ret_ev(ev, kwargs=jnorm(kwargs), state=flow_state(agent, ("context_compressor",)))
    return ev


@handler("run_summary_dispatch")
def h_dispatch(i):
    ev = []
    comp = mk_state_obj(i.get("compressor"), frozen=bool(i.get("frozen")))
    agent = NS(context_compressor=comp, session_id="s1")
    fence_present = bool(i.get("fence_present"))
    fence_cancelled = bool(i.get("fence_cancelled"))
    hard_set = bool(i.get("hard_cancel_set"))
    fence = None
    skip_seen = {"v": False}
    if fence_present:
        class Fence:
            touch_progress = staticmethod(lambda: None)

            @property
            def is_cancelled(self):
                if fence_cancelled and not skip_seen["v"]:
                    skip_seen["v"] = True
                    ev.append({"a": "skip_cancelled"})
                return fence_cancelled
        fence = Fence()
    hard = NS(is_set=lambda: hard_set) if i.get("hard_cancel_set") is not None else None
    if hard is None and hard_set:
        hard = NS(is_set=lambda: True)
    messages = copy.deepcopy(i.get("messages") or [])
    cf = i.get("compress") or {}

    def compress_fn(msgs, **kwargs):
        raised = cf.get("raises")
        rec(ev, "compress_fn", kwargs=jnorm(kwargs), raised=raised,
            ret=None if raised else cf.get("result"))
        if raised == "AuxiliaryExplicitCancellation":
            raise cc.AuxiliaryExplicitCancellation()
        if raised:
            raise type(raised, (Exception,), {})("scripted")
        if hard_set:
            rec(ev, "hard_cancel_raise")
            raise cc.AuxiliaryExplicitCancellation()
        return copy.deepcopy(cf.get("result"))
    orig_install = cc._install_compression_cancelled_check
    orig_clear = cc._clear_compression_cancelled_check_if_owner
    orig_mark = cc._mark_compressor_working_attempt

    def install(c, check, gen):
        rec(ev, "install_cancelled_check", generation=gen)
        orig_install(c, check, gen)

    def clear(c, gen):
        r = orig_clear(c, gen)
        rec(ev, "clear_cancelled_check", generation=gen, ret=r)
        return r

    def mark(c, gen):
        rec(ev, "mark_working", generation=gen)
        orig_mark(c, gen)
    raised = None
    with patched(cc, "_install_compression_cancelled_check", install), \
         patched(cc, "_clear_compression_cancelled_check_if_owner", clear), \
         patched(cc, "_mark_compressor_working_attempt", mark), \
         patched(AX, "aux_progress_hook", lambda hook: contextlib.nullcontext()), \
         patched(AX, "aux_interrupt_protection", lambda cancel_check=None: contextlib.nullcontext()), \
         stub_modules({"agent.conversation_compression_deadline":
                       {"compression_deadline_scope": lambda fence: contextlib.nullcontext()}}):
        if fence_present and fence_cancelled:
            pass  # skip event is recorded by the fence.is_cancelled read
        try:
            compressed = cc._run_summary_dispatch(
                agent, messages, compress_fn, i.get("kwargs") or {},
                commit_fence=fence, attempt_generation=i.get("generation"),
                hard_cancel_event=hard)
        except cc.AuxiliaryExplicitCancellation:
            raised = "AuxiliaryExplicitCancellation"
            compressed = None
        except Exception as exc:
            raised = type(exc).__name__
            compressed = None
    ret_ev(ev, compressed=jnorm(compressed), raised=raised, state=norm_state(comp))
    return ev


@handler("run_summary_phase")
def h_summary_phase(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    comp = mk_state_obj(i.get("compressor"), frozen=bool(i.get("frozen")))
    agent.context_compressor = comp
    messages = copy.deepcopy(i.get("messages") or [])
    fence_present = bool(i.get("fence_present"))
    fence = None
    if fence_present:
        class Fence:
            @property
            def is_cancelled(self):
                return bool(i.get("fence_cancelled"))
        fence = Fence()
    hard = NS(is_set=lambda: bool(i.get("hard_cancel_set")))
    lease = NS(status_emitted=bool(i.get("status_emitted")))
    lease.start_refresher = lambda: rec(ev, "start_refresher")
    lease.release = lambda: rec(ev, "lease_release")

    def adopt_stub(a, l, msgs):
        r = (i.get("adopt") or {}).get("result")
        rec(ev, "adopt_grown_parent", ret=r)
        return copy.deepcopy(r) if r is not None else None

    def memory_stub(a, msgs, checkpoint_required):
        mem = i.get("memory") or {}
        rec(ev, "pre_compress_memory", ret=mem.get("memory_context", ""),
            raised=mem.get("raised"))
        if mem.get("raised"):
            raise _named_exc(mem["raised"])
        return mem.get("memory_context", "")

    def _named_exc(name):
        if name == "CompressionCheckpointUnavailable":
            from agent.conversation_compression_checkpoint import CompressionCheckpointUnavailable
            return CompressionCheckpointUnavailable("scripted checkpoint refusal")
        return type(name, (Exception,), {})("scripted")

    def resolve_stub(a, **kw):
        rec(ev, "resolve_compress_call", kwargs=(i.get("resolve") or {}).get("kwargs"))
        return (lambda *a, **k: []), copy.deepcopy((i.get("resolve") or {}).get("kwargs") or {})

    class FakeHeartbeat:
        def __init__(self, a, commit_fence=None, emit_client_status=False):
            rec(ev, "heartbeat_start", emit_client_status=bool(emit_client_status))

        def start(self):
            return self

        def stop(self, desc="context compression completed"):
            rec(ev, "heartbeat_stop", desc=desc)

    def dispatch_stub(a, msgs, fn, kwargs, commit_fence=None, attempt_generation=None,
                      hard_cancel_event=None):
        disp = i.get("dispatch") or {}
        rec(ev, "telemetry_seed_reset", seed=i.get("seed"))
        if not i.get("frozen"):
            comp._last_compression_telemetry = None
            comp._active_compression_telemetry = None
            comp._compression_telemetry_seed = dict(i.get("seed") or {})
        rec(ev, "summary_dispatch", raised=disp.get("raises"))
        if disp.get("raises") == "AuxiliaryExplicitCancellation":
            raise cc.AuxiliaryExplicitCancellation()
        if disp.get("raises"):
            raise type(disp["raises"], (Exception,), {})("scripted")
        return copy.deepcopy(disp.get("compressed"))

    def restore_stub(c):
        rs = i.get("restore") or {}
        rec(ev, "restore_compressor", raised=rs.get("raises"))
        if rs.get("raises"):
            raise RuntimeError(rs["raises"])
    attempt = NS(snapshot=i.get("snapshot") or {}, generation=int(i.get("generation", 1)),
                 started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {},
                 durable_cooldown_authoritative=None, durable_cooldown_state=None)
    attempt.restore_compressor = restore_stub

    def stall_stub(a, commit_fence=None, started_at=None, messages=None, approx_tokens=None):
        r = bool(i.get("stall_backoff"))
        rec(ev, "record_stall_backoff", ret=r)
        return r
    orig_restore_msgs = cc._restore_messages_snapshot

    def restore_msgs_wrapper(msgs, snapshot):
        before = copy.deepcopy(msgs)
        orig_restore_msgs(msgs, snapshot)
        rec(ev, "restore_messages", restored=before != msgs)
    agent._cached_system_prompt = i.get("cached")
    agent._build_system_prompt = lambda sm: i.get("build_result", "")
    raised = None
    with patched(cc, "_adopt_grown_durable_parent", adopt_stub), \
         patched(cc, "_pre_compress_memory_context", memory_stub), \
         patched(cc, "_resolve_compress_call", resolve_stub), \
         patched(cc, "_CompressionActivityHeartbeat", FakeHeartbeat), \
         patched(cc, "_run_summary_dispatch", dispatch_stub), \
         patched(cc, "_record_stall_interrupted_backoff", stall_stub), \
         patched(cc, "_restore_messages_snapshot", restore_msgs_wrapper), \
         patched(cc, "_emit_aborted_attempt_telemetry", telemetry_stub(ev, "aborted")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        try:
            phase = cc._run_summary_phase(
                agent, messages, lease=lease, in_place=bool(i.get("in_place")),
                checkpoint_required=bool(i.get("checkpoint_required")),
                approx_tokens=i.get("approx_tokens"), focus_topic=None,
                force=bool(i.get("force")), bypass_cooldown=bool(i.get("bypass_cooldown")),
                commit_fence=fence, hard_cancel_event=hard, system_message="sys",
                attempt=attempt)
        except cc.AuxiliaryExplicitCancellation:
            raised = "AuxiliaryExplicitCancellation"
            phase = None
        except BaseException as exc:
            raised = type(exc).__name__
            phase = None
    _phase_seams = ("context_compressor", "_cached_system_prompt", "_build_system_prompt")
    if raised:
        ret_ev(ev, raised=raised, state=flow_state(agent, _phase_seams),
               compressor=norm_state(comp))
    elif phase.abort_prompt is not None:
        ret_ev(ev, phase={"messages": jnorm(phase.messages), "abort_prompt": phase.abort_prompt},
               state=flow_state(agent, _phase_seams), compressor=norm_state(comp))
    else:
        ret_ev(ev, phase={"messages": jnorm(phase.messages),
                          "compressed": jnorm(phase.compressed),
                          "messages_before_compression": jnorm(phase.messages_before_compression),
                          "approx_tokens": phase.approx_tokens,
                          "pre_msg_count": phase.pre_msg_count},
               state=flow_state(agent, _phase_seams), compressor=norm_state(comp))
    return ev


@handler("candidate_rejected")
def h_candidate_rejected(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    sid_injected = not hasattr(agent, "session_id")
    if sid_injected:
        agent.session_id = "s1"
    comp = mk_state_obj(i.get("compressor"))
    agent.context_compressor = comp
    warning_agent(agent, ev, raises=bool(i.get("emit_raises")))
    sr = i.get("structural_recorder") or {}
    if sr.get("present"):
        def recorder(reason):
            rec(ev, "record_structural_no_op", raised=bool(sr.get("raises")))
            if sr.get("raises"):
                raise RuntimeError("recorder failed")
        comp._record_structural_no_op = recorder
    attempt = NS(started_at=float(i.get("started_at", 0)), generation=int(i.get("generation", 0)),
                 seed=i.get("seed") or {})
    compressed = copy.deepcopy(i.get("compressed"))
    before = copy.deepcopy(i.get("before"))
    with patched(cc, "_emit_aborted_attempt_telemetry", telemetry_stub(ev, "aborted")):
        r = cc._candidate_rejected(agent, compressed, before, before, attempt=attempt)
    seams = ("context_compressor", "_emit_warning") + (("session_id",) if sid_injected else ())
    ret_ev(ev, v=r, state=flow_state(agent, seams))
    return ev


@handler("salvage_or_refuse_grown_transcript")
def h_salvage(i):
    ev = []
    agent = NS(session_id="s1")
    comp = mk_state_obj(i.get("compressor"), frozen=bool(i.get("frozen")))
    agent.context_compressor = comp
    warning_agent(agent, ev, raises=bool(i.get("emit_raises")))
    agent._cached_system_prompt = i.get("cached")
    agent._build_system_prompt = lambda sm: i.get("build_result", "")
    rr = i.get("record_rejected") or {}
    if rr.get("present"):
        def record_rejected():
            rec(ev, "record_rejected_compaction", raised=bool(rr.get("raises")))
            if rr.get("raises"):
                raise RuntimeError("strike failed")
        comp.record_rejected_compaction = record_rejected
    estimates = [float(i.get("rough_in", 0)), float(i.get("rough_out", 0))]
    if i.get("salvage", {}).get("result") is not None:
        estimates.append(float(i.get("salv_est", 0)))
    calls = {"n": 0}

    def estimate(msgs):
        k = calls["n"]
        calls["n"] += 1
        which = ["in", "out", "salvaged"][k] if k < 3 else "salvaged"
        r = estimates[k] if k < len(estimates) else estimates[-1]
        rec(ev, "estimate_rough", which=which, ret=r)
        return r

    def salvage_stub(original, compressed, budget=None):
        r = (i.get("salvage") or {}).get("result")
        rec(ev, "salvage_grown", budget=budget, ret=r)
        return copy.deepcopy(r) if r is not None else None
    orig_prune = cc._restore_prune_rearm_tokens

    def prune_wrapper(c, snapshot):
        rec(ev, "restore_prune_rearm")
        orig_prune(c, snapshot)
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {},
                 snapshot=i.get("snapshot") or {})
    compressed = copy.deepcopy(i.get("compressed"))
    with patched(cc, "estimate_messages_tokens_rough", estimate), \
         patched(CX, "salvage_grown_transcript", salvage_stub), \
         patched(cc, "_restore_prune_rearm_tokens", prune_wrapper), \
         patched(cc, "_emit_aborted_attempt_telemetry", telemetry_stub(ev, "aborted")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        r = cc._salvage_or_refuse_grown_transcript(agent, compressed, compressed,
                                                   system_message="sys", attempt=attempt)
    ret_ev(ev, v=jnorm(list(r)),
           compressor=flow_state(comp, ("record_rejected_compaction",)))
    return ev


@handler("fold_todo_snapshot")
def h_fold_todo(i):
    ev = []
    phase = {"name": "scan"}
    merged = {"done": False}
    snapshot_text = {"v": i.get("todo_snapshot", "")}

    class FoldList(list):
        def pop(self, idx=-1):
            rec(ev, "todo_strip", idx=idx, action="pop")
            return super().pop(idx)

        def append(self, item):
            if phase["name"] == "fold" and isinstance(item, dict) \
                    and item.get("_todo_snapshot_synthetic"):
                rec(ev, "todo_append")
            return super().append(item)
    compressed = FoldList(copy.deepcopy(i.get("compressed") or []))

    def fmt():
        r = i.get("todo_snapshot", "")
        rec(ev, "todo_format", ret=r)
        return r
    store = NS(format_for_injection=fmt)
    hi = i.get("has_items") or {}
    if hi.get("present"):
        def has_items():
            raised = bool(hi.get("raises"))
            r = bool(hi.get("result")) and not raised
            rec(ev, "todo_has_items", raised=raised, ret=r)
            if raised:
                raise RuntimeError("has_items failed")
            return r
        store.has_items = has_items
    agent = NS(_todo_store=store)
    agent._repair_message_sequence = lambda msgs: rec(ev, "repair_message_sequence")

    def notice_stub(comp):
        r = i.get("reload_notice", "")
        rec(ev, "pruned_skill_notice", ret=r)
        phase["name"] = "fold"
        if r:
            snapshot_text["v"] = i.get("todo_snapshot", "") + "\n\n" + r
        return r

    def real_probe(probe):
        r = bool(i.get("tail_is_real"))
        rec(ev, "is_real_user_probe", ret=r)
        return r
    orig_replace = cc._replace_message_content

    def replace_wrapper(msg, content):
        if phase["name"] == "scan":
            idx = next((k for k, m in enumerate(compressed) if m is msg), None)
            rec(ev, "todo_strip", idx=idx, action="replace")
        elif phase["name"] == "fold" and not merged["done"]:
            if content == snapshot_text["v"]:
                rec(ev, "todo_merge_refresh")
                merged["done"] = True
        orig_replace(msg, content)

    orig_append = CX._append_text_to_content

    def append_text_stub(content, text, *, prepend=False):
        rec(ev, "todo_merge_append")
        merged["done"] = True
        return orig_append(content, text, prepend=prepend)
    with patched(cc, "_pruned_skill_reload_notice", notice_stub), \
         patched(cc, "_is_real_user_message", real_probe), \
         patched(cc, "_replace_message_content", replace_wrapper), \
         patched(CX, "_append_text_to_content", append_text_stub), \
         stub_modules({"tools.todo_tool": {"TODO_INJECTION_HEADER":
                       "[Your active task list was preserved across context compression]"},
                       "session_rules.turns": {"drop_stale_api_content":
                       lambda m: m.pop("api_content", None)}}):
        cc._fold_todo_snapshot(agent, compressed)
    ret_ev(ev, compressed=jnorm(list(compressed)))
    return ev


@handler("rebuild_system_prompt_at_boundary")
def h_rebuild_prompt(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    if not hasattr(agent, "_cached_system_prompt"):
        agent._cached_system_prompt = None
    sid_injected = not hasattr(agent, "session_id")
    if sid_injected:
        agent.session_id = "s1"
    agent._invalidate_system_prompt = lambda: rec(ev, "invalidate_system_prompt")

    def build(sm):
        r = i.get("build_result", "")
        rec(ev, "build_system_prompt", ret=r)
        return r
    agent._build_system_prompt = build

    def refresh(a):
        rec(ev, "refresh_tool_definitions", raised=bool(i.get("refresh_raises")))
        if i.get("refresh_raises"):
            raise RuntimeError("refresh failed")
        return False

    def reconstruct(a, system_message=None, log_label=None):
        rec(ev, "reconstruct_static_prefix", log_label=log_label)
    with patched(cc, "_refresh_agent_tool_definitions", refresh), \
         stub_modules({"agent.system_prompt": {"reconstruct_static_prefix": reconstruct}}):
        r = cc._rebuild_system_prompt_at_boundary(agent, "sys")
    seams = ("_invalidate_system_prompt", "_build_system_prompt") +         (("session_id",) if sid_injected else ())
    ret_ev(ev, v=r, state=flow_state(agent, seams))
    return ev
# ---------------------------------------------------------------- commit / boundary

@handler("commit_compaction")
def h_commit(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    comp = mk_state_obj(i.get("compressor"))
    agent.context_compressor = comp
    db_present = bool(i.get("session_db_present"))
    in_place = bool(i.get("in_place"))
    messages = copy.deepcopy(i.get("messages") or [])
    compressed = copy.deepcopy(i.get("compressed") or [])
    before = copy.deepcopy(i.get("messages_before")) if i.get("messages_before") is not None else None
    tail = copy.deepcopy(i.get("verbatim_tail")) if i.get("verbatim_tail") is not None else None
    agent._session_db = object() if db_present else None
    store_present = bool(i.get("store_present", True))
    roles = i.get("absorbed_roles") or {}

    def role_of(sid, rid):
        return roles.get(str(rid))
    def update_prompt(sid, p):
        rec(ev, "update_system_prompt", sid=sid, prompt=p,
            raised=bool(i.get("update_prompt_raises")))
        if i.get("update_prompt_raises"):
            raise RuntimeError(i.get("error_msg", "prompt update failed"))
    if store_present:
        store = NS(messages=NS(message_role=role_of),
                   sessions=NS(update_system_prompt=update_prompt))
    else:
        store = None
        agent._session_db = NS(get_message_role=role_of,
                               update_system_prompt=update_prompt)

    def salvage_stub(a, original, comp_msgs, system_message=None, attempt=None):
        s = i.get("salvage") or {}
        rec(ev, "salvage_gate", refused=bool(s.get("refused")), prompt=s.get("prompt"))
        if s.get("refused"):
            return None, s.get("prompt")
        return comp_msgs, None

    def memory_call(original):
        mc = i.get("memory_call") or {}
        resets = mc.get("resets")
        if resets:
            for k, v in resets.items():
                setattr(comp, k, copy.deepcopy(v))
        rec(ev, "commit_memory_session", raised=bool(mc.get("raises")))
        if mc.get("raises"):
            raise RuntimeError(i.get("error_msg", "memory failed"))
    agent.commit_memory_session = memory_call

    def rejoin(head, t):
        r = (i.get("rejoin") or {}).get("result")
        rec(ev, "rejoin_head_tail", ret=r)
        return copy.deepcopy(r)

    def coverage(db, sid, original, verbatim_tail=None, persistence=None):
        cov = i.get("coverage") or {}
        rec(ev, "coverage_for_commit", covered_ids=cov.get("covered_ids"),
            unresolved_held=cov.get("unresolved_held"))
        return cov.get("covered_ids") or [], cov.get("unresolved_held") or []

    def archive_commit(db, store_, sid, persisted, **kw):
        ac = i.get("archive_commit") or {}
        rec(ev, "commit_compaction", watermark=kw.get("watermark"),
            lock_holder=kw.get("lock_holder"), tail_count=kw.get("tail_count"),
            raised=bool(ac.get("raises")))
        if ac.get("raises"):
            raise RuntimeError(i.get("error_msg", "archive failed"))

    def held_wm(a, watermark, msgs, verbatim_tail=None):
        r = (i.get("held_watermark") or {}).get("result")
        rec(ev, "held_archive_watermark", ret=r)
        return r

    def publish_stub(a, msgs, comp_msgs, new_system_prompt=None, lease=None,
                     old_session_id=None, compressed_user_turn_outcome=None):
        pub = i.get("publish") or {}
        rec(ev, "publish_rotated", old_session_id=old_session_id, raised=bool(pub.get("raises")))
        if pub.get("raises"):
            raise RuntimeError(i.get("error_msg", "publish failed"))
        a.session_id = pub.get("new_session_id")
    cd = i.get("split_cooldown") or {}
    if cd.get("present"):
        def record_cooldown(seconds, reason):
            rec(ev, "record_split_cooldown", seconds=seconds, reason=reason,
                raised=bool(cd.get("raises")))
            if cd.get("raises"):
                raise RuntimeError("cooldown persist failed")
        comp._record_compression_failure_cooldown = record_cooldown
    orig_prune = cc._restore_prune_rearm_tokens

    def prune_wrapper(c, snapshot):
        rec(ev, "restore_prune_rearm")
        orig_prune(c, snapshot)
    lease = NS(holder=(i.get("lease") or {}).get("holder"),
               watermark=(i.get("lease") or {}).get("watermark"))
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {},
                 snapshot=i.get("snapshot") or {})
    clock = iter([float(i.get("now", 0))])
    archive_mod = {"coverage_for_commit": coverage, "commit_compaction": archive_commit,
                   "ABSORBED_ROW_IDS": "_absorbed_row_ids",
                   "_positive_id": lambda v: v if isinstance(v, int) and not isinstance(v, bool) and v > 0 else None}
    with patched(cc, "_salvage_or_refuse_grown_transcript", salvage_stub), \
         patched(cc, "_held_watermark", held_wm), \
         patched(cc, "_publish_rotated_compaction", publish_stub), \
         patched(cc, "_restore_prune_rearm_tokens", prune_wrapper), \
         patched(cc.time, "monotonic", lambda: next(clock)), \
         stub_modules({"agent.agent_init_session": {"agent_interface": lambda a: store},
                       "hermes_cli.partial_compress": {"rejoin_compressed_head_and_tail": rejoin},
                       "agent.conversation_compression_archive": archive_mod}):
        outcome = cc._commit_compaction(
            agent, messages, compressed, in_place=in_place, lease=lease,
            new_system_prompt=i.get("new_system_prompt", ""), system_message="sys",
            compressed_user_turn_outcome=i.get("compressed_user_turn_outcome", "already_present"),
            messages_before_compression=before, made_progress=bool(i.get("made_progress")),
            attempt=attempt, verbatim_tail=tail, carried_messages=i.get("carried_messages"))
    ret_ev(ev, compressed=jnorm(outcome.compressed),
           commit_started_at=outcome.commit_started_at,
           refused_prompt=outcome.refused_prompt, old_session_id=outcome.old_session_id,
           split_status=outcome.split_status,
           session_commit_succeeded=outcome.session_commit_succeeded,
           compacted_in_place=outcome.compacted_in_place, made_progress=outcome.made_progress,
           state=flow_state(agent, ("context_compressor", "_session_db",
                                    "commit_memory_session")),
           compressor=flow_state(comp, ("_record_compression_failure_cooldown",)))
    return ev


@handler("finish_compaction_boundary")
def h_finish_boundary(i):
    ev = []
    agent = mk_state_obj(i.get("agent"))
    tools_injected = not hasattr(agent, "tools")
    if tools_injected:
        agent.tools = None

    class CompD:
        pass
    comp = CompD()
    for k, v in (i.get("compressor") or {}).items():
        setattr(comp, k, copy.deepcopy(v))
    agent.context_compressor = comp
    warning_agent(agent, ev)
    old_sid = i.get("old_session_id")
    succeeded = bool(i.get("session_commit_succeeded"))
    cl = i.get("clear_labels") or {}
    via = cl.get("via", "none")
    store = None
    if via == "store":
        store = NS(sessions=NS(clear_activity_labels=lambda sid: rec(ev, "clear_activity_labels",
                                                                       via="store", sid=sid,
                                                                       raised=bool(cl.get("raises")))))
    elif via == "db":
        class DB:
            def clear_session_activity_labels(self, sid):
                rec(ev, "clear_activity_labels", via="db", sid=sid, raised=bool(cl.get("raises")))
        agent._session_db = DB()

    def notify_direct(a, new_session_id=None, old_session_id=None):
        rec(ev, "context_engine_notify", mode="direct", new_session_id=new_session_id,
            old_session_id=old_session_id, raised=bool(i.get("notify_raises")))
        return True

    def notify_queue(a, new_session_id=None, old_session_id=None):
        rec(ev, "context_engine_notify", mode="queue", new_session_id=new_session_id,
            old_session_id=old_session_id, raised=bool(i.get("notify_raises")))
    if i.get("mm_present"):
        agent._memory_manager = NS(on_session_switch=lambda sid, parent_session_id=None,
                                   reset=None, reason=None:
                                   rec(ev, "memory_session_switch", new_session_id=sid,
                                       parent_session_id=parent_session_id,
                                       raised=bool(i.get("memory_switch_raises"))))
    else:
        agent._memory_manager = None
    if i.get("event_callback_present"):
        def event_cb(event, payload):
            rec(ev, "event_callback", event=event, payload=jnorm(payload),
                raised=bool(i.get("event_callback_raises")))
        agent.event_callback = event_cb

    def estimate(msgs, system_prompt=None, tools=None):
        r = float(i.get("estimate_request", 0))
        rec(ev, "estimate_request_rough", ret=r)
        return r

    def anchor(a, value):
        rec(ev, "set_usage_anchor")
    if i.get("record_boundary_present"):
        def record_completed_compaction(self, used_fallback=None, feasibility_skip=None):
            rec(ev, "record_completed_compaction", used_fallback=bool(used_fallback),
                feasibility_skip=bool(feasibility_skip))
        CompD.record_completed_compaction = record_completed_compaction

    def dedup(task_id, session_id=""):
        rec(ev, "reset_read_dedup", task_id=task_id, session_id=session_id)
    with patched(cc, "_notify_context_engine_compression_complete", notify_direct), \
         patched(cc, "_queue_context_engine_compression_notification", notify_queue), \
         patched(cc, "estimate_request_tokens_rough", estimate), \
         patched(cc, "set_usage_anchor", anchor), \
         patched(cc, "_reset_read_dedup_caches", dedup), \
         stub_modules({"agent.agent_init_session": {"agent_interface": lambda a: store}}):
        r = cc._finish_compaction_boundary(
            agent, i.get("compressed") or [],
            new_system_prompt=i.get("new_system_prompt", ""), old_session_id=old_sid,
            in_place=bool(i.get("in_place")), compacted_in_place=bool(i.get("compacted_in_place")),
            session_commit_succeeded=succeeded,
            defer_context_engine_notification=bool(i.get("defer_notification")),
            compression_made_progress=bool(i.get("compression_made_progress")),
            compression_used_fallback=bool(i.get("compression_used_fallback")),
            compression_feasibility_skip=bool(i.get("compression_feasibility_skip")),
            task_id=i.get("task_id", ""))
    seams = ("context_compressor", "_emit_warning", "_memory_manager",
             "event_callback", "_session_db") + (("tools",) if tools_injected else ())
    ret_ev(ev, v=r, state=flow_state(agent, seams), compressor=norm_state(comp))
    return ev


@handler("publish_rotated_compaction")
def h_publish(i):
    ev = []
    _pub_seams = ("_session_db", "_session_init_model_config", "model",
                  "_flush_messages_to_session_db", "_session_messages")
    agent = mk_state_obj(i.get("agent"))
    messages = copy.deepcopy(i.get("messages") or [])
    compressed = copy.deepcopy(i.get("compressed") or [])
    store_present = bool(i.get("store_present"))
    agent._session_db = NS()
    agent._session_init_model_config = {}
    agent.model = "m"
    flags = list(i.get("real_user_flags") or [])
    flag_map = {id(m): (flags[k] if k < len(flags) else False) for k, m in enumerate(messages)}

    def real_user(m):
        return flag_map.get(id(m), False)

    def parent_ended(db, sid, store=None):
        r = bool(i.get("parent_ended"))
        rec(ev, "parent_deliberately_ended", ret=r)
        return r

    def watermark(store, db, sid):
        fc = i.get("foreign_ceiling") or {}
        raised = bool(fc.get("raises"))
        rec(ev, "active_watermark", raised=raised, ret=None if raised else fc.get("result"))
        if raised:
            raise RuntimeError("watermark failed")
        return fc.get("result")

    def flush(msgs, conversation_history=None):
        rec(ev, "flush_messages",
            history_len=len(conversation_history) if conversation_history is not None else None,
            raised=bool((i.get("flush") or {}).get("raises")))
        if (i.get("flush") or {}).get("raises"):
            raise RuntimeError("flush failed")

    def profile():
        pr = i.get("profile") or {}
        rec(ev, "active_profile", raised=bool(pr.get("raises")), ret=pr.get("result"))
        if pr.get("raises"):
            raise RuntimeError("profile failed")
        return pr.get("result")

    def get_title(sid):
        t = i.get("title")
        rec(ev, "get_title", ret=t if t else None)
        return t

    def mint():
        rec(ev, "mint_session_id", ret=i.get("mint"))
        return i.get("mint")

    def child_source(a, parent):
        rec(ev, "compression_child_source", ret=i.get("child_source"))
        return i.get("child_source")

    def yolo(cfg, sid):
        rec(ev, "with_session_yolo", ret=i.get("yolo"))
        return i.get("yolo")

    def publish_child(*a, **kw):
        pub = i.get("publish") or {}
        terms = {k: kw.get(k) for k in ("compression_lock_holder", "require_compression_lease",
                                        "require_lease_refresh", "lease_ttl_seconds",
                                        "watermark", "watermark_ceiling")}
        rec(ev, "publish_compression_child", via="store" if store_present else "db",
            new_session_id=kw.get("child_session_id") or (a[1].session_id if len(a) > 1 and hasattr(a[1], "session_id") else None),
            lease_terms=terms, raised=bool(pub.get("raises")))
        if pub.get("raises"):
            raise type(pub.get("raises_kind", "RuntimeError"), (Exception,), {})("publish failed")
    if store_present:
        store = NS(sessions=NS(get_title=get_title),
                   lineage=NS(publish_compression_child=publish_child))
        agent._session_db.publish_compression_child = None
    else:
        store = None
        agent._session_db.get_session_title = get_title
        agent._session_db.publish_compression_child = publish_child
    agent._flush_messages_to_session_db = flush
    if i.get("session_messages_separate"):
        agent._session_messages = copy.deepcopy(i.get("session_messages") or [])
    else:
        agent._session_messages = messages

    def rebind(sid):
        rec(ev, "rebind_session_context", session_id=sid)

    def carry(a, old_sid, old_title):
        rec(ev, "carry_session_state", old_session_id=old_sid, title=old_title if old_title else None)
    lease = NS(holder=(i.get("lease") or {}).get("holder"),
               watermark=(i.get("lease") or {}).get("watermark"),
               ttl=float((i.get("lease") or {}).get("ttl", 300)))
    lineage_mod = {"CompressionChild": lambda **kw: NS(**kw)}
    messages_mod = {"NewMessage": NS(from_live=staticmethod(lambda m: m)),
                    "ReplaceResult": lambda rows=None: NS(rows=rows),
                    "stamp_live": lambda msgs, result: None}
    raised = None
    with patched(cc, "_parent_deliberately_ended", parent_ended), \
         patched(cc, "_active_watermark", watermark), \
         patched(cc, "mint_session_id", mint), \
         patched(cc, "_compression_child_source", child_source), \
         patched(cc, "_is_real_user_message", real_user), \
         patched(cc, "_rebind_session_context", rebind), \
         patched(cc, "_carry_session_state_to_child", carry), \
         stub_modules({"agent.agent_init_session": {"agent_interface": lambda a: store},
                       "hermes_cli.profiles": {"get_active_profile_name": profile},
                       "tools.approval_yolo": {"with_session_yolo": yolo},
                       "persistence.conversation.lineage": lineage_mod,
                       "persistence.conversation.messages": messages_mod}):
        try:
            cc._publish_rotated_compaction(
                agent, messages, compressed,
                new_system_prompt=i.get("new_system_prompt", ""), lease=lease,
                old_session_id=i.get("old_session_id"),
                compressed_user_turn_outcome=i.get("compressed_user_turn_outcome", "already_present"))
        except Exception as exc:
            raised = type(exc).__name__
            if raised == "RuntimeError" and str(exc).startswith("Compression parent already ended"):
                ret_ev(ev, raised=raised, message=str(exc), state=flow_state(agent, _pub_seams))
                return ev
            ret_ev(ev, raised=raised, state=flow_state(agent, _pub_seams))
            return ev
    out = {"state": flow_state(agent, _pub_seams), "compressed": jnorm(compressed)}
    if i.get("session_messages_separate"):
        out["session_messages"] = jnorm(agent._session_messages)
    ret_ev(ev, **out)
    return ev


@handler("carry_session_state_to_child")
def h_carry(i):
    ev = []
    store_present = bool(i.get("store_present"))

    def automation(a, old_sid):
        rec(ev, "carry_automation", old_session_id=old_sid, raised=bool(i.get("automation_raises")))
        if i.get("automation_raises"):
            raise RuntimeError("automation failed")

    def thinking(a, old_sid):
        rec(ev, "carry_rejected_thinking", old_session_id=old_sid,
            raised=bool(i.get("thinking_raises")))
        if i.get("thinking_raises"):
            raise RuntimeError("thinking failed")
    ts = i.get("title_source") or {}

    def get_source(sid):
        rec(ev, "get_title_source", via="store" if store_present else "db",
            raised=bool(ts.get("raises")), ret=None if ts.get("raises") else ts.get("result"))
        if ts.get("raises"):
            raise RuntimeError("source read failed")
        return ts.get("result")
    st = i.get("set_title") or {}

    def set_title(sid, title):
        rec(ev, "set_title", sid=sid, title=title, raised=bool(st.get("raises")))
        if st.get("raises"):
            raise RuntimeError("title write failed")

    def set_source(sid, src):
        rec(ev, "set_title_source", sid=sid, source=src, raised=bool(i.get("set_source_raises")))
        if i.get("set_source_raises"):
            raise RuntimeError("source write failed")
    if store_present:
        store = NS(sessions=NS(get_title_source=get_source, set_title=set_title,
                               set_title_source=set_source))
    else:
        store = None
    agent = NS(session_id=i.get("new_session_id"),
               _session_db=NS(get_session_title_source=get_source, set_session_title=set_title,
                              set_session_title_source=set_source))
    with stub_modules({"agent.compression_automation": {"carry_compression_automation": automation},
                       "agent.anthropic_thinking_replay": {"carry_rejected_thinking_to_session": thinking},
                       "agent.agent_init_session": {"agent_interface": lambda a: store}}):
        try:
            cc._carry_session_state_to_child(agent, i.get("old_session_id"), i.get("old_title"))
        except Exception:
            ret_ev(ev, raised="Exception")
            return ev
    ret_ev(ev)
    return ev


@handler("route_codex_compaction")
def h_codex_route(i):
    ev = []
    fence_present = bool(i.get("fence_present"))
    fence = None
    if fence_present:
        fence = NS()

        def begin_commit(cancel_event=None):
            r = bool(i.get("begin_commit"))
            rec(ev, "begin_commit", ret=r)
            return r
        fence.begin_commit = begin_commit
        fence.finish_commit = lambda: rec(ev, "finish_commit")
    agent = NS(_hard_interrupt_requested=None,
               context_compressor=NS(),
               _cached_system_prompt=i.get("cached"),
               _build_system_prompt=lambda sm: i.get("build_result", ""))
    attempt = NS(started_at=float(i.get("started_at", 0)), seed=i.get("seed") or {})
    attempt.restore_compressor = lambda c: rec(ev, "restore_compressor")
    cx = i.get("codex") or {}

    def codex_compact(a, msgs, sm, approx_tokens=None, task_id=None, force=None,
                      started_at=None, attempt_seed=None):
        raised = cx.get("raises")
        r = cx.get("result")
        rec(ev, "codex_compact", raised=raised,
            ret=None if raised else {"messages": r.get("messages"), "prompt": r.get("prompt")})
        if raised:
            raise type(raised, (Exception,), {})("codex failed")
        return copy.deepcopy(r.get("messages")), r.get("prompt")
    raised = None
    with patched(cc, "_compress_context_via_codex_app_server", codex_compact), \
         patched(cc, "_emit_bypassed_attempt_telemetry", telemetry_stub(ev, "bypassed")), \
         patched(cc, "_existing_system_prompt", existing_prompt_wrapper(ev, cc._existing_system_prompt)):
        try:
            r = cc._route_codex_compaction(agent, i.get("messages") or [], "sys",
                                           commit_fence=fence, attempt=attempt,
                                           approx_tokens=i.get("approx_tokens"),
                                           task_id="t", force=bool(i.get("force")))
        except Exception as exc:
            raised = type(exc).__name__
            r = None
    if raised:
        ret_ev(ev, raised=raised)
    else:
        ret_ev(ev, v=[jnorm(r[0]), r[1]])
    return ev
# ---------------------------------------------------------------- stall ladder

@handler("stall_retry_routes")
def h_stall_routes(i):
    ev = []

    def resolve():
        rec(ev, "resolve_fallback_route", ret=i.get("chain_route"))
        return copy.deepcopy(i.get("chain_route")) if i.get("chain_route") is not None else None
    with patched(cc, "resolve_compression_fallback_route", resolve):
        r = cc._stall_retry_routes(bool(i.get("escalate_deterministic")))
    ret_ev(ev, v=jnorm(r))
    return ev


@handler("retry_compression_on_fallback_chain")
def h_retry_chain(i):
    ev = []
    agent = NS()
    if i.get("hard_cancel_present"):
        def is_set():
            v = bool(i.get("hard_cancel_set"))
            rec(ev, "hard_cancel_check", set=v)
            return v
        agent._hard_interrupt_requested = NS(is_set=is_set)
    routes = copy.deepcopy(i.get("routes") or [])
    results = list(i.get("pinned_results") or [])
    calls = {"n": 0}

    def routes_stub(escalate):
        rec(ev, "stall_retry_routes", escalate=bool(escalate), ret=copy.deepcopy(routes))
        return routes

    def pinned_stub(route, **kw):
        k = calls["n"]
        calls["n"] += 1
        r = results[k] if k < len(results) else None
        rec(ev, "pinned_retry", label=route.get("label"), ret=r)
        return copy.deepcopy(r) if r is not None else None
    with patched(cc, "_stall_retry_routes", routes_stub), \
         patched(cc, "_run_pinned_compression_retry", pinned_stub):
        r = cc._retry_compression_on_fallback_chain(
            worker=lambda fence: ([], ""), messages=[], system_prompt_fallback="",
            idle_timeout_seconds=30.0, total_ceiling_seconds=60.0,
            precommit_deadline_monotonic=100.0, telemetry_agent=agent,
            escalate_deterministic=bool(i.get("escalate_deterministic")))
    ret_ev(ev, v=jnorm(r) if r is not None else None)
    return ev


class _RetryLogger:
    """Classify the stall-retry log branches (the observable decision)."""

    def __init__(self, ev):
        self.ev = ev

    def _emit(self, level, msg, args):
        if msg.startswith("Skipping compression stall fallback on %s: the shared pre-commit ceiling"):
            rec(self.ev, "log_skip", reason="ceiling_spent", label=args[0])
        elif msg.startswith("compression stall-fallback fence factory failed"):
            rec(self.ev, "log_fence_factory_failed")
        elif msg.startswith("compression stall-fallback retry running on an unpublished fence"):
            rec(self.ev, "log_unpublished_fence")
        elif msg.startswith("Skipping compression stall fallback on %s: its setup spent"):
            rec(self.ev, "log_skip", reason="setup_spent", label=args[0])
        elif msg.startswith("Context compression stalled on every summary route"):
            rec(self.ev, "log_deterministic_commit", label=args[0])
        elif msg.startswith("Context compression stalled on the configured summary route"):
            rec(self.ev, "log_retry", label=args[0], model=args[1], ceiling=args[2])
        elif msg.startswith("Context compression fallback attempt on %s failed"):
            rec(self.ev, "log_fallback_failed", label=args[0])
        elif msg.startswith("Context compression fallback attempt on %s produced no compression"):
            rec(self.ev, "log_no_compression", label=args[0])
        elif msg.startswith("Context compression committed a deterministic fallback summary"):
            rec(self.ev, "recovery_verdict", kind="deterministic_fallback", label=args[0])
        elif msg.startswith("Context compression recovered on"):
            rec(self.ev, "recovery_verdict", kind="recovered", label=args[0])

    def warning(self, msg, *args, **kw):
        self._emit("warning", msg, args)

    def info(self, msg, *args, **kw):
        self._emit("info", msg, args)

    def debug(self, msg, *args, **kw):
        pass

    def error(self, msg, *args, **kw):
        self._emit("error", msg, args)


@handler("run_pinned_compression_retry")
def h_pinned_retry(i):
    ev = []
    route = copy.deepcopy(i.get("route") or {})
    now = float(i.get("now", 0))
    precommit = float(i.get("precommit_deadline", 0))
    nf = i.get("new_fence") or {}
    run = i.get("run") or {}
    messages = []
    comp = NS(_fallback_compression_streak=kind_value(i.get("streak_before", 0),
              "int" if i.get("streak_before_int") else "str"))
    agent = NS(context_compressor=comp)

    class RecFence(cc.CompressionCommitFence):
        def revoke_commit_admission(self):
            rec(ev, "revoke_commit_admission")
            super().revoke_commit_admission()

    def new_fence():
        if nf.get("raises"):
            raise RuntimeError("factory failed")
        return RecFence() if nf.get("valid") else object()
    nf_arg = new_fence if nf.get("present") else None
    if not route.get("deterministic"):
        rec(ev, "budget_check", now=now, deadline=precommit, spent=now >= precommit)
    # Live short-circuits BEFORE the fence stage when the shared budget is spent.
    if route.get("deterministic") or now < precommit:
        rec(ev, "new_fence", present=bool(nf.get("present")), raised=bool(nf.get("raises")),
            valid=bool(nf.get("valid")) and not nf.get("raises"))

    @contextlib.contextmanager
    def pin(r):
        rec(ev, "pin_summary_route", label=r.get("label"))
        yield

    def run_stub(worker=None, messages=None, system_prompt_fallback=None,
                 idle_timeout_seconds=None, total_ceiling_seconds=None,
                 on_timeout=None, on_timeout_cause=None, on_commit_overrun=None,
                 fence=None, telemetry_agent=None, stall_fallback=True,
                 new_fence=None, fallback_worker=None, request_exceeds_window=False,
                 precommit_deadline_monotonic=None):
        rec(ev, "run_with_timeout", idle=idle_timeout_seconds, ceiling=total_ceiling_seconds,
            stall_fallback=bool(stall_fallback), deadline=precommit_deadline_monotonic,
            raised=bool(run.get("raises")))
        comp._fallback_compression_streak = kind_value(i.get("streak_after", 0),
                  "int" if i.get("streak_after_int") else "str")
        if run.get("raises"):
            raise RuntimeError("run failed")
        if run.get("same_list"):
            return messages, ""
        r = run.get("result") or {}
        return copy.deepcopy(r.get("messages")), r.get("prompt")
    with patched(cc, "CompressionCommitFence", RecFence), \
         patched(cc.time, "monotonic", lambda: now), \
         patched(cc, "run_compress_context_with_progress_timeout", run_stub), \
         patched(CX, "pin_summary_route", pin), \
         patched(cc, "logger", _RetryLogger(ev)):
        r = cc._run_pinned_compression_retry(
            route, worker=lambda fence: ([], ""), messages=messages,
            system_prompt_fallback="", idle_timeout_seconds=float(i.get("idle_timeout_seconds", 0)),
            total_ceiling_seconds=float(i.get("total_ceiling_seconds", 0)),
            precommit_deadline_monotonic=precommit, on_commit_overrun=None,
            on_timeout_cause=None, telemetry_agent=agent, new_fence=nf_arg)
    ret_ev(ev, v=jnorm(r) if r is not None else None)
    return ev


@handler("join_cancelled_worker")
def h_join_worker(i):
    ev = []
    outcome = i.get("outcome", "ok")

    class Future:
        def result(self, timeout=None):
            rec(ev, "future_result", grace=timeout, outcome=outcome)
            if outcome == "ok":
                return "R"
            if outcome == "timeout":
                raise concurrent.futures.TimeoutError()
            if outcome == "cancelled":
                raise concurrent.futures.CancelledError()
            raise RuntimeError("worker died")

        def done(self):
            return bool(i.get("done"))
    grace = kind_value(i.get("grace"), i.get("grace_kind", "int"))
    r = cc._join_cancelled_worker(Future(), grace)
    ret_ev(ev, v=r)
    return ev


# ---------------------------------------------------------------- worker waits

class _WaitLogger:
    def __init__(self, ev):
        self.ev = ev

    def info(self, msg, *args, **kw):
        if msg.startswith("Context compression worker exited with"):
            rec(self.ev, "log_worker_exited")
        elif msg.startswith("Context compression still streaming"):
            rec(self.ev, "log_extending_wait", waited=args[0],
                since_progress=args[1], ceiling=args[2])

    def warning(self, msg, *args, **kw):
        if msg.startswith("Context compression SessionDB commit still running"):
            rec(self.ev, "log_commit_overrun", level="warning",
                over=args[0], waited=args[1], ceiling=args[2])

    def error(self, msg, *args, **kw):
        if msg.startswith("Context compression SessionDB commit still running"):
            rec(self.ev, "log_commit_overrun", level="error",
                over=args[0], waited=args[1], ceiling=args[2])

    def debug(self, msg, *args, **kw):
        pass


@handler("await_worker_within_budget")
def h_await_worker(i):
    ev = []
    ticks = list(i.get("ticks") or [])
    idle = float(i.get("idle", 0))
    ceiling = float(i.get("ceiling", 0))
    wait_started = float(i.get("wait_started", 0))
    pos = {"k": 0, "remaining": 0.0, "sp_seen": False}

    def tick():
        return ticks[min(pos["k"], len(ticks) - 1)] if ticks else {}

    class Fence:
        @property
        def deadline_monotonic(self):
            t = tick()
            pos["sp_seen"] = False
            dl = t.get("deadline")
            base = dl if dl is not None else wait_started + ceiling
            pos["remaining"] = base - t.get("now", 0)
            if pos["remaining"] <= 0:
                rec(ev, "poll", remaining_ceiling=pos["remaining"])
            return dl

        def seconds_since_progress(self):
            t = tick()
            sp = t.get("since_progress", 0)
            if pos["remaining"] > 0 and not pos["sp_seen"]:
                pos["sp_seen"] = True
                ws = idle - sp
                if ws < 0.005:
                    ws = 0.005
                if ws > pos["remaining"]:
                    ws = pos["remaining"]
                rec(ev, "poll", remaining_ceiling=pos["remaining"],
                    since_progress=sp, wait_slice=ws)
            return sp

        @property
        def deadline_exceeded(self):
            t = tick()
            dl = t.get("deadline")
            return dl is not None and t.get("now", 0) >= dl

    class Future:
        def __init__(self):
            self.cur = {}

        def result(self, timeout=None):
            if timeout is None:
                # done() branch: the settled future's own result, no new wait
                rec(ev, "settled_done")
                return self.cur.get("result")
            self.cur = tick()
            t = self.cur
            pos["k"] += 1
            if t.get("outcome") == "result":
                return t.get("result")
            raise concurrent.futures.TimeoutError()

        def done(self):
            return bool(self.cur.get("done"))

        def exception(self):
            return None if self.cur.get("exc_none") else RuntimeError("worker died")
    with patched(cc.time, "monotonic", lambda: tick().get("now", 0)), \
         patched(cc, "logger", _WaitLogger(ev)):
        settled, result = cc._await_worker_within_budget(
            Future(), Fence(), idle=idle, ceiling=ceiling, wait_started=wait_started)
    ret_ev(ev, settled=settled, result=jnorm(result))
    return ev


@handler("await_in_flight_commit")
def h_await_commit(i):
    ev = []
    ticks = list(i.get("ticks") or [])
    ceiling = float(i.get("ceiling", 0))
    wait_started = float(i.get("wait_started", 0))
    pos = {"k": 0}

    def tick():
        return ticks[min(pos["k"], len(ticks) - 1)] if ticks else {}

    def overrun(waited, ceil):
        rec(ev, "on_commit_overrun", waited=waited, ceiling=ceil,
            raised=bool(i.get("callback_raises")))
        if i.get("callback_raises"):
            raise RuntimeError("overrun cb failed")
    cb = overrun if i.get("callback_present") else None

    class Future:
        def __init__(self):
            self.cur = {}

        def result(self, timeout=None):
            if timeout is None:
                # done() branch: the settled future's own result, no new wait
                rec(ev, "settled_done")
                return self.cur.get("result")
            self.cur = tick()
            t = self.cur
            waited = t.get("now", 0) - wait_started
            remaining = ceiling - waited
            if remaining <= 0:
                remaining = min(30.0, max(ceiling, 0.05))
            rec(ev, "commit_poll", waited=waited, remaining=remaining)
            pos["k"] += 1
            if t.get("outcome") == "result":
                return t.get("result")
            raise concurrent.futures.TimeoutError()

        def done(self):
            return bool(self.cur.get("done"))
    with patched(cc.time, "monotonic", lambda: tick().get("now", 0)), \
         patched(cc, "logger", _WaitLogger(ev)):
        result = cc._await_in_flight_commit(Future(), ceiling=ceiling,
                                            wait_started=wait_started, on_commit_overrun=cb)
    ret_ev(ev, result=jnorm(result))
    return ev


@handler("cancel_or_join_worker")
def h_cancel_or_join(i):
    ev = []
    steps = list(i.get("steps") or [])
    pos = {"k": 0}

    def step():
        return steps[min(pos["k"], len(steps) - 1)] if steps else {}

    class Fence:
        @property
        def commit_in_flight(self):
            s = step()
            rec(ev, "cancel_poll", in_flight=bool(s.get("in_flight")),
                try_cancel=s.get("try_cancel"))
            return bool(s.get("in_flight"))

        def try_cancel_before_commit(self):
            return step().get("try_cancel")

    def sleep(seconds):
        rec(ev, "sleep", seconds=seconds)
        pos["k"] += 1
    with patched(cc.time, "sleep", sleep):
        r = cc._cancel_or_join_worker(Fence())
    ret_ev(ev, v=r)
    return ev


@handler("release_cancelled_worker")
def h_release_worker(i):
    ev = []
    ceiling = float(i.get("ceiling", 0))

    def join_stub(future, grace):
        r = bool(i.get("join_result"))
        rec(ev, "join_cancelled_worker", grace=grace, ret=r)
        return r
    fence = NS(allow_cancelled_lock_release=lambda: rec(ev, "allow_cancelled_lock_release"),
               release_cancelled_compression_lock=lambda: rec(ev, "release_cancelled_compression_lock"))

    class L:
        def warning(self, msg, *args, **kw):
            if msg.startswith("Cancelled compression worker did not exit"):
                rec(ev, "log_orphan", grace=args[0])

        def info(self, *a, **k):
            pass

        def debug(self, *a, **k):
            pass

        def error(self, *a, **k):
            pass
    with patched(cc, "_join_cancelled_worker", join_stub), patched(cc, "logger", L()):
        cc._release_cancelled_worker(object(), fence,
                                     total_exhausted=bool(i.get("total_exhausted")),
                                     ceiling=ceiling)
    ret_ev(ev)
    return ev


# ---------------------------------------------------------------- main

def main():
    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            fatal(f"bad harness line: {line[:120]}")
        fn = c.get("func")
        if fn not in HANDLERS:
            fatal(f"unknown func {fn!r}")
        if "in" not in c or "out" not in c:
            fatal(f"harness line missing in/out for {fn!r}")
        total += 1
        try:
            want = HANDLERS[fn](c["in"] or {})
        except Exception as exc:
            mism += 1
            print(f"MISMATCH {total} ({fn}): python raised {exc!r}")
            continue
        got = c["out"]
        if got != want:
            mism += 1
            print(f"MISMATCH {total} ({fn}):")
            print(f"  C ={json.dumps(got, ensure_ascii=False)[:600]}")
            print(f"  PY={json.dumps(want, ensure_ascii=False, default=str)[:600]}")
    if total == 0:
        fatal("0 cases (link failure in disguise)")
    print(f"ORACLE conversation_compression_flow: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())