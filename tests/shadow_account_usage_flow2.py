#!/usr/bin/env python3
"""Literal Python shadow-mirror of src/agent/account_usage_flow2.c (au2_*).

Code-only verification (no C compile/link): transcribes the C helper algorithms
and entry flows FAITHFULLY (presence semantics via a MISSING sentinel, the same
exception-parent table, the same key encoding / state model / seam dispatcher),
then:
  diff  -- mirror {ret,trace,st} vs the oracle's LIVE drive over every fixture.
  proxy -- serialize mirror envelopes to JSONL and pipe into the oracle's REAL
           main() (subprocess stdin); expect 'ORACLE ...: N cases, 0 mismatches'.
Run: python tests/shadow_account_usage_flow2.py [diff|proxy|both]
"""
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIX = HERE / "fixtures" / "account_usage_flow2.jsonl"
ORACLE = HERE / "oracle_account_usage_flow2.py"
PYBIN = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"

MISSING = object()

# ---- exception-parent table (verbatim from the C) ----
EXC_PARENT = {
    "BaseException": "object", "BaseExceptionGroup": "BaseException", "Exception": "BaseException",
    "GeneratorExit": "BaseException", "KeyboardInterrupt": "BaseException", "SystemExit": "BaseException",
    "CancelledError": "BaseException", "ArithmeticError": "Exception", "AssertionError": "Exception",
    "AttributeError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError", "ImportError": "Exception",
    "LookupError": "Exception", "MemoryError": "Exception", "NameError": "Exception", "OSError": "Exception",
    "ReferenceError": "Exception", "RuntimeError": "Exception", "StopAsyncIteration": "Exception",
    "StopIteration": "Exception", "SyntaxError": "Exception", "SystemError": "Exception",
    "TypeError": "Exception", "ValueError": "Exception", "Warning": "Exception", "WindowsError": "OSError",
    "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "ModuleNotFoundError": "ImportError", "IndexError": "LookupError", "KeyError": "LookupError",
    "UnboundLocalError": "NameError", "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "FileExistsError": "OSError", "FileNotFoundError": "OSError",
    "InterruptedError": "OSError", "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError", "TimeoutError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "NotImplementedError": "RuntimeError", "PythonFinalizationError": "RuntimeError", "RecursionError": "RuntimeError",
    "IndentationError": "SyntaxError", "_IncompleteInputError": "SyntaxError", "TabError": "IndentationError",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "BytesWarning": "Warning", "DeprecationWarning": "Warning",
    "EncodingWarning": "Warning", "FutureWarning": "Warning", "ImportWarning": "Warning",
    "PendingDeprecationWarning": "Warning", "ResourceWarning": "Warning", "RuntimeWarning": "Warning",
    "SyntaxWarning": "Warning", "UnicodeWarning": "Warning", "UserWarning": "Warning",
}


def is_exception(kind):
    if kind == "ExceptionGroup":
        return True
    cur = kind
    for _ in range(32):
        if cur is None:
            return True
        if cur == "Exception":
            return True
        if cur in ("BaseException", "object"):
            return False
        cur = EXC_PARENT.get(cur)
        if cur is None:
            return True
    return True


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
        if cur is None:
            return False
    return False


# ---- value helpers ----
def present(v):
    return v is not None and v is not MISSING


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


def jget(o, k):
    return o[k] if isinstance(o, dict) and k in o else None


def jstr(v):
    return v if isinstance(v, str) else None


def jnum(v):
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    return 0.0


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def kind(r):
    return r.get("__raise__", "") if isinstance(r, dict) else ""


def mkraise(k, m):
    return {"__raise__": k, "message": m}


def type_name(v):
    if v is None or v is MISSING:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "int" if (math.isfinite(v) and v == math.floor(v)) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    return "dict"


def attr_raise(obj, attr):
    return mkraise("AttributeError", "'%s' object has no attribute '%s'" % (type_name(obj), attr))


WS = set(" \t\n\r\v\f") | {chr(c) for c in range(0x1c, 0x20)}


def strip(s):
    if s is None:
        s = ""
    a, b = 0, len(s)
    while a < b and s[a] in WS:
        a += 1
    while b > a and s[b - 1] in WS:
        b -= 1
    return s[a:b]


