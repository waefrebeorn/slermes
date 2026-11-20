#!/usr/bin/env python3
"""Shadow mirror for tui_methods_status_flow — CODE-ONLY verification.

Transcribes the C port (src/gateway/tui_methods_status_flow.c) LITERALLY into
Python operating on raw JSON values (the C data model: dicts/lists/scalars, no
SimpleNamespace materialization), then diffs its {ret, trace, state} against the
LIVE oracle drive() over EVERY fixture case. A shadow-diff of 0 means the C
transcription agrees with the live quarry reference on the whole fixture corpus
without compiling or linking anything.

The mirror reproduces the C helpers (truthy / pystr / strip / py_float /
json_eq / durable_ids), the universal seam tmst_call (fail-closed, null-arg
drop, __script__ wrapper, array cursor), the exception-parent table, the
_sessions state mirror and the current_sid pointer-identity model. It imports
only the shared canon() projection from the oracle (the representation
contract), never the oracle's drive logic.

Run: python tests/shadow_tui_methods_status_flow.py
"""
import copy
import importlib.util
import json
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parent.parent
_FIX = _REPO / "tests" / "fixtures" / "tui_methods_status_flow.jsonl"

# import the oracle module for the shared canon() projection + live drive()
_spec = importlib.util.spec_from_file_location(
    "orc_tmst", _REPO / "tests" / "oracle_tui_methods_status_flow.py")
_ORC = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_ORC)
canon = _ORC.canon

# ── C helper transcriptions ──


def truthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0.0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def pystr(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        d = float(v)
        if d == int(d) and -9.2e18 < d < 9.2e18:
            return str(int(d))
        return repr(d)
    return json.dumps(v)


def strip(s):
    return s.strip(" \t\n\r\v\f")


def py_float(v):
    """Return (ok, value) mirroring C tmst_py_float."""
    if isinstance(v, bool):
        return True, (1.0 if v else 0.0)
    if isinstance(v, (int, float)):
        return True, float(v)
    if isinstance(v, str):
        s = v
        try:
            # C strtod + trailing-whitespace-only check
            core = s.strip(" \t\n\r\v\f")
            if core == "":
                return False, 0.0
            f = float(core)
            return True, f
        except ValueError:
            return False, 0.0
    return False, 0.0


def json_eq(a, b):
    if a is None:
        return b is None
    if b is None:
        return False
    if isinstance(a, bool) and isinstance(b, bool):
        return a == b
    if isinstance(a, bool) and isinstance(b, (int, float)):
        return (1.0 if a else 0.0) == b
    if isinstance(a, (int, float)) and isinstance(b, bool):
        return a == (1.0 if b else 0.0)
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a == b
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    if type(a) is not type(b):
        # bool/int cross already handled; list vs dict etc.
        if isinstance(a, (list, dict)) and isinstance(b, (list, dict)):
            pass
        else:
            return False
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(json_eq(x, y) for x, y in zip(a, b))
    if isinstance(a, dict) and isinstance(b, dict):
        if len(a) != len(b):
            return False
        return all(k in b and json_eq(v, b[k]) for k, v in a.items())
    return False


MESSAGE_UID = "message_uid"
ABSORBED_UIDS = "_absorbed_message_uids"


def durable_ids(msg):
    """Transcription of C tmst_durable_ids: sorted, deduped [tag, id] array."""
    out = []
    if not isinstance(msg, dict):
        return out
    tmp = []
    uid = msg.get(MESSAGE_UID)
    if isinstance(uid, str) and uid:
        tmp.append(["uid", uid])
    absorbed = msg.get(ABSORBED_UIDS)
    if isinstance(absorbed, list):
        for u in absorbed:
            if isinstance(u, str) and u:
                tmp.append(["uid", u])
    row = msg.get("_row_id")
    if isinstance(row, (int, float)) and not isinstance(row, bool) and float(row) == int(float(row)):
        tmp.append(["row", row])
    for p in tmp:
        if not any(json_eq(p, q) for q in out):
            out.append(copy.deepcopy(p))
    # insertion sort by (tag, id): row numeric, uid lexical
    def cmp_key(p):
        tag = p[0]
        idv = p[1]
        return (tag, idv if tag == "row" else str(idv))
    # C compares tag strcmp then id; row ids numeric, uid lexical. Python tuple
    # sort with homogeneous id types per tag matches (tags differ first).
    out.sort(key=lambda p: (p[0], p[1] if p[0] == "row" else p[1]))
    return out


def ids_intersect(a, b):
    return any(json_eq(x, y) for x in a for y in b)


# ── exception parent table (transcribed from C TMST_EXC_PARENTS) ──
EXC_PARENTS = {
    "BaseException": [], "Exception": ["BaseException"],
    "SystemExit": ["BaseException"], "KeyboardInterrupt": ["BaseException"],
    "GeneratorExit": ["BaseException"], "CancelledError": ["BaseException"],
    "StopIteration": ["Exception"], "StopAsyncIteration": ["Exception"],
    "ArithmeticError": ["Exception"], "FloatingPointError": ["ArithmeticError"],
    "OverflowError": ["ArithmeticError"], "ZeroDivisionError": ["ArithmeticError"],
    "AssertionError": ["Exception"], "AttributeError": ["Exception"],
    "BufferError": ["Exception"], "EOFError": ["Exception"],
    "ImportError": ["Exception"], "ModuleNotFoundError": ["ImportError"],
    "LookupError": ["Exception"], "IndexError": ["LookupError"], "KeyError": ["LookupError"],
    "MemoryError": ["Exception"], "NameError": ["Exception"], "UnboundLocalError": ["NameError"],
    "OSError": ["Exception"], "BlockingIOError": ["OSError"], "ChildProcessError": ["OSError"],
    "ConnectionError": ["OSError"], "BrokenPipeError": ["ConnectionError"],
    "ConnectionAbortedError": ["ConnectionError"], "ConnectionRefusedError": ["ConnectionError"],
    "ConnectionResetError": ["ConnectionError"], "FileExistsError": ["OSError"],
    "FileNotFoundError": ["OSError"], "InterruptedError": ["OSError"],
    "IsADirectoryError": ["OSError"], "NotADirectoryError": ["OSError"],
    "PermissionError": ["OSError"], "ProcessLookupError": ["OSError"], "TimeoutError": ["OSError"],
    "ReferenceError": ["Exception"], "RuntimeError": ["Exception"],
    "NotImplementedError": ["RuntimeError"], "RecursionError": ["RuntimeError"],
    "SyntaxError": ["Exception"], "IndentationError": ["SyntaxError"], "TabError": ["IndentationError"],
    "SystemError": ["Exception"], "TypeError": ["Exception"], "ValueError": ["Exception"],
    "UnicodeError": ["ValueError"], "UnicodeDecodeError": ["UnicodeError"],
    "UnicodeEncodeError": ["UnicodeError"], "UnicodeTranslateError": ["UnicodeError"],
    "Warning": ["Exception"], "DeprecationWarning": ["Warning"],
    "PendingDeprecationWarning": ["Warning"], "RuntimeWarning": ["Warning"],
    "SyntaxWarning": ["Warning"], "UserWarning": ["Warning"], "FutureWarning": ["Warning"],
    "ImportWarning": ["Warning"], "UnicodeWarning": ["Warning"], "BytesWarning": ["Warning"],
    "ResourceWarning": ["Warning"], "EncodingWarning": ["Warning"],
}


def exc_isa(kind, ancestor):
    if kind is None:
        return False
    if kind == ancestor:
        return True
    parents = EXC_PARENTS.get(kind)
    if parents is None:
        return ancestor in ("Exception", "BaseException")
    return any(exc_isa(p, ancestor) for p in parents)


def is_base_exc(kind):
    return kind is not None and not exc_isa(kind, "Exception")


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


def raise_kind(v):
    return v.get("__raise__") if is_raise(v) else None


def err_spec(kind, message):
    return {"__raise__": kind, "message": message}


AGENT_STILL_STARTING = (
    "Hermes is still starting this session (loading tools), so this command could not run yet. "
    "Wait for the status bar to show ready and try again.")
SESS_NOWAIT_FMT = (
    "session-scoped RPC rejected: method=%s session_id=%r not in memory "
    "(detached/reaped runtime; client should resume the stored session), rid=%r")


# ── universal seam (mirrors C tmst_call) ──
class ShadowCtx:
    def __init__(self, state, io):
        self.state = state
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        clean = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, None)
        has = name in self.io
        used = entry
        ret = None
        if not has:
            ret = err_spec("RuntimeError", f"no script for {name}")
        elif isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                ret = err_spec("RuntimeError", f"script exhausted for {name}")
            else:
                self.cursors[name] = idx + 1
                used = entry[idx]
        if ret is None:
            if isinstance(used, dict) and used.get("__script__"):
                r = used.get("raise")
                if isinstance(r, dict):
                    ret = err_spec(r.get("type", "RuntimeError"), r.get("message", ""))
                else:
                    ret = copy.deepcopy(used.get("ret"))
            else:
                ret = copy.deepcopy(used)
        self.trace.append({"name": name, "args": canon(clean), "ret": canon(ret)})
        return ret

    def sessions(self):
        s = self.state.get("sessions")
        if not isinstance(s, dict):
            s = {}
            self.state["sessions"] = s
        return s

    def current(self, args):
        csid = args.get("current_sid")
        sessions = self.sessions()
        if csid and csid in sessions and isinstance(sessions[csid], dict):
            return sessions[csid], True
        return args.get("current"), False

    def is_current(self, sid, current, in_registry):
        if not in_registry or current is None:
            return False
        return self.sessions().get(sid) is current


def propagate(r):
    return is_raise(r)


# ── ported function transcriptions ──

def f_is_server_minted_key(ctx, args):
    value = args.get("value")
    if not truthy(value):
        return False
    if not isinstance(value, str) or len(value) != 22:
        return False
    for i, c in enumerate(value):
        if i in (8, 15):
            if c != "_":
                return False
        elif i < 8 or 9 <= i <= 14:
            if not ("0" <= c <= "9"):
                return False
        else:
            if not (("0" <= c <= "9") or ("a" <= c <= "f")):
                return False
    return True


def f_live_profile_matches(ctx, args):
    session = args.get("session")
    ph = args.get("profile_home")
    if isinstance(ph, str) and ph == "__ANY_PROFILE__":
        return True
    sh = session.get("profile_home") if isinstance(session, dict) else None
    sh_truthy = truthy(sh)
    ph_str = pystr(ph) if truthy(ph) else None
    if not sh_truthy and ph_str is None:
        return True
    if not sh_truthy or ph_str is None:
        return False
    return pystr(sh) == ph_str


def f_session_lookup_key(ctx, args):
    session = args.get("session")
    fallback = args.get("fallback")
    agent = session.get("agent") if isinstance(session, dict) else None
    asid = agent.get("session_id") if isinstance(agent, dict) else None
    skey = session.get("session_key") if isinstance(session, dict) else None
    val = asid if truthy(asid) else skey if truthy(skey) else fallback if truthy(fallback) else None
    return pystr(val) if val is not None else ""


def f_durable_ids(ctx, args):
    return durable_ids(args.get("msg"))


def f_legacy_display_anchor(ctx, args):
    display = args.get("display")
    memory = args.get("memory")
    prefix = []
    if isinstance(display, list):
        for msg in display:
            role = msg.get("role") if isinstance(msg, dict) else None
            content = msg.get("content") if isinstance(msg, dict) else None
            text = ctx.call("_coerce_message_text", {"content": content})
            if propagate(text):
                return text
            prefix.append([role, text])
    cursor = 0
    plen = len(prefix)
    result = -1
    if isinstance(memory, list):
        for index, msg in enumerate(memory):
            if not isinstance(msg, dict):
                continue
            role = msg.get("role")
            content = msg.get("content")
            text = ctx.call("_coerce_message_text", {"content": content})
            if propagate(text):
                return text
            value = [role, text]
            found = -1
            for k in range(cursor, plen):
                if json_eq(value, prefix[k]):
                    found = k
                    break
            if found < 0:
                continue
            cursor = found + 1
            if cursor == plen:
                result = index
                break
    return result


def f_main_runtime_from_agent(ctx, args):
    agent = args.get("agent")
    if agent is None:
        return None
    runtime = {}
    for field in ("provider", "model", "base_url", "api_key", "api_mode", "auth_mode", "session_id"):
        value = agent.get(field) if isinstance(agent, dict) else None
        if isinstance(value, str):
            s = strip(value)
            if s:
                runtime[field] = s
        elif field == "api_key" and isinstance(value, dict) and value.get("__callable__"):
            runtime[field] = copy.deepcopy(value)
    return runtime or None


def f_new_session_key(ctx, args):
    return ctx.call("new_session_id", {})


def f_session_pending_kind(ctx, args):
    return ctx.call("pending_kind", {"sid": args.get("sid")})


def f_session_live_status(ctx, args):
    sid = args.get("sid")
    session = args.get("session")
    kind = ctx.call("_session_pending_kind", {"sid": sid})
    if propagate(kind):
        return kind
    if truthy(kind):
        return "waiting"
    ready = session.get("agent_ready") if isinstance(session, dict) else None
    if ready is not None:
        isset = ctx.call("agent_ready.is_set", {})
        if propagate(isset):
            return isset
        set_ = truthy(isset)
        started = session.get("agent_build_started") if isinstance(session, dict) else None
        if not set_ and truthy(started):
            return "starting"
    running = session.get("running") if isinstance(session, dict) else None
    return "working" if truthy(running) else "idle"


def f_session_live_title(ctx, args):
    session = args.get("session")
    key = args.get("key")
    pt = session.get("pending_title") if isinstance(session, dict) else None
    title = strip(pystr(pt) if truthy(pt) else "")
    db = ctx.call("session_db", {"session": session})
    if is_raise(db):
        if is_base_exc(raise_kind(db)):
            return db
        return title
    persistence = ctx.call("persistence_for", {"db": "db" if db is not None else None})
    if is_raise(persistence):
        if is_base_exc(raise_kind(persistence)):
            return persistence
        return title
    if persistence is not None:
        gt = ctx.call("get_title", {"key": key})
        if is_raise(gt):
            if is_base_exc(raise_kind(gt)):
                return gt
            return title
        pick = gt if truthy(gt) else (title if title else "")
        title = strip(pick if isinstance(pick, str) else "")
    return title


def f_reconcile_display_with_live(ctx, args):
    db_display = args.get("db_display")
    in_memory = args.get("in_memory")
    if not truthy(db_display):
        return copy.deepcopy(in_memory) if in_memory is not None else None
    if not truthy(in_memory):
        return copy.deepcopy(db_display)
    last = db_display[-1]
    anchor = durable_ids(last)
    last_shared = -1
    for idx, msg in enumerate(in_memory):
        ids = durable_ids(msg)
        if ids_intersect(ids, anchor):
            last_shared = idx
    if last_shared == -1:
        any_ids = False
        if len(anchor) > 0:
            for msg in in_memory:
                ids = durable_ids(msg)
                if len(ids) > 0:
                    any_ids = True
                    break
        if len(anchor) > 0 and any_ids:
            return copy.deepcopy(db_display)
        ar = f_legacy_display_anchor(ctx, {"display": db_display, "memory": in_memory})
        if is_raise(ar):
            return ar
        last_shared = int(ar)
    if last_shared == -1:
        return copy.deepcopy(db_display)
    return copy.deepcopy(db_display) + copy.deepcopy(in_memory[last_shared + 1:])


def f_any_live_session_claims_key(ctx, args):
    target = args.get("target")
    t = pystr(target)
    sessions = ctx.sessions()
    for rec in sessions.values():
        if not isinstance(rec, dict):
            continue
        sk = rec.get("session_key")
        s = pystr(sk) if truthy(sk) else ""
        if s == t:
            return True
    return False


def f_find_live_session_by_key(ctx, args):
    session_key = args.get("session_key")
    ph = args.get("profile_home")
    if ph is None and "profile_home" not in args:
        ph = "__ANY_PROFILE__"
    sessions = ctx.sessions()
    for sid, session in sessions.items():
        if not isinstance(session, dict):
            continue
        if truthy(session.get("_finalized")):
            continue
        lk = f_session_lookup_key(ctx, {"session": session, "fallback": sid})
        if not json_eq(lk, session_key):
            continue
        pm = f_live_profile_matches(ctx, {"session": session, "profile_home": ph})
        if not truthy(pm):
            continue
        return [sid, copy.deepcopy(session)]
    return None


def f_claim_parked_runtimes(ctx, args):
    session_key = args.get("session_key")
    ks = args.get("keep_sid") or ""
    ph = args.get("profile_home")
    if ph is None and "profile_home" not in args:
        ph = "__ANY_PROFILE__"
    sessions = ctx.sessions()
    cand = []
    for old_sid, old in sessions.items():
        if not isinstance(old, dict):
            continue
        if old_sid == ks:
            continue
        if truthy(old.get("_finalized")):
            continue
        lk = f_session_lookup_key(ctx, {"session": old, "fallback": old_sid})
        if not json_eq(lk, session_key):
            continue
        pm = f_live_profile_matches(ctx, {"session": old, "profile_home": ph})
        if not truthy(pm):
            continue
        tr = old.get("transport")
        if not (isinstance(tr, str) and tr == "__DETACHED_WS__"):
            continue
        cand.append(old_sid)
    stale = []
    for old_sid in cand:
        r = ctx.call("_cancel_ws_orphan_reap", {"sid": old_sid})
        if propagate(r):
            return r
        popped = ctx.call("_pop_session_by_id", {"sid": old_sid})
        if propagate(popped):
            return popped
        if popped is not None:
            sessions.pop(old_sid, None)
            stale.append([old_sid, popped])
    return stale


def f_finalize_superseded_runtimes(ctx, args):
    stale = args.get("stale")
    if isinstance(stale, list):
        for pair in stale:
            old_sid = pair[0] if isinstance(pair, list) and len(pair) > 0 else None
            popped = pair[1] if isinstance(pair, list) and len(pair) > 1 else None
            r = ctx.call("_teardown_popped_session", {"popped": popped, "end_reason": "superseded_by_resume"})
            if is_raise(r):
                if is_base_exc(raise_kind(r)):
                    return r
                lr = ctx.call("log.exception", {"fmt": "superseded runtime teardown failed sid=%s", "args": [old_sid]})
                if propagate(lr):
                    return lr
    return None


def f_claim_or_reuse_live(ctx, args):
    sid = args.get("sid")
    session_key = args.get("session_key")
    record = args.get("record")
    lease = args.get("lease")
    ph = record.get("profile_home") if isinstance(record, dict) else None
    sids = sid if isinstance(sid, str) else ""
    live = ctx.call("_find_live_session_by_key", {"session_key": session_key, "profile_home": ph})
    if propagate(live):
        return live
    if live is not None:
        if lease is not None:
            r = ctx.call("lease.release", {})
            if propagate(r):
                return r
        return live
    sessions = ctx.sessions()
    sessions[sids] = copy.deepcopy(record)
    registered = sessions.get(sids)
    r2 = ctx.call("_register_session_cwd", {"record": registered})
    if propagate(r2):
        return r2
    r3 = ctx.call("_cancel_ws_orphan_reap", {"sid": sid})
    if propagate(r3):
        return r3
    stale = ctx.call("_claim_parked_runtimes", {"session_key": session_key, "keep_sid": sid, "profile_home": ph})
    if propagate(stale):
        return stale
    r5 = ctx.call("_finalize_superseded_runtimes", {"stale": stale})
    if propagate(r5):
        return r5
    return None


def f_start_session_services(ctx, args):
    sid = args.get("sid")
    key = args.get("key")
    current = args.get("current")
    sessions = ctx.sessions()
    sids = sid if isinstance(sid, str) else ""
    rec = sessions.get(sids)
    if rec is not None:
        old_stop = rec.get("_notif_stop") if isinstance(rec, dict) else None
        if old_stop is not None:
            r = ctx.call("old_stop.set", {})
            if propagate(r):
                return r
        ns = ctx.call("_start_notification_poller", {"sid": sid, "rec": rec})
        if propagate(ns):
            return ns
        rec["_notif_stop"] = ns
    src = ctx.call("_session_source", {"session": current})
    if propagate(src):
        return src
    r3 = ctx.call("_notify_session_boundary", {"hook": "on_session_reset", "key": key, "source": src})
    if propagate(r3):
        return r3
    return None


def f_wait_agent(ctx, args):
    session = args.get("session")
    rid = args.get("rid")
    timeout = args.get("timeout", 30.0)
    ready = session.get("agent_ready") if isinstance(session, dict) else None
    if ready is not None:
        w = ctx.call("agent_ready.wait", {"timeout": timeout})
        if propagate(w):
            return w
        if not truthy(w):
            return ctx.call("_err", {"rid": rid, "code": 5032, "message": AGENT_STILL_STARTING})
    err = session.get("agent_error") if isinstance(session, dict) else None
    if truthy(err):
        return ctx.call("_err", {"rid": rid, "code": 5032, "message": err})
    return None


def f_agent_build_wait_cap(ctx, args):
    cfg = ctx.call("_load_cfg", {})
    if is_raise(cfg):
        if is_base_exc(raise_kind(cfg)):
            return cfg
        return 600.0
    if not isinstance(cfg, dict):
        return 600.0
    agent = cfg.get("agent")
    src = agent if truthy(agent) else None
    if src is not None and not isinstance(src, dict):
        return 600.0
    raw = src.get("build_wait_timeout") if isinstance(src, dict) else None
    result = 600.0
    if raw is not None:
        ok, f = py_float(raw)
        if not ok:
            return 600.0
        if f > 0:
            result = f
    return result


def f_await_resume_history(ctx, args):
    sid = args.get("sid")
    current, in_reg = ctx.current(args)
    hr = current.get("resume_history_ready") if isinstance(current, dict) else None
    if hr is None:
        return True
    w = ctx.call("resume_history.wait", {"timeout": 300.0})
    if propagate(w):
        return w
    if not truthy(w):
        return err_spec("TimeoutError", "session history hydration timed out")
    herr = current.get("resume_history_error") if isinstance(current, dict) else None
    if truthy(herr):
        return err_spec("RuntimeError", pystr(herr))
    return ctx.is_current(sid if isinstance(sid, str) else "", current, in_reg)


def f_attach_built_agent(ctx, args):
    sid = args.get("sid")
    current, in_reg = ctx.current(args)
    agent_in = args.get("agent")
    agent = copy.deepcopy(agent_in) if isinstance(agent_in, dict) else None
    pt = current.get("pending_title") if isinstance(current, dict) else None
    hint = strip(pystr(pt) if truthy(pt) else "")
    if hint and isinstance(agent, dict):
        agent["_session_title_hint"] = hint
    if not ctx.is_current(sid if isinstance(sid, str) else "", current, in_reg):
        return False
    current["agent"] = copy.deepcopy(agent)
    r = ctx.call("_register_session_cwd", {"record": current})
    if propagate(r):
        return r
    r2 = ctx.call("_session_todo_state", {"record": current})
    if propagate(r2):
        return r2
    cm = ctx.call("_config_model_target", {})
    if propagate(cm):
        return cm
    current["config_model_seen"] = cm
    return True


def f_finish_agent_build(ctx, args):
    sid = args.get("sid")
    key = args.get("key")
    notify_registered = args.get("notify_registered", False)
    scopes = args.get("scopes")
    session_db = args.get("session_db")
    persistence = args.get("persistence")
    current, in_reg = ctx.current(args)
    if scopes is not None:
        r = ctx.call("_release_build_profile_scopes", {"scopes": scopes})
        if propagate(r):
            return r
    replaced = not ctx.is_current(sid if isinstance(sid, str) else "", current, in_reg)
    if replaced and notify_registered:
        r = ctx.call("unregister_gateway_notify", {"key": key})
        if is_raise(r):
            if is_base_exc(raise_kind(r)):
                return r
    if session_db is not None:
        agent = current.get("agent") if (not replaced and isinstance(current, dict)) else None
        tr = ctx.call("_transfer_db_to_agent", {"agent": "agent" if agent is not None else None, "db": "db"})
        if propagate(tr):
            return tr
        if not truthy(tr):
            r = ctx.call("db.close", {})
            if is_raise(r):
                if is_base_exc(raise_kind(r)):
                    return r
            r2 = ctx.call("_release_persistence", {"persistence": "persistence" if persistence is not None else None})
            if propagate(r2):
                return r2
    return None


def f_bind_build_profile_scopes(ctx, args):
    ph = args.get("profile_home")
    r = ctx.call("_profile_runtime_scope_tokens", {"profile_home": ph})
    if is_raise(r):
        if is_base_exc(raise_kind(r)):
            return r
    else:
        return r
    if not truthy(ph):
        return None
    scope = ctx.call("scope_for_profile", {"profile_home": ph})
    if is_raise(scope):
        if is_base_exc(raise_kind(scope)):
            return scope
        return None
    token = ctx.call("bind_state", {"scope": "scope", "secrets": {}})
    if is_raise(token):
        if is_base_exc(raise_kind(token)):
            return token
        return None
    return token


def f_release_build_profile_scopes(ctx, args):
    scopes = args.get("scopes")
    r = ctx.call("_release_profile_runtime_scope_tokens", {"scopes": scopes})
    if is_raise(r):
        if is_base_exc(raise_kind(r)):
            return r
    return None


def f_deferred_build_agent_kwargs(ctx, args):
    current = args.get("current")
    session_db = args.get("session_db")
    kw = {"session_db": copy.deepcopy(session_db)}
    ca = ctx.call("_context_cwd_is_launch_artifact", {"current": current})
    if propagate(ca):
        return ca
    kw["context_cwd_is_launch_artifact"] = ca
    src = ctx.call("_session_source", {"session": current})
    if propagate(src):
        return src
    kw["platform_override"] = src
    cwd = ctx.call("_session_cwd", {"session": current})
    if propagate(cwd):
        return cwd
    kw["cwd_override"] = cwd
    rsid = current.get("resume_session_id") if isinstance(current, dict) else None
    if truthy(rsid):
        kw["session_id"] = copy.deepcopy(rsid)
    ro = current.get("resume_runtime_overrides") if isinstance(current, dict) else None
    use_ro = False
    if isinstance(ro, dict) and len(ro) > 0:
        routable = ctx.call("_overrides_have_routable_provider", {"overrides": ro})
        if propagate(routable):
            return routable
        use_ro = truthy(routable)
    if use_ro:
        for k, v in ro.items():
            kw[k] = copy.deepcopy(v)
    else:
        mo = current.get("model_override") if isinstance(current, dict) else None
        if truthy(mo):
            kw["model_override"] = copy.deepcopy(mo)
        rc = current.get("create_reasoning_override") if isinstance(current, dict) else None
        if rc is not None:
            kw["reasoning_config_override"] = copy.deepcopy(rc)
        st = current.get("create_service_tier_override") if isinstance(current, dict) else None
        if st is not None:
            kw["service_tier_override"] = copy.deepcopy(st)
    return kw


def f_sess_nowait(ctx, args):
    params = args.get("params")
    rid = args.get("rid")
    psid = params.get("session_id") if isinstance(params, dict) else None
    sid = pystr(psid) if truthy(psid) else ""
    sessions = ctx.sessions()
    s = sessions.get(sid)
    if truthy(s):
        return [copy.deepcopy(s), None]
    m = ctx.call("_current_rpc_method.get", {})
    if propagate(m):
        return m
    method = m if truthy(m) else "?"
    lr = ctx.call("log.warning", {"fmt": SESS_NOWAIT_FMT, "args": [method, sid, rid]})
    if propagate(lr):
        return lr
    err = ctx.call("_err", {"rid": rid, "code": 4001, "message": "session not found"})
    if propagate(err):
        return err
    return [None, err]


def f_sess_building(ctx, args):
    params = args.get("params")
    rid = args.get("rid")
    se = ctx.call("_sess_nowait", {"params": params, "rid": rid})
    if propagate(se):
        return se
    s = se[0] if isinstance(se, list) and len(se) > 0 else None
    err = se[1] if isinstance(se, list) and len(se) > 1 else None
    if not truthy(err):
        psid = params.get("session_id") if isinstance(params, dict) else None
        sid = pystr(psid) if truthy(psid) else ""
        r = ctx.call("_start_agent_build", {"sid": sid, "session": s})
        if propagate(r):
            return r
    if truthy(err):
        return [None, copy.deepcopy(err)]
    return [copy.deepcopy(s) if s is not None else None, None]


def f_sess(ctx, args):
    params = args.get("params")
    rid = args.get("rid")
    se = ctx.call("_sess_building", {"params": params, "rid": rid})
    if propagate(se):
        return se
    s = se[0] if isinstance(se, list) and len(se) > 0 else None
    err = se[1] if isinstance(se, list) and len(se) > 1 else None
    if truthy(err):
        return [None, copy.deepcopy(err)]
    w = ctx.call("_wait_agent", {"session": s, "rid": rid, "timeout": 30.0})
    if propagate(w):
        return w
    return [copy.deepcopy(s) if s is not None else None, w]


FUNCS = {
    "_wait_agent": f_wait_agent,
    "_agent_build_wait_cap": f_agent_build_wait_cap,
    "_bind_build_profile_scopes": f_bind_build_profile_scopes,
    "_release_build_profile_scopes": f_release_build_profile_scopes,
    "_deferred_build_agent_kwargs": f_deferred_build_agent_kwargs,
    "_await_resume_history": f_await_resume_history,
    "_attach_built_agent": f_attach_built_agent,
    "_start_session_services": f_start_session_services,
    "_finish_agent_build": f_finish_agent_build,
    "_sess_nowait": f_sess_nowait,
    "_sess_building": f_sess_building,
    "_sess": f_sess,
    "_new_session_key": f_new_session_key,
    "_is_server_minted_key": f_is_server_minted_key,
    "_any_live_session_claims_key": f_any_live_session_claims_key,
    "_live_profile_matches": f_live_profile_matches,
    "_session_lookup_key": f_session_lookup_key,
    "_find_live_session_by_key": f_find_live_session_by_key,
    "_claim_parked_runtimes": f_claim_parked_runtimes,
    "_finalize_superseded_runtimes": f_finalize_superseded_runtimes,
    "_claim_or_reuse_live": f_claim_or_reuse_live,
    "_session_pending_kind": f_session_pending_kind,
    "_session_live_status": f_session_live_status,
    "_session_live_title": f_session_live_title,
    "_durable_ids": f_durable_ids,
    "_legacy_display_anchor": f_legacy_display_anchor,
    "_reconcile_display_with_live": f_reconcile_display_with_live,
    "_main_runtime_from_agent": f_main_runtime_from_agent,
}

REGISTRY_FUNCS = _ORC.REGISTRY_FUNCS


def shadow_drive(func, inp):
    inp = copy.deepcopy(inp)
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    if func in REGISTRY_FUNCS and "sessions" not in state:
        state["sessions"] = {}
    ctx = ShadowCtx(state, inp.get("io"))
    ret = FUNCS[func](ctx, args)
    state_out = dict(state)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


def main():
    n = diff = shown = 0
    for line in open(_FIX, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        c = json.loads(line)
        n += 1
        func = c["func"]
        inp = c.get("in") or {}
        want = _ORC.drive(func, inp)
        got = shadow_drive(func, inp)
        if want != got:
            diff += 1
            if shown < 15:
                print(f"SHADOW DIFF {func} (case {n}):")
                if want["ret"] != got["ret"]:
                    print(f"  ret live  ={json.dumps(want['ret'])[:240]}")
                    print(f"  ret shadow={json.dumps(got['ret'])[:240]}")
                wt, gt = want["trace"], got["trace"]
                for i in range(max(len(wt), len(gt))):
                    w = wt[i] if i < len(wt) else None
                    g = gt[i] if i < len(gt) else None
                    if w != g:
                        print(f"  trace[{i}] live  ={json.dumps(w)[:240]}")
                        print(f"  trace[{i}] shadow={json.dumps(g)[:240]}")
                        break
                if want["state"] != got["state"]:
                    print(f"  state live  ={json.dumps(want['state'])[:240]}")
                    print(f"  state shadow={json.dumps(got['state'])[:240]}")
                shown += 1
    print(f"SHADOW tui_methods_status_flow: {n} cases, {diff} diffs")
    sys.exit(1 if diff or n == 0 else 0)


if __name__ == "__main__":
    main()