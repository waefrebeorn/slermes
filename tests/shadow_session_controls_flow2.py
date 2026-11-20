#!/usr/bin/env python3
"""Literal Python shadow-mirror of src/gateway/session_controls_flow2.c (sct2_*).

Code-only verification (no C compile/link): transcribes the C helper algorithms
and entry flows FAITHFULLY (presence semantics via a MISSING sentinel, the same
FULL builtin exception-parent table, the same seam dispatcher / state model /
projections), then:
  diff  -- mirror {ret,trace,state} vs the oracle's LIVE drive over every fixture.
  proxy -- serialize mirror envelopes to JSONL and pipe into the oracle's REAL
           main() (subprocess stdin); expect 'ORACLE ...: N cases, 0 mismatches'.
Run: python tests/shadow_session_controls_flow2.py [diff|proxy|both]
"""
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIX = HERE / "fixtures" / "session_controls_flow2.jsonl"
ORACLE = HERE / "oracle_session_controls_flow2.py"
PYBIN = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"

MISSING = object()

# ---- exception-parent table (verbatim from the C, FULL builtin hierarchy) ----
EXC_PARENT = {
    "BaseException": "object", "BaseExceptionGroup": "BaseException", "Exception": "BaseException",
    "ExceptionGroup": "Exception", "GeneratorExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "SystemExit": "BaseException", "CancelledError": "BaseException", "ArithmeticError": "Exception",
    "AssertionError": "Exception", "AttributeError": "Exception", "BufferError": "Exception",
    "EOFError": "Exception", "EnvironmentError": "OSError", "IOError": "OSError",
    "ImportError": "Exception", "LookupError": "Exception", "MemoryError": "Exception",
    "NameError": "Exception", "OSError": "Exception", "ReferenceError": "Exception",
    "RuntimeError": "Exception", "StopAsyncIteration": "Exception", "StopIteration": "Exception",
    "SyntaxError": "Exception", "SystemError": "Exception", "TypeError": "Exception",
    "ValueError": "Exception", "Warning": "Exception", "WindowsError": "OSError",
    "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError",
    "ZeroDivisionError": "ArithmeticError", "ModuleNotFoundError": "ImportError",
    "IndexError": "LookupError", "KeyError": "LookupError", "UnboundLocalError": "NameError",
    "BlockingIOError": "OSError", "ChildProcessError": "OSError", "ConnectionError": "OSError",
    "FileExistsError": "OSError", "FileNotFoundError": "OSError", "InterruptedError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError", "PermissionError": "OSError",
    "ProcessLookupError": "OSError", "TimeoutError": "OSError", "BrokenPipeError": "ConnectionError",
    "ConnectionAbortedError": "ConnectionError", "ConnectionRefusedError": "ConnectionError",
    "ConnectionResetError": "ConnectionError", "NotImplementedError": "RuntimeError",
    "PythonFinalizationError": "RuntimeError", "RecursionError": "RuntimeError",
    "IndentationError": "SyntaxError", "_IncompleteInputError": "SyntaxError",
    "TabError": "IndentationError", "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "BytesWarning": "Warning", "DeprecationWarning": "Warning", "EncodingWarning": "Warning",
    "FutureWarning": "Warning", "ImportWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "ResourceWarning": "Warning", "RuntimeWarning": "Warning", "SyntaxWarning": "Warning",
    "UnicodeWarning": "Warning", "UserWarning": "Warning",
    "PersistenceError": "Exception", "AuthorityRefusal": "PersistenceError",
    "Conflict": "PersistenceError", "_SessionConflict": "Conflict",
    "CompressionBusy": "_SessionConflict", "CompressionInProgress": "CompressionBusy",
    "TurnLeaseLost": "_SessionConflict", "TransientStoreError": "PersistenceError",
    "PermanentStoreError": "PersistenceError", "CorruptStoreError": "PermanentStoreError",
    "StoreUnavailable": "PersistenceError", "StoreReplaced": "PersistenceError",
}


def caught_by(kind, names):
    cur = kind
    for _ in range(32):
        if cur is None:
            return False
        if cur in names:
            return True
        if cur in ("BaseException", "object"):
            return False
        cur = EXC_PARENT.get(cur)
    return False