def lower(s):
    if s is None:
        s = ""
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)


def rstrip_slash(s):
    if s is None:
        s = ""
    return s.rstrip("/")


def pystr(v):
    if v is None or v is MISSING:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        d = float(v)
        if d == 0.0:
            return "0"
        if math.isfinite(d) and d == math.floor(d) and abs(d) < 1e16:
            return "%.0f" % d
        return "%.17g" % d
    if isinstance(v, str):
        return v
    return json.dumps(v, ensure_ascii=False, separators=(",", ":"))


def pystr_or_empty(v):
    if not truth(v):
        return ""
    return pystr(v)


def slug(provider):
    return lower(strip(pystr_or_empty(provider)))


def copy(v):
    return json.loads(json.dumps(v)) if isinstance(v, (dict, list)) else v


def copy_or_null(v):
    return copy(v) if v is not None and v is not MISSING else None


# ---- strip nulls (harness trace args) ----
def strip_nulls(v):
    if v is None or v is MISSING:
        return None
    if isinstance(v, list):
        return [strip_nulls(x) for x in v]
    if isinstance(v, dict):
        return {k: strip_nulls(x) for k, x in v.items() if x is not None}
    return v


# ---- seam dispatcher (mirrors au2_h_dispatch) ----
class MirrorIO:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        entry = self.io.get(name, MISSING)
        present_key = entry is not MISSING
        exhausted = False
        result = None
        if present_key:
            if isinstance(entry, list):
                idx = self.cursors.get(name, 0)
                if idx >= len(entry):
                    exhausted = True
                else:
                    result = entry[idx]
                    self.cursors[name] = idx + 1
            else:
                result = entry
        failclosed = (not present_key) or exhausted
        actual = result
        if not failclosed and isinstance(actual, dict) and actual.get("__script__") is True:
            actual = actual.get("ret")
        if failclosed:
            retnode = mkraise("RuntimeError", "no script for " + name)
        else:
            retnode = copy(actual) if actual is not None else None
        self.trace.append({"ev": name, "args": strip_nulls(args), "ret": copy(retnode)})
        if failclosed:
            return retnode
        if actual is None:
            return None
        return retnode


def call(io, name, args):
    if io is None:
        return mkraise("RuntimeError", "no seam installed for " + name)
    return io.call(name, args)


# ---- state model ----
def keystr(keyarr):
    parts = []
    for i in range(4):
        e = keyarr[i] if isinstance(keyarr, list) and i < len(keyarr) else None
        parts.append(pystr(e))
    return "".join(f"{len(s)}:{s}" for s in parts)


class State:
    def __init__(self, inp):
        st = jget(inp, "state")
        sn = jget(st, "snapshots")
        sta = jget(st, "started")
        lt = jget(st, "last_try")
        inf = jget(st, "inflight")
        self.snapshots = copy(sn) if isinstance(sn, dict) else {}
        self.started = copy(sta) if isinstance(sta, dict) else {}
        self.last_try = copy(lt) if isinstance(lt, dict) else {}
        self.inflight = copy(inf) if isinstance(inf, list) else []

    def emit(self, st):
        st["snapshots"] = copy(self.snapshots)
        st["started"] = copy(self.started)
        st["last_try"] = copy(self.last_try)
        st["inflight"] = copy(self.inflight)

    def contains(self, ks):
        return ks in self.inflight

    def add(self, ks):
        if ks in self.inflight:
            return
        self.inflight.append(ks)
        self.inflight.sort()

    def discard(self, ks):
        self.inflight = [x for x in self.inflight if x != ks]

    def claim(self, ks, now):
        if self.contains(ks):
            return 0
        last = self.last_try.get(ks, MISSING)
        lastv = jnum(last) if last is not MISSING else -math.inf
        if now - lastv < 120.0:
            return 0
        self.add(ks)
        self.last_try[ks] = now
        return 1

    def release(self, ks):
        self.discard(ks)


def key_core(io, provider, identity_id, base_url):
    home = call(io, "hermes_home_key", {})
    if is_raise(home):
        return home
    s = slug(provider)
    ident = strip(pystr_or_empty(identity_id))
    base = rstrip_slash(strip(pystr_or_empty(base_url)))
    return [home, s, ident, base]


# ---- entries ----
def e_plural(io, inp, st):
    count = jget(inp, "count")
    eq1 = False
    if isinstance(count, bool):
        eq1 = count
    elif isinstance(count, (int, float)):
        eq1 = count == 1.0
    return "" if eq1 else "s"


def e_get_json(io, inp, st):
    url = jget(inp, "url")
    headers = jget(inp, "headers")
    timeout = jnum(jget(inp, "timeout"))
    cm = call(io, "httpx.Client", {"timeout": timeout})
    if is_raise(cm):
        return cm
    if not present(cm):
        return attr_raise(None, "__enter__")
    if not isinstance(cm, dict):
        return attr_raise(cm, "__enter__")
    client = call(io, "client.__enter__", {"client": copy(cm)})
    if is_raise(client):
        return client
    pending = None
    response = None
    response_bound = False
    if not present(client):
        pending = attr_raise(None, "get")
    elif not isinstance(client, dict):
        pending = attr_raise(client, "get")
    else:
        response = call(io, "client.get", {"client": copy(client), "url": copy_or_null(url),
                                           "headers": copy_or_null(headers)})
        if is_raise(response):
            pending = response
            response = None
        else:
            response_bound = True
    if pending is None:
        if not present(response):
            pending = attr_raise(None, "raise_for_status")
        elif not isinstance(response, dict):
            pending = attr_raise(response, "raise_for_status")
        else:
            rr = call(io, "response.raise_for_status", {"response": copy(response)})
            if is_raise(rr):
                pending = rr
    xr = call(io, "client.__exit__", {"client": copy(cm), "exc": copy(pending) if pending is not None else None})
    if is_raise(xr):
        return xr
    suppress = truth(xr)
    if pending is not None and not suppress:
        return pending
    pending = None
    if not response_bound:
        return mkraise("UnboundLocalError",
                       "cannot access local variable 'response' where it is not associated with a value")
    j = call(io, "response.json", {"response": copy(response)})
    if is_raise(j):
        return j
    if truth(j):
        return j
    return {}


def e_data(io, inp, st):
    client = jget(inp, "client")
    normalized = jstr(jget(inp, "normalized")) or ""
    path = jstr(jget(inp, "path")) or ""
    headers = jget(inp, "headers")
    url = f"{normalized}/{path}"
    if not present(client):
        return attr_raise(None, "get")
    if not isinstance(client, dict):
        return attr_raise(client, "get")
    resp = call(io, "client.get", {"client": copy(client), "url": url, "headers": copy_or_null(headers)})
    if is_raise(resp):
        return resp
    if not present(resp):
        return attr_raise(None, "raise_for_status")
    if not isinstance(resp, dict):
        return attr_raise(resp, "raise_for_status")
    rr = call(io, "response.raise_for_status", {"response": copy(resp)})
    if is_raise(rr):
        return rr
    j = call(io, "response.json", {"response": copy(resp)})
    if is_raise(j):
        return j
    if not truth(j):
        return {}
    if not isinstance(j, dict):
        return attr_raise(j, "get")
    data = j.get("data", MISSING)
    if not truth(data):
        return {}
    return copy(data)


def e_fetch_portal_account(io, inp, st):
    timeout = jnum(jget(inp, "timeout"))
    stored = truth(jget(inp, "stored_token_only"))
    ctx = call(io, "contextvars.copy_context", {})
    if is_raise(ctx):
        return ctx
    pool = call(io, "DaemonThreadPoolExecutor", {"max_workers": 1})
    if is_raise(pool):
        return pool
    if stored:
        read = {"fn": "stored_token_account_info"}
    else:
        read = call(io, "partial", {"fn": "get_nous_portal_account_info", "force_fresh": True})
        if is_raise(read):
            return read
    if not present(ctx):
        return attr_raise(None, "run")
    if not isinstance(ctx, dict):
        return attr_raise(ctx, "run")
    if not present(pool):
        return attr_raise(None, "submit")
    if not isinstance(pool, dict):
        return attr_raise(pool, "submit")
    future = call(io, "pool.submit", {"context": copy(ctx), "read": copy_or_null(read)})
    if is_raise(future):
        return future
    pending = None
    ret = None
    if not present(future):
        pending = attr_raise(None, "result")
    elif not isinstance(future, dict):
        pending = attr_raise(future, "result")
    else:
        ret = call(io, "future.result", {"timeout": timeout})
        if is_raise(ret):
            pending = ret
            ret = None
    if pending is not None:
        if not present(future):
            cb = attr_raise(None, "add_done_callback")
        elif not isinstance(future, dict):
            cb = attr_raise(future, "add_done_callback")
        else:
            cb = call(io, "future.add_done_callback", {"callback": {"__callable__": "future.exception_drain"}})
        if is_raise(cb):
            pending = cb
    sd = call(io, "pool.shutdown", {"wait": False})
    if is_raise(sd):
        return sd
    if pending is not None:
        return pending
    return ret


def e_key(io, inp, st):
    return key_core(io, jget(inp, "provider"), jget(inp, "identity_id"), jget(inp, "base_url"))


def e_has_account_usage(io, inp, st):
    fetchers = call(io, "usage_fetchers", {})
    if is_raise(fetchers):
        return fetchers
    s = slug(jget(inp, "provider"))
    if not present(fetchers):
        return mkraise("TypeError", "argument of type 'NoneType' is not a container or iterable")
    member = False
    if isinstance(fetchers, dict):
        member = s in fetchers
    elif isinstance(fetchers, list):
        member = s in fetchers
    elif isinstance(fetchers, str):
        member = s in fetchers
    if member:
        return True
    if not s:
        return False
    profile = call(io, "get_provider_profile", {"slug": s})
    if is_raise(profile):
        return profile
    if not present(profile):
        return False
    hook = jstr(jget(profile, "hook"))
    return hook == "custom"


def e_identity_id_for(io, inp, st):
    provider = jget(inp, "provider")
    entry = jget(inp, "entry")
    token = strip(pystr_or_empty(jget(entry, "runtime_api_key")))
    if not token:
        idn = entry.get("id", MISSING) if isinstance(entry, dict) else MISSING
        ids = pystr(idn) if idn is not MISSING else ""
        return "entry:" + ids
    prov = jstr(provider)
    if prov == "openai-codex":
        principal = call(io, "codex_principal_identity", {"token": token})
        if is_raise(principal):
            return principal
        if truth(principal) and isinstance(principal, list):
            if len(principal) < 2:
                return mkraise("IndexError", "list index out of range")
            return "codex:%s:%s" % (pystr(principal[0]), pystr(principal[1]))
    hexv = call(io, "sha256_hexdigest", {"text": token})
    if is_raise(hexv):
        return hexv
    hs = jstr(hexv) or ""
    return "fp:" + hs[:16]


def e_remember(io, inp, st):
    state = State(inp)
    provider = jget(inp, "provider")
    snapshot = jget(inp, "snapshot")
    identity_id = jget(inp, "identity_id")
    base_url = jget(inp, "base_url")
    started_monotonic = jget(inp, "started_monotonic")
    if not truth(provider) or not present(snapshot) or not truth(jget(snapshot, "windows")):
        state.emit(st)
        return None
    if present(started_monotonic):
        started = jnum(started_monotonic)
    else:
        sm = call(io, "time.monotonic", {})
        if is_raise(sm):
            state.emit(st)
            return sm
        started = jnum(sm)
    snapshot_identity = jget(snapshot, "identity")
    targets = []
    err = None
    if not present(identity_id):
        k1 = key_core(io, provider, None, base_url)
        if is_raise(k1):
            err = k1
        else:
            targets.append(keystr(k1))
        if err is None and truth(snapshot_identity):
            k2 = key_core(io, provider, snapshot_identity, None)
            if is_raise(k2):
                err = k2
            else:
                targets.append(keystr(k2))
    else:
        id_used = snapshot_identity if truth(snapshot_identity) else identity_id
        k1 = key_core(io, provider, id_used, None)
        if is_raise(k1):
            err = k1
        else:
            targets.append(keystr(k1))
    if err is not None:
        state.emit(st)
        return err
    for ks in targets:
        prev = state.started.get(ks, MISSING)
        if prev is not MISSING and jnum(prev) > started:
            continue
        state.snapshots[ks] = copy(snapshot)
        state.started[ks] = started
    state.emit(st)
    return None


def e_cached(io, inp, st):
    state = State(inp)
    k = key_core(io, jget(inp, "provider"), jget(inp, "identity_id"), jget(inp, "base_url"))
    if is_raise(k):
        state.emit(st)
        return k
    ks = keystr(k)
    found = state.snapshots.get(ks, MISSING)
    ret = copy(found) if found is not MISSING else None
    state.emit(st)
    return ret


def e_snapshot_is_stale(io, inp, st):
    snapshot = jget(inp, "snapshot")
    if not isinstance(snapshot, dict):
        return True
    now = call(io, "time.time", {})
    if is_raise(now):
        return now
    nowv = jnum(now)
    fa = jget(snapshot, "fetched_at")
    if isinstance(fa, (int, float)) and not isinstance(fa, bool):
        ts = fa / 1e6
    elif is_raise(fa):
        if caught_by(kind(fa), ("AttributeError", "TypeError", "ValueError", "OSError", "OverflowError")):
            return True
        return copy(fa)
    else:
        return True
    age = nowv - ts
    return age < 0.0 or age > 900.0


def e_claim(io, inp, st):
    state = State(inp)
    ks = keystr(jget(inp, "key"))
    now = jnum(jget(inp, "now"))
    claimed = state.claim(ks, now)
    state.emit(st)
    return bool(claimed)


def e_release(io, inp, st):
    state = State(inp)
    ks = keystr(jget(inp, "key"))
    state.release(ks)
    state.emit(st)
    return None


def e_refresh_async(io, inp, st):
    state = State(inp)
    now = call(io, "time.monotonic", {})
    if is_raise(now):
        state.emit(st)
        return now
    nowv = jnum(now)
    providers = jget(inp, "providers")
    started = []
    seen = set()
    if isinstance(providers, list):
        for pn in providers:
            ps = jstr(pn)
            if ps is None or ps in seen:
                continue
            seen.add(ps)
            key = key_core(io, pn, None, None)
            if is_raise(key):
                state.emit(st)
                return key
            ks = keystr(key)
            claimed = state.claim(ks, nowv)
            if not claimed:
                continue
            ctx = call(io, "contextvars.copy_context", {})
            if is_raise(ctx):
                state.emit(st)
                return ctx
            if not present(ctx):
                state.emit(st)
                return attr_raise(None, "run")
            thread = call(io, "threading.Thread", {"target": "context.run", "fn": "_refresh_legacy",
                                                   "provider": ps, "key": copy(key),
                                                   "name": "hermes-account-usage-refresh", "daemon": True})
            if is_raise(thread):
                state.emit(st)
                return thread
            if not present(thread):
                state.emit(st)
                return attr_raise(None, "start")
            sr = call(io, "thread.start", {"thread": copy(thread)})
            if is_raise(sr):
                state.emit(st)
                return sr
            started.append({"fn": "_refresh_legacy", "provider": ps, "key": copy(key)})
    state.emit(st)
    return started


def e_refresh_legacy(io, inp, st):
    state = State(inp)
    provider = jget(inp, "provider")
    ks = keystr(jget(inp, "key"))
    r = call(io, "fetch_account_usage", {"provider": copy_or_null(provider)})
    pending = r if is_raise(r) else None
    state.release(ks)
    state.emit(st)
    return pending


def e_refresh_entries_async(io, inp, st):
    state = State(inp)
    now = call(io, "time.monotonic", {})
    if is_raise(now):
        state.emit(st)
        return now
    nowv = jnum(now)
    requests = jget(inp, "requests")
    due = []
    if isinstance(requests, list):
        for req in requests:
            ps = slug(jget(req, "provider"))
            ident = strip(pystr_or_empty(jget(req, "identity_id")))
            apikey = strip(pystr_or_empty(jget(req, "api_key")))
            if not ps or not ident or not apikey:
                continue
            key = key_core(io, ps, ident, None)
            if is_raise(key):
                state.emit(st)
                return key
            ks = keystr(key)
            if not state.claim(ks, nowv):
                continue
            due.append(copy(req))
    if not due:
        state.emit(st)
        return []
    ctx = call(io, "contextvars.copy_context", {})
    if is_raise(ctx):
        state.emit(st)
        return ctx
    if not present(ctx):
        state.emit(st)
        return attr_raise(None, "run")
    thread = call(io, "threading.Thread", {"target": "context.run", "fn": "_refresh_entries",
                                           "due": copy(due), "name": "hermes-account-usage-refresh", "daemon": True})
    if is_raise(thread):
        state.emit(st)
        return thread
    if not present(thread):
        state.emit(st)
        return attr_raise(None, "start")
    sr = call(io, "thread.start", {"thread": copy(thread)})
    if is_raise(sr):
        state.emit(st)
        return sr
    state.emit(st)
    return [{"fn": "_refresh_entries", "due": copy(due)}]


def e_refresh_entries(io, inp, st):
    state = State(inp)
    requests = jget(inp, "requests")
    if isinstance(requests, list):
        for req in requests:
            prov = req.get("provider", MISSING) if isinstance(req, dict) else MISSING
            if prov is MISSING:
                state.emit(st)
                return mkraise("KeyError", "'provider'")
            ident = req.get("identity_id", MISSING)
            if ident is MISSING:
                state.emit(st)
                return mkraise("KeyError", "'identity_id'")
            key = key_core(io, prov, ident, None)
            if is_raise(key):
                state.emit(st)
                return key
            ks = keystr(key)
            r = call(io, "fetch_account_usage", {"provider": copy(prov),
                                                 "base_url": copy_or_null(jget(req, "base_url")),
                                                 "api_key": copy_or_null(jget(req, "api_key")),
                                                 "read_only": True, "identity_id": copy(ident)})
            pending = r if is_raise(r) else None
            state.release(ks)
            if pending is not None:
                state.emit(st)
                return pending
    state.emit(st)
    return None


ENTRIES = {
    "_plural": e_plural, "_get_json": e_get_json, "_data": e_data,
    "_fetch_portal_account": e_fetch_portal_account, "_key": e_key,
    "has_account_usage": e_has_account_usage, "_identity_id_for": e_identity_id_for,
    "remember_account_usage": e_remember, "cached_account_usage": e_cached,
    "snapshot_is_stale": e_snapshot_is_stale, "_claim_refresh": e_claim,
    "_release_refresh": e_release, "refresh_account_usage_async": e_refresh_async,
    "_refresh_legacy": e_refresh_legacy,
    "refresh_account_usage_entries_async": e_refresh_entries_async,
    "_refresh_entries": e_refresh_entries,
}


def mirror_case(func, inp):
    io = MirrorIO(inp.get("io"))
    st = {}
    ret = ENTRIES[func](io, inp, st)
    return {"ret": ret, "trace": io.trace, "st": st}


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
    spec = importlib.util.spec_from_file_location("au2_oracle", str(ORACLE))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    m.load_ns()
    cases = load_cases()
    mism = 0
    for i, c in enumerate(cases):
        func, inp = c["func"], c["in"]
        mir = norm(mirror_case(func, inp))
        live = m.run_case(func, inp)
        live = norm(live)
        if mir != live:
            mism += 1
            if mism <= 15:
                print("SHADOW MISMATCH [%s] case %d" % (func, i + 1))
                for k in ("ret", "trace", "st"):
                    if mir.get(k) != live.get(k):
                        print("  %s MIR : %s" % (k, json.dumps(mir.get(k), ensure_ascii=False)[:500]))
                        print("  %s LIVE: %s" % (k, json.dumps(live.get(k), ensure_ascii=False)[:500]))
    print("SHADOW diff: %d cases, %d mismatches" % (len(cases), mism))
    return mism


def do_proxy():
    cases = load_cases()
    lines = []
    for c in cases:
        env = mirror_case(c["func"], c["in"])
        lines.append(json.dumps({"func": c["func"], "in": c["in"], **env}, ensure_ascii=False))
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