# ---- value helpers ----
def truth(v):
    if v is None or v is MISSING:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def kind(r):
    return r.get("__raise__", "") if isinstance(r, dict) else ""


def mkraise(k, m):
    return {"__raise__": k, "message": m}


def has(o, k):
    return isinstance(o, dict) and k in o


def jget(o, k):
    return o[k] if isinstance(o, dict) and k in o else None


def jstr(v):
    return v if isinstance(v, str) else None


def is_obj(v):
    return isinstance(v, dict)


def is_arr(v):
    return isinstance(v, list)


def is_str(v):
    return isinstance(v, str)


def is_bool(v):
    return isinstance(v, bool)


def is_true(v):
    return isinstance(v, bool) and v


def is_int_strict(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and float(v) == int(v)


def str_eq(v, s):
    return isinstance(v, str) and v == s


def json_eq(a, b):
    if a is None or b is None:
        return a is b
    if isinstance(a, bool) != isinstance(b, bool):
        return False
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return float(a) == float(b)
    if type(a) is not type(b) and not (isinstance(a, (int, float)) and isinstance(b, (int, float))):
        return False
    if isinstance(a, dict):
        if len(a) != len(b):
            return False
        return all(k in b and json_eq(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(json_eq(x, y) for x, y in zip(a, b))
    return a == b


def copy(v):
    return json.loads(json.dumps(v)) if isinstance(v, (dict, list)) else v


def copy_or_null(v):
    return copy(v) if v is not None and v is not MISSING else None


def strdup(s):
    return s if s is not None else ""


def repr_str(s):
    if s is None:
        s = ""
    has_sq = "'" in s
    has_dq = '"' in s
    q = '"' if (has_sq and not has_dq) else "'"
    out = [q]
    for c in s:
        o = ord(c)
        if c == "\\":
            out.append("\\\\")
        elif c == q:
            out.append("\\" + c)
        elif c == "\n":
            out.append("\\n")
        elif c == "\r":
            out.append("\\r")
        elif c == "\t":
            out.append("\\t")
        elif o < 0x20 or o == 0x7F:
            out.append("\\x%02x" % o)
        else:
            out.append(c)
    out.append(q)
    return "".join(out)


def one_line(value, limit):
    if value is None:
        return None
    text = " ".join(str(value).split())
    text = "".join(ch for ch in text if ch.isprintable())
    text = text[:limit]
    return text or None


def refusal(reason, field, detail):
    e = {"__raise__": "AuthorityRefusal", "message": reason or "", "reason": reason or ""}
    e["field"] = one_line(field, 80)
    e["detail"] = one_line(detail, 160)
    return e


def make_exc(spec):
    type_ = spec.get("type", "RuntimeError")
    if type_ == "AuthorityRefusal":
        return refusal(spec.get("reason", ""), jstr(spec.get("field")), jstr(spec.get("detail")))
    if type_ in ("TurnLeaseLost", "CompressionInProgress"):
        rep = repr_str(spec.get("session_id", ""))
        if type_ == "TurnLeaseLost":
            msg = "Session turn lease lost; refusing transcript write for %s" % rep
        else:
            msg = "Session %s is being compressed by another writer" % rep
        return mkraise(type_, msg)
    return mkraise(type_, spec.get("message", ""))


def keyerror(key):
    return mkraise("KeyError", "'%s'" % (key or ""))


def attr(obj, name):
    if obj is None:
        return mkraise("AttributeError", "'NoneType' object has no attribute '%s'" % name)
    if not isinstance(obj, dict) or name not in obj:
        return mkraise("AttributeError", "'types.SimpleNamespace' object has no attribute '%s'" % name)
    return obj[name]


def sorted_strs(arr):
    return sorted([s for s in arr if isinstance(s, str)]) if isinstance(arr, list) else []


def set_has(arr, id_):
    return isinstance(arr, list) and id_ is not None and any(
        isinstance(x, str) and x == id_ for x in arr)


def set_add(arr, id_):
    if not isinstance(arr, list) or id_ is None or set_has(arr, id_):
        return
    pos = len(arr)
    for i, x in enumerate(arr):
        if isinstance(x, str) and x > id_:
            pos = i
            break
    arr.insert(pos, id_)


def actor_arg(actor):
    return {"subject": jget(actor, "subject") or "", "profile_id": jget(actor, "profile_id") or "",
            "capabilities": sorted_strs(jget(actor, "capabilities")),
            "transport_id": jget(actor, "transport_id") or ""}


def ref_arg(ref):
    return {"profile_id": jget(ref, "profile_id") or "", "session_id": jget(ref, "session_id") or ""}


def aid_of(st):
    return jget(st, "authority") or ""


def auth_of(st):
    return jget(jget(st, "authorities"), aid_of(st))


def sub(st, key):
    v = jget(st, key)
    return v if isinstance(v, dict) else None


def sub_ensure(st, key):
    v = sub(st, key)
    if v is None:
        v = {}
        st[key] = v
    return v


def set_ensure(st, key):
    v = jget(st, key)
    if not isinstance(v, list):
        v = []
        st[key] = v
    return v


def keyset_eq(params, req):
    if not isinstance(params, dict) or len(params) != len(req):
        return False
    return all(k in params for k in req)


def str_in(s, set_):
    return s is not None and s in set_


# ---- seam dispatcher (mirrors sct2_call / sct2_peek) ----
class MirrorIO:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def peek(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            return entry[idx]
        return entry

    def _entry_result(self, entry):
        if isinstance(entry, dict) and entry.get("__script__") is True:
            if isinstance(entry.get("raise"), dict):
                return make_exc(entry["raise"])
            ret = entry.get("ret")
            return copy(ret) if ret is not None else None
        return copy(entry) if entry is not None else None

    def call(self, name, args):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            res = mkraise("RuntimeError", "no script for " + name)
        elif isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                res = mkraise("RuntimeError", "script exhausted for " + name)
            else:
                self.cursors[name] = idx + 1
                res = self._entry_result(entry[idx])
        else:
            res = self._entry_result(entry)
        self.trace.append({"name": name, "args": args, "ret": copy(res)})
        return res


# ---- entry flows (literal transcriptions of the C) ----
def e_init(io, args, st):
    aid = args.get("authority", "")
    tid = args.get("transport", "")
    identity = jget(args, "identity")
    operator = jget(args, "operator")
    auth = jget(sub(st, "authorities"), aid)
    caps = ["session:read", "session:submit", "session:control", "session:approve", "session:respond"]
    if truth(identity):
        set_add(caps, "session:create")
    if has(identity, "capabilities"):
        ic = identity["capabilities"]
        if not is_arr(ic):
            return mkraise("TypeError", "argument of type 'NoneType' is not iterable")
        caps = list(ic)
    if "session:operator" in caps:
        caps.remove("session:operator")
    if is_true(operator):
        set_add(caps, "session:operator")
    user_id = jget(identity, "user_id")
    auth_inst = jget(auth, "instance_id")
    inst = identity["instance_id"] if has(identity, "instance_id") else auth_inst
    if not truth(user_id) or not json_eq(inst, auth_inst):
        caps = []
    sorted_caps = sorted_strs(caps)
    subject = io.call("principal_subject", {"identity": copy_or_null(identity)})
    if is_raise(subject):
        return subject
    pid = identity["profile_id"] if has(identity, "profile_id") else jget(auth, "profile_id")
    hexv = io.call("uuid4_hex", {})
    if is_raise(hexv):
        return hexv
    actor = {"subject": subject, "profile_id": copy_or_null(pid),
             "capabilities": sorted_caps, "transport_id": copy(hexv)}
    bind = io.call("bind_native_transport", {"authority": aid, "actor": copy(actor),
                                             "identity": copy_or_null(identity)})
    if is_raise(bind):
        return bind
    native_owner = is_true(jget(identity, "native_bootstrap"))
    events = sub_ensure(st, "events")
    events[jstr(hexv) or ""] = tid
    return {"authority": aid, "transport": tid, "actor": actor, "native_owner": native_owner,
            "identity": copy_or_null(identity), "operator": copy(operator) if operator is not None else False,
            "subscriptions": {}, "replay_cursors": {}, "reconnects": [], "handed_back": [], "siblings": {}}


def e_host_scoped(io, args, st):
    identity = jget(st, "identity")
    return str_eq(jget(identity, "profile_scope"), "host")


def e_voice_stop_key(io, args, st):
    params = jget(args, "params")
    ref = jget(args, "ref")
    rid = jget(params, "submission_id")
    if not truth(rid):
        rid = jget(params, "input_id")
    if not is_str(rid) or not truth(rid):
        return None
    actor = jget(st, "actor")
    return [jget(actor, "subject") or "", jget(ref, "profile_id") or "",
            jget(ref, "session_id") or "", copy(rid)]


def e_resume_transcript(io, args, st):
    ref = jget(args, "ref")
    snapshot = jget(args, "snapshot")
    history = jget(args, "history")
    omit = jget(args, "omit")
    if not truth(omit):
        todo = io.call("_todo_state_from_history", {"history": copy_or_null(history)})
        if is_raise(todo):
            return todo
        return {"messages": copy_or_null(history),
                "message_count": len(history) if is_arr(history) else 0, "todo_state": todo}
    pair = io.call("authority.unless_deleted",
                   {"ref": ref_arg(ref), "transcript_target": copy_or_null(jget(snapshot, "transcript_target"))})
    if is_raise(pair):
        return pair
    if not is_arr(pair) or len(pair) != 2:
        got = len(pair) if is_arr(pair) else 0
        if not is_arr(pair):
            msg = "cannot unpack non-iterable NoneType object"
            return mkraise("TypeError", msg)
        elif got < 2:
            msg = "not enough values to unpack (expected 2, got %d)" % got
        else:
            msg = "too many values to unpack (expected 2)"
        return mkraise("ValueError", msg)
    return {"messages": [], "messages_omitted": True, "message_count": copy(pair[1]),
            "todo_state": copy(pair[0])}


def e_create(io, args, st):
    params = jget(args, "params")
    p = copy(params) if is_obj(params) else {}
    title = None
    if has(p, "title"):
        raw = p.pop("title")
        title = io.call("validate_title", {"title": raw})
        if is_raise(title):
            return title
    if title is None:
        return io.call("self._create", {"params": p, "title": None})
    auth = auth_of(st)
    if auth is not None and not has(auth, "_titled_create_lock"):
        auth["_titled_create_lock"] = True
    return io.call("self._create", {"params": p, "title": title})


def e_describe(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    pong = io.call("self.ping", {"ref": ref_arg(ref), "params": copy_or_null(params)})
    if is_raise(pong):
        return pong
    skin = io.call("resolved_skin", {})
    if is_raise(skin):
        return skin
    auth = auth_of(st)
    CAPS = ["durable-admission-v1", "event-replay-v1", "local-cli-create-v1", "acp-editor-policy-v1",
            "acp-session-mcp-v1", "clarify-batch-v1", "plugin-rpc-v1", "session-client-v1",
            "session-transcript-v1", "session-list-picker-v1", "session-list-cursor-v1"]
    SOURCES = ["acp", "cli", "gui", "oneshot", "tool", "tui"]
    PARAMETERS = ["accept_hooks", "api_key", "base_url", "checkpoints", "client", "client_env",
                  "cwd", "cwd_explicit", "editor", "follow_profile_config", "ignore_rules",
                  "ignore_user_config", "max_turns", "model", "pass_session_id", "provider",
                  "reasoning", "request_id", "row_cwd", "run_budget", "safe_mode", "service_tier",
                  "skills", "source", "title", "toolsets", "verbose", "yolo"]
    return {"instance_id": copy_or_null(jget(auth, "instance_id")),
            "profile_id": copy_or_null(jget(auth, "profile_id")),
            "authority_epoch": copy_or_null(jget(auth, "epoch")),
            "capabilities": list(CAPS),
            "session_create": {"sources": list(SOURCES), "parameters": list(PARAMETERS)},
            "skin": skin}


def e_info(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    if not keyset_eq(params, ["session_id"]):
        return refusal("invalid_params", None, None)
    r = io.call("authority.resolve", {"actor": actor_arg(jget(st, "actor")), "ref": ref_arg(ref)})
    if is_raise(r):
        return r
    return io.call("snapshot_info", {"authority": aid_of(st), "ref": ref_arg(ref)})


def e_list_sessions(io, args, st):
    params = jget(args, "params")
    actor = jget(st, "actor")
    caps = jget(actor, "capabilities")
    if not set_has(caps, "session:read"):
        return refusal("permission_denied", None, None)
    limitv = params["limit"] if has(params, "limit") else None
    limit = 200
    if limitv is None:
        limit_ok = True
    elif is_int_strict(limitv):
        limit = limitv
        limit_ok = True
    else:
        limit_ok = False
    workspace = jget(params, "workspace")
    pickerv = params["picker"] if has(params, "picker") else None
    picker = is_true(pickerv)
    picker_ok = pickerv is None or is_bool(pickerv)
    ALLOWED = ["limit", "title", "include_hidden", "scope", "workspace", "picker", "query", "cursor"]
    if is_obj(params):
        bad_keys = any(k not in ALLOWED for k in params)
    else:
        bad_keys = True
    ws_ok = workspace is None or (is_str(workspace) and truth(workspace))
    if bad_keys or not limit_ok or not (1 <= limit <= 200) or not ws_ok or not picker_ok:
        return refusal("invalid_params", None, None)
    scopev = params["scope"] if has(params, "scope") else None
    scope_ok = True if scopev is None else (str_eq(scopev, "live") or str_eq(scopev, "stored"))
    ws_not_none = has(params, "workspace") and workspace is not None
    if not scope_ok or (has(params, "title") and (has(params, "scope") or ws_not_none or picker)):
        return refusal("invalid_params", None, None)
    cp = io.call("check_page_params", {"params": copy_or_null(params)})
    if is_raise(cp):
        return cp
    aid = aid_of(st)
    if str_eq(scopev, "stored"):
        return io.call("stored_picker_sessions",
                       {"authority": aid, "actor": actor_arg(actor), "limit": limit,
                        "include_hidden": truth(jget(params, "include_hidden")),
                        "workspace": copy_or_null(workspace), "picker": picker,
                        "query": copy_or_null(jget(params, "query")),
                        "cursor": copy_or_null(jget(params, "cursor"))})
    if has(params, "title"):
        found = io.call("resolve_titled_session",
                        {"authority": aid, "actor": actor_arg(actor),
                         "title": copy(params["title"]), "missing_ok": True})
        if is_raise(found):
            return found
        if found is None:
            return {"sessions": [], "scope": "stored"}
        fsidv = attr(found, "session_id")
        if is_raise(fsidv):
            return fsidv
        fsid = jstr(fsidv) or ""
        row = io.call("store.sessions.get", {"session_id": fsid})
        if is_raise(row):
            return row
        rt = io.call("store.sessions.resolve_by_title", {"title": copy(params["title"])})
        if is_raise(rt):
            return rt
        ga2 = {"session_id": copy(rt) if truth(rt) else fsid}
        root0 = io.call("store.sessions.get", ga2)
        if is_raise(root0):
            return root0
        if truth(root0):
            root = root0
        else:
            root = row
        sess = {"session_id": fsid}
        rid = None if root is None else attr(root, "session_id")
        if is_raise(rid):
            return rid
        if not truth(rid):
            rid = fsidv
        sess["id"] = copy(rid)
        sess["resolved_id"] = fsid
        rtitle = None if row is None else attr(row, "title")
        if is_raise(rtitle):
            return rtitle
        sess["title"] = copy(rtitle) if truth(rtitle) else ""
        rotitle = None if root is None else attr(root, "title")
        if is_raise(rotitle):
            return rotitle
        sess["root_title"] = copy(rotitle) if truth(rotitle) else ""
        rsource = None if row is None else attr(row, "source")
        if is_raise(rsource):
            return rsource
        sess["source"] = copy_or_null(rsource)
        rstarted = None if row is None else attr(row, "started_at")
        if is_raise(rstarted):
            return rstarted
        sess["started_at"] = copy_or_null(rstarted)
        rmc = None if row is None else attr(row, "message_count")
        if is_raise(rmc):
            return rmc
        sess["message_count"] = copy(rmc) if rmc is not None else 0
        sess["running"] = has(sub(st, "sessions"), fsid)
        return {"sessions": [sess], "scope": "stored"}
    entries = io.call("live_entries", {"authority": aid, "actor": actor_arg(actor)})
    if is_raise(entries):
        return entries
    INTERNAL = ["kanban", "tool", "oneshot"]
    include_hidden = truth(jget(params, "include_hidden"))
    sessions = []
    if is_arr(entries):
        for entry in entries:
            esid = attr(entry, "session_id")
            if is_raise(esid):
                return esid
            row = attr(entry, "row")
            if is_raise(row):
                return row
            closed = attr(entry, "closed")
            if is_raise(closed):
                return closed
            source = jget(row, "source")
            if str_in(jstr(source), INTERNAL) or truth(closed) or (
                    not include_hidden and (truth(jget(row, "hidden")) or truth(jget(row, "archived")))):
                continue
            handle = attr(entry, "handle")
            if is_raise(handle):
                return handle
            estate = attr(handle, "execution_state")
            if is_raise(estate):
                return estate
            title = jget(row, "title")
            mc = jget(row, "message_count")
            sessions.append({"session_id": copy(esid), "id": copy(esid),
                             "title": copy(title) if truth(title) else "",
                             "source": copy_or_null(source),
                             "started_at": copy_or_null(jget(row, "started_at")),
                             "message_count": copy(mc) if mc is not None else 0,
                             "model": copy_or_null(jget(row, "model")),
                             "running": str_eq(estate, "running")})

    def keyf(s):
        sa = s.get("started_at")
        return sa if (isinstance(sa, (int, float)) and not isinstance(sa, bool) and truth(sa)) else 0
    sessions.sort(key=keyf, reverse=True)   # Python sort is stable, matches C insertion sort
    page = io.call("live_picker_page", {"authority": aid, "sessions": sessions, "limit": limit,
                                        "workspace": copy_or_null(workspace), "picker": picker})
    if is_raise(page):
        return page
    return {"sessions": page, "scope": "live"}


def e_submit(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    replay = io.call("self._voice_stop_replay", {"ref": ref_arg(ref), "params": copy_or_null(params)})
    if is_raise(replay):
        return replay
    stop = truth(replay)
    if not stop:
        typed = io.call("self._typed_voice_stop", {"ref": ref_arg(ref), "params": copy_or_null(params)})
        if is_raise(typed):
            return typed
        stop = truth(typed)
    if stop:
        rec = io.call("self._record_voice_stop", {"ref": ref_arg(ref), "params": copy_or_null(params)})
        if is_raise(rec):
            return rec
        return {"voice_stopped": True}
    submission = io.call("self.submission", {"ref": ref_arg(ref), "params": copy_or_null(params)})
    if is_raise(submission):
        return submission
    qwr = is_true(jget(st, "operator"))
    receipt = io.call("authority.submit", {"actor": actor_arg(jget(st, "actor")),
                                           "submission": submission, "queue_while_restarting": qwr})
    if is_raise(receipt):
        return receipt
    return receipt


def e_mutate(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    actor = jget(st, "actor")
    aid = aid_of(st)
    r = io.call("mutate_session", {"authority": aid, "actor": actor_arg(actor),
                                   "ref": ref_arg(ref), "params": copy_or_null(params)})
    if is_raise(r):
        k = kind(r)
        if caught_by(k, ["TurnLeaseLost", "CompressionInProgress"]):
            compression = caught_by(k, ["CompressionInProgress"])
            return refusal("session_busy", "compression" if compression else None, None)
        if caught_by(k, ["AuthorityRefusal"]):
            reason = r.get("reason", "")
            if reason in ("revision_conflict", "stale_generation"):
                row = io.call("store.sessions.get", {"session_id": jget(ref, "session_id") or ""})
                if is_raise(row):
                    return row
                if row is not None:
                    rev = attr(row, "runtime_revision")
                    if is_raise(rev):
                        return rev
                    gen = attr(row, "runtime_generation")
                    if is_raise(gen):
                        return gen
                    r["revision"] = copy(rev)
                    r["execution_generation"] = copy(gen)
            return r
        return r
    child = jget(r, "branched_session_id") if is_obj(r) else None
    if truth(child):
        csid = jstr(child)
        subscribed = csid is not None and has(sub(st, "subscriptions"), csid)
        if not subscribed:
            rr = io.call("self.resume", {"ref": {"profile_id": jget(ref, "profile_id") or "",
                                                 "session_id": copy(child)}, "params": {}})
            if is_raise(rr):
                return rr
    return r


def e_receipt(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    admid = jget(params, "admission_id")
    rc = io.call("authority.receipt", {"actor": actor_arg(jget(st, "actor")), "ref": ref_arg(ref),
                                       "admission_id": copy_or_null(admid)})
    if is_raise(rc):
        return rc
    if not is_obj(rc):
        return mkraise("TypeError", "asdict() should be called on dataclass instances")
    out = copy(rc)
    if is_true(jget(params, "include_result")) and str_eq(jget(rc, "status"), "terminal"):
        saved = io.call("admission_result", {"admission_id": copy_or_null(jget(rc, "admission_id"))})
        if is_raise(saved):
            return saved
        if saved is not None:
            if not is_obj(saved) or not has(saved, "result"):
                return keyerror("result")
            out["result"] = copy(saved["result"])
            usage = jget(saved, "usage")
            out["usage"] = copy(usage) if truth(usage) else {}
    return out


def e_resolve_unknown(io, args, st):
    ref = jget(args, "ref")
    params = jget(args, "params")
    if not keyset_eq(params, ["session_id", "admission_id", "execution_generation"]):
        return refusal("invalid_params", None, None)
    return io.call("authority.resolve_unknown",
                   {"actor": actor_arg(jget(st, "actor")), "ref": ref_arg(ref),
                    "admission_id": copy(params["admission_id"]),
                    "execution_generation": copy(params["execution_generation"])})


def e_do_respond(io, args, st, clarify):
    ref = jget(args, "ref")
    params = jget(args, "params")
    req = ["session_id", "execution_generation", "prompt_id", "answer" if clarify else "choice"]
    if not keyset_eq(params, req):
        return refusal("invalid_params", None, None)
    payload = {("answer" if clarify else "choice"): copy(params["answer" if clarify else "choice"])}
    ra = {"actor": actor_arg(jget(st, "actor")), "ref": ref_arg(ref),
          "execution_generation": copy(params["execution_generation"]),
          "prompt_id": copy(params["prompt_id"]), "payload": payload,
          "kind": "clarify" if clarify else None}
    return io.call("authority.respond", ra)


def e_respond(io, args, st):
    return e_do_respond(io, args, st, False)


def e_respond_clarify(io, args, st):
    return e_do_respond(io, args, st, True)


def e_entitled(io, st, ask):
    sid = ask.get("session_id", "")
    events = jget(ask, "events")
    r = io.call("may_settle", {"authority": aid_of(st), "actor": actor_arg(jget(st, "actor")),
                               "session_id": sid, "events": copy_or_null(events)})
    if is_raise(r):
        return r, False
    if truth(r):
        return None, True
    siblings = jget(st, "siblings")
    if is_obj(siblings):
        for sib in siblings.values():
            sr = io.call("may_settle", {"authority": jget(sib, "authority") or "",
                                        "actor": actor_arg(jget(sib, "actor")),
                                        "session_id": sid, "events": copy_or_null(events)})
            if is_raise(sr):
                return sr, False
            if truth(sr):
                return None, True
    return None, False


def e_resolve_response(io, args, st):
    frame = jget(args, "frame")
    entry = io.peek("resolve")
    asks = jget(entry, "asks")
    if is_arr(asks):
        for ask in asks:
            err, ok = e_entitled(io, st, ask)
            if err is not None:
                return err
    r = io.call("resolve", {"frame": copy_or_null(frame)})
    if is_raise(r):
        return r
    return None


def e_close(io, args, st):
    aid = aid_of(st)
    actor = jget(st, "actor")
    siblings = jget(st, "siblings")
    if is_obj(siblings):
        for sib in list(siblings.values()):
            r = io.call("sibling.close", {"authority": jget(sib, "authority") or ""})
            if is_raise(r):
                return r
    st["siblings"] = {}
    fr = io.call("forget", {})
    if is_raise(fr):
        return fr
    subs = jget(st, "subscriptions")
    if is_obj(subs):
        for sid, sub_ in list(subs.items()):
            live = jget(sub(st, "sessions"), sid)
            if live is None:
                continue
            subscribers = attr(live, "subscribers")
            if is_raise(subscribers):
                return subscribers
            if not set_has(subscribers, jstr(sub_)):
                continue
            dr = io.call("authority.detach", {"actor": actor_arg(actor),
                                              "subscription_id": copy(sub_), "orphaned": True})
            if is_raise(dr):
                return dr
    hb = jget(st, "handed_back")
    if is_arr(hb):
        for sid in hb:
            if not isinstance(sid, str):
                continue
            ar = io.call("arm_detached_orphan_close", {"authority": aid, "session_id": sid})
            if is_raise(ar):
                return ar
    ur = io.call("unbind_native_transport", {"authority": aid, "actor": actor_arg(actor)})
    if is_raise(ur):
        return ur
    st["subscriptions"] = {}
    events = jget(st, "events")
    if is_obj(events):
        events.pop(jget(actor, "transport_id") or "", None)
    return None


def e_spawn_notice(io, args, st):
    ref = jget(args, "ref")
    task = io.call("create_task", {"authority": aid_of(st), "ref": ref_arg(ref)})
    if is_raise(task):
        return task
    tasks = set_ensure(st, "notice_tasks")
    tid = jstr(task)
    if tid is not None:
        set_add(tasks, tid)
    return None


ENTRIES = {
    "__init__": e_init, "host_scoped": e_host_scoped, "_voice_stop_key": e_voice_stop_key,
    "_resume_transcript": e_resume_transcript, "create": e_create, "describe": e_describe,
    "info": e_info, "list_sessions": e_list_sessions, "submit": e_submit, "mutate": e_mutate,
    "receipt": e_receipt, "resolve_unknown": e_resolve_unknown, "respond": e_respond,
    "respond_clarify": e_respond_clarify, "_resolve_response": e_resolve_response,
    "close": e_close, "_spawn_notice": e_spawn_notice,
}


def mirror_case(func, inp):
    io = MirrorIO(inp.get("io"))
    st = copy(inp.get("state") or {})
    ret = ENTRIES[func](io, inp.get("args") or {}, st)
    return {"ret": ret, "trace": io.trace, "state": st}


def norm(v):
    if isinstance(v, bool):
        return v
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, list):
        return [norm(x) for x in v]
    return v


def load_cases():
    return [json.loads(l) for l in FIX.read_text(encoding="utf-8").splitlines() if l.strip()]


def do_diff():
    import importlib.util
    spec = importlib.util.spec_from_file_location("sct2_oracle", str(ORACLE))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    cases = load_cases()
    mism = 0
    for i, c in enumerate(cases):
        func, inp = c["func"], c["in"]
        mir = norm(mirror_case(func, inp))
        live = norm(m.drive(func, inp))
        if mir != live:
            mism += 1
            if mism <= 15:
                print("SHADOW MISMATCH [%s] case %d" % (func, i + 1))
                for k in ("ret", "trace", "state"):
                    if mir.get(k) != live.get(k):
                        print("  %s MIR : %s" % (k, json.dumps(mir.get(k), ensure_ascii=False)[:600]))
                        print("  %s LIVE: %s" % (k, json.dumps(live.get(k), ensure_ascii=False)[:600]))
    print("SHADOW diff: %d cases, %d mismatches" % (len(cases), mism))
    return mism


def do_proxy():
    cases = load_cases()
    lines = []
    for c in cases:
        env = mirror_case(c["func"], c["in"])
        lines.append(json.dumps({"func": c["func"], "in": c["in"], "out": env}, ensure_ascii=False))
    payload = "\n".join(lines) + "\n"
    p = subprocess.run([PYBIN, str(ORACLE)], input=payload, capture_output=True, text=True, encoding="utf-8")
    print(p.stdout.strip())
    if p.stderr.strip():
        print("STDERR:", p.stderr.strip()[:500])
    print("proxy exit:", p.returncode)
    return p.returncode


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "both"
    rc = 0
    if mode in ("diff", "both"):
        rc |= (1 if do_diff() else 0)
    if mode in ("proxy", "both"):
        rc |= do_proxy()
    sys.exit(rc)