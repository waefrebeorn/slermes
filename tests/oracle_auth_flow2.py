#!/usr/bin/env python3
"""Oracle for auth_flow2 — the three STILL-UNCLAIMED auth.py FLOW cores.

Ports covered (the unclaimed subset of live_parity_scan.json's 12 auth.py
REAL_GAP entries, verified by a CRLF-aware PoP grep at authoring time):
    primary_failure_wording
    OAuthProviderFlow.is_terminal_refresh_error
    _resolve_nous_access_token
The other 9 of the 12 are already claimed by port_auth.c / auth_helpers2.c /
commands.c and are NOT re-ported.

Drives the LIVE defs AST-extracted from the quarry (no heavy import): the
functions plus the ProviderConfig/OAuthProviderFlow classes from auth.py and
AuthError + its literal constants from auth_constants.py. Every effectful
collaborator is replaced by a recording stub returning the SAME scripted
results the C harness feeds its universal seam: a MISSING key fail-closes with
RuntimeError on both sides, a present JSON null is a valid None, and an array
script yields one result PER CALL (last repeats). Scripted {"__raise__": cls}
raises a REAL builtin of that class (full py3.14 parent hierarchy, including
the BaseException trio), so KeyboardInterrupt/SystemExit pass through every
with-block exactly as in the C port. Args/ret are DEEP-COPIED into the trace
at call time, matching the C harness's json_copy snapshots.

Input: stdin JSON lines from t_port_auth_flow2:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...]}}
Summary: ORACLE auth_flow2: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.

--self-test drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow  diffs a literal Python MIRROR of the C port against the LIVE drive
          over every fixture (the shadow-diff; must print 0 diffs).
"""
import ast
import copy
import json
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path = None  # set below (kept explicit for readability)
from pathlib import Path, PurePosixPath
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "auth_flow2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "hermes_cli", "auth.py")
SRC_CONST = os.path.join(QUARRY, "hermes_cli", "auth_constants.py")

# harness func -> live python name. The method name is unqualified here; drive()
# binds it to a SimpleNamespace self (only attributes are read).
PY = {
    "primary_failure_wording": "primary_failure_wording",
    "is_terminal_refresh_error": "is_terminal_refresh_error",
    "resolve_nous_access_token": "_resolve_nous_access_token",
}
WANTED = set(PY.values())

CUR = {"trace": [], "scripts": {}, "idx": {}}
NS = {}


# ── seam recorder ──

def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            # explicit array VALUE, not a per-call queue
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return maybe_raise(r)


EXC_MAP = {
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "FileExistsError": FileExistsError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "TimeoutError": TimeoutError, "BlockingIOError": BlockingIOError,
    "ProcessLookupError": ProcessLookupError, "ChildProcessError": ChildProcessError,
    "InterruptedError": InterruptedError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "AssertionError": AssertionError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError, "SystemError": SystemError,
    "BufferError": BufferError, "EOFError": EOFError, "Exception": Exception,
    "BaseException": BaseException, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit, "GeneratorExit": GeneratorExit,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "AuthError":
            raise NS["AuthError"](r.get("msg", ""))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


# ── extraction ──

def extract():
    from dataclasses import dataclass, field
    from typing import Any, Callable, Optional, FrozenSet
    import time as _time

    src = open(SRC, encoding="utf-8").read()
    const = open(SRC_CONST, encoding="utf-8").read()
    ns = {
        "__name__": "hermes_cli.auth_flow2_extracted",
        "json": json, "os": os, "sys": sys, "time": _time,
        "dataclass": dataclass, "field": field,
        "Any": Any, "Callable": Callable, "Optional": Optional, "FrozenSet": FrozenSet,
        "frozenset": frozenset,
    }
    tree = ast.parse(src)
    # classes first (OAuthProviderFlow carries the target method)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "OAuthProviderFlow":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # AuthError + its literal constants
    ctree = ast.parse(const)
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name == "AuthError":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CONST, "exec"), ns)
    for node in ctree.body:
        if (isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets)) or \
           (isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_CONST, "exec"), ns)
            except Exception:
                pass
    # auth.py module-level literal constants (_RESOLVE_TOKEN_CACHE_TTL_S etc.)
    for node in tree.body:
        if (isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets)) or \
           (isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    # target functions (top-level) + the OAuthProviderFlow method.
    # is_rate_limited_auth_error is a claimed sibling: extracted here ONLY as
    # the LIVE reference for primary_failure_wording (the C port transcribes
    # its pure leaf inline, with no PoP re-claim).
    EXTRA = {"is_rate_limited_auth_error"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
                (node.name in WANTED or node.name in EXTRA):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    if "is_terminal_refresh_error" in WANTED and "OAuthProviderFlow" in ns:
        ns["is_terminal_refresh_error"] = ns["OAuthProviderFlow"].is_terminal_refresh_error
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("AuthError", "OAuthProviderFlow", "CODEX_RATE_LIMITED_CODE",
              "DEFAULT_NOUS_CLIENT_ID", "AUTH_LOCK_TIMEOUT_SECONDS"):
        if k not in ns:
            print("FATAL missing class/constant: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording doubles / stubs ──

class SeamCM:
    """Generic context manager firing <name>_enter / <name>_exit."""
    def __init__(self, name, enter_args=None):
        self.name = name
        self.enter_args = enter_args or {}

    def __enter__(self):
        # The C harness hands each seam a json_copy of the scripted result, so
        # the live side must work on a fresh object too: otherwise the flow's
        # in-place state mutations write back into the shared fixture script.
        return copy.deepcopy(seam(self.name + "_enter", dict(self.enter_args)))

    def __exit__(self, et, ev, tb):
        seam(self.name + "_exit", {})
        return False


class CacheDouble:
    def get(self, key):
        return seam("cache_get", {"cache_key": key})

    def __setitem__(self, key, value):
        # value == (time.monotonic(), token); C fires cache_set with {key, token}
        seam("cache_set", {"cache_key": key, "token": value[1]})


class HttpxStub:
    def __init__(self):
        self.Timeout = lambda t: t  # httpx.Timeout(x) is pure identity here

    def Client(self, timeout=None, headers=None, verify=None):
        return SeamCM("httpx_client", {"timeout": timeout, "headers": headers, "verify": verify})


def held_connectors_token(state, skew_seconds=None):
    return seam("held_connectors_token", {"state": state, "skew_seconds": skew_seconds})


def is_guest_state(state):
    return seam("is_guest_state", {"state": state})


def refresh_guest_state(state, client, purpose=None):
    return seam("refresh_guest_state", {"state": state, "client": client, "purpose": purpose})


class AnonAuthModule(types.ModuleType):
    """from ... import a,b,c fires ONE import_anon_auth seam (first getattr)."""

    def __getattr__(self, name):
        if not self.__dict__.get("_fired"):
            self.__dict__["_fired"] = True
            seam("import_anon_auth", {})
        table = {
            "held_connectors_token": held_connectors_token,
            "is_guest_state": is_guest_state,
            "refresh_guest_state": refresh_guest_state,
        }
        if name not in table:
            raise AttributeError(name)
        return table[name]


PERSIST_MARKER = {"__callable__": "persist"}


def refresh_nous_or_quarantine(*, client=None, auth_store=None, state=None,
                               portal_base_url=None, client_id=None,
                               refresh_token=None, reason=None, persist=None):
    return seam("refresh_nous_or_quarantine", {
        "client": client, "auth_store": auth_store, "state": state,
        "portal_base_url": portal_base_url, "client_id": client_id,
        "refresh_token": refresh_token, "reason": reason,
        "persist": PERSIST_MARKER,
    })


def apply_nous_refreshed_tokens(state, refreshed, refresh_token):
    r = seam("apply_nous_refreshed_tokens", {
        "state": state, "refreshed": refreshed, "refresh_token": refresh_token})
    # both sides write result["access_token"] into state
    if isinstance(r, dict) and "access_token" in r:
        state["access_token"] = r["access_token"]
    return r


def nous_err(message, code, relogin=False):
    return NS["AuthError"](message, provider="nous", code=code, relogin_required=relogin)


def install_stubs(ns):
    ns["hermes_home_key"] = lambda: seam("hermes_home_key", {})
    ns["_RESOLVE_TOKEN_CACHE_LOCK"] = SeamCM("cache_lock")
    ns["_RESOLVE_TOKEN_CACHE"] = CacheDouble()
    ns["time"] = types.SimpleNamespace(monotonic=lambda: seam("monotonic", {}))
    ns["_provider_state_transaction"] = lambda provider_id: SeamCM(
        "txn", {"provider_id": provider_id})
    ns["_nous_err"] = nous_err
    ns["_nous_portal_base_url"] = lambda state: seam("nous_portal_base_url", {"state": state})
    ns["_resolve_verify"] = lambda insecure=None, ca_bundle=None, auth_state=None: seam(
        "resolve_verify", {"insecure": insecure, "ca_bundle": ca_bundle, "auth_state": auth_state})
    ns["_nous_shared_store_lock"] = lambda timeout_seconds=15.0: SeamCM(
        "shared_lock", {"timeout_seconds": timeout_seconds})
    ns["_save_provider_state_to_source"] = lambda auth_store, provider_id, state, state_source_path: seam(
        "save_provider_state_to_source", {"auth_store": auth_store, "provider_id": provider_id,
                                          "state": state, "state_source_path": state_source_path})
    ns["_write_shared_nous_state"] = lambda state, renewal=False: seam(
        "write_shared_nous_state", {"state": state, "renewal": renewal})
    ns["_merge_shared_nous_oauth_state"] = lambda state: seam(
        "merge_shared_nous_oauth_state", {"state": state})
    ns["_is_expiring"] = lambda expires_at_iso, skew_seconds: seam(
        "is_expiring", {"expires_at": expires_at_iso, "skew_seconds": skew_seconds})
    ns["_refresh_nous_or_quarantine"] = refresh_nous_or_quarantine
    ns["_apply_nous_refreshed_tokens"] = apply_nous_refreshed_tokens
    ns["_tls_state_from_verify"] = lambda verify: seam("tls_state_from_verify", {"verify": verify})
    ns["httpx"] = HttpxStub()

    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = []
    sys.modules["hermes_cli"] = pkg
    sys.modules["hermes_cli.anon_auth"] = AnonAuthModule("hermes_cli.anon_auth")


# ── materialization ──

def mat_exc(d):
    if d is None:
        return None
    kind = d.get("kind", "AuthError")
    if kind == "AuthError":
        return NS["AuthError"](d.get("message", ""), provider=d.get("provider", ""),
                               code=d.get("code"), relogin_required=d.get("relogin_required", False))
    cls = EXC_MAP.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(d.get("message", ""))


# ── live drive ──

def drive(ns, func, inp):
    if func not in PY:
        raise ValueError("unknown func: %s" % func)
    if func == "primary_failure_wording":
        return ns["primary_failure_wording"](mat_exc(inp.get("exc")))
    if func == "is_terminal_refresh_error":
        s = inp.get("self") or {}
        selfobj = types.SimpleNamespace(
            provider_id=s.get("provider_id", ""),
            terminal_refresh_codes=frozenset(s.get("terminal_refresh_codes") or []))
        meth = ns["OAuthProviderFlow"].is_terminal_refresh_error
        return meth(selfobj, mat_exc(inp.get("exc")))
    if func == "resolve_nous_access_token":
        return ns["_resolve_nous_access_token"](
            timeout_seconds=inp.get("timeout_seconds", 15.0),
            insecure=inp.get("insecure"),
            ca_bundle=inp.get("ca_bundle"),
            refresh_skew_seconds=inp.get("refresh_skew_seconds", 0))
    raise ValueError("unhandled func: %s" % func)


# ── comparison ──

def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


def reset_case(inp):
    """Per-case recorder state. The anon_auth import stub fires ONCE per
    executed `from ... import ...`, so its one-shot flag must be reset for
    each independently driven case (otherwise later cases lose the seam)."""
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams", {})
    CUR["idx"] = {}
    m = sys.modules.get("hermes_cli.anon_auth")
    if isinstance(m, AnonAuthModule):
        m.__dict__["_fired"] = False


def run_live(func, inp):
    """Drive live; return (want_object, raised_or_None)."""
    reset_case(inp)
    try:
        ret = drive(NS, func, inp)
        return {"ret": ret, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return ({"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                 "trace": CUR["trace"]}, ex)


def fixture():
    return _REPO / "tests" / "fixtures" / "auth_flow2.jsonl"


def load_cases():
    p = fixture()
    if not p.exists():
        return []
    return [json.loads(l) for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]


def check_func_set(cases):
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("FATAL <5 cases for: %s" % thin)
        sys.exit(2)


def self_test():
    ns = extract()
    install_stubs(ns)
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        _, ex = run_live(c["func"], c["in"])
        if isinstance(ex, AttributeError) and any(
                ("'%s'" % n) in str(ex) for n in list(PY) + list(SEAM_NAMES)):
            print("SELFTEST BINDING %s: %s: %s" % (c["func"], type(ex).__name__, ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
    print("SELFTEST %d cases, %d binding/name errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


SEAM_NAMES = [
    "hermes_home_key", "cache_lock", "cache_get", "cache_set", "monotonic",
    "txn", "shared_lock", "import_anon_auth", "is_guest_state",
    "held_connectors_token", "httpx_client", "refresh_guest_state",
    "write_shared_nous_state", "save_provider_state_to_source",
    "merge_shared_nous_oauth_state", "is_expiring", "refresh_nous_or_quarantine",
    "apply_nous_refreshed_tokens", "tls_state_from_verify", "nous_portal_base_url",
    "resolve_verify",
]


# ── shadow mirror (literal transcription of the C port, for --shadow) ──

def _shadow_memo(memoable, token):
    out = {"__seams": [], "__seq": []}
    if not memoable:
        return token
    out["__seq"].append(("cache_lock_enter", {}))
    out["__seq"].append(("monotonic", {}))
    out["__seq"].append(("cache_set", {"token": token}))
    out["__seq"].append(("cache_lock_exit", {}))
    return token


def shadow_drive(func, inp):
    """Literal Python MIRROR of the au2_* C port; fires the SAME seams in the
    same order, including CM exits on both normal and raise paths. Diffed
    against the live drive by --shadow (the shadow-diff, must be 0)."""
    reset_case(inp)

    if func == "primary_failure_wording":
        e = inp.get("exc") or {}
        limited = (e.get("kind") == "AuthError" and not e.get("relogin_required", False)
                   and e.get("code") == "codex_rate_limited")
        return (["rate-limited (429)", "Primary provider quota exhausted"] if limited
                else ["auth failed", "Primary auth failed"])

    if func == "is_terminal_refresh_error":
        e = inp.get("exc") or {}
        s = inp.get("self") or {}
        return bool(e.get("kind") == "AuthError"
                    and e.get("provider") == s.get("provider_id")
                    and e.get("code") in (s.get("terminal_refresh_codes") or [])
                    and bool(e.get("relogin_required", False)))

    # ── _resolve_nous_access_token ──
    tmo = inp.get("timeout_seconds", 15.0)
    insecure = inp.get("insecure")
    cab = inp.get("ca_bundle")
    skew = inp.get("refresh_skew_seconds", 0)
    memoable = (not bool(insecure)) and (cab is None)
    stack = []
    key = None

    def memo(token):
        if not memoable:
            return token
        seam("cache_lock_enter", {})
        try:
            seam("monotonic", {})
            seam("cache_set", {"cache_key": key, "token": token})
        finally:
            seam("cache_lock_exit", {})
        return token

    try:
        if memoable:
            key = seam("hermes_home_key", {})
            seam("cache_lock_enter", {})
            try:
                cached = seam("cache_get", {"cache_key": key})
            finally:
                seam("cache_lock_exit", {})
            if (cached is not None and isinstance(cached, list) and len(cached) >= 2
                    and isinstance(cached[0], (int, float))):
                m = seam("monotonic", {})
                if (m - cached[0]) < 5.0:
                    return cached[1]

        seam("txn_enter", {"provider_id": "nous"})
        stack.append("txn")
        tx = CUR["scripts"]["txn_enter"]["__arr__"]
        store, state, path = tx[0], copy.deepcopy(tx[1]), tx[2]
        if not state:
            raise NS["AuthError"]("Hermes is not logged into Nous Portal.", provider="nous")
        portal = seam("nous_portal_base_url", {"state": state})
        cid = state.get("client_id") or "hermes-cli"
        verify = seam("resolve_verify", {"insecure": insecure, "ca_bundle": cab,
                                         "auth_state": state})
        lto = max(tmo + 5.0, 15.0)
        seam("shared_lock_enter", {"timeout_seconds": lto})
        stack.append("shared_lock")
        seam("import_anon_auth", {})
        if seam("is_guest_state", {"state": state}):
            held = seam("held_connectors_token", {"state": state, "skew_seconds": skew})
            if held:
                return memo(held)
            to = tmo or 15.0
            seam("httpx_client_enter", {"timeout": to,
                                        "headers": {"Accept": "application/json"},
                                        "verify": verify})
            stack.append("httpx_client")
            try:
                # client value is the scripted httpx_client_enter result (last trace ret)
                client = CUR["trace"][-1]["ret"]
                minted = seam("refresh_guest_state", {"state": state, "client": client,
                                                      "purpose": "connectors"})
            finally:
                # pop BEFORE firing exit so a body raise + raising exit cannot
                # re-fire this CM in the outer unwind (C does cmn-- before exit)
                stack.pop()
                seam("httpx_client_exit", {})
            seam("save_provider_state_to_source", {"auth_store": store, "provider_id": "nous",
                                                   "state": state, "state_source_path": path})
            seam("write_shared_nous_state", {"state": state, "renewal": True})
            return memo(minted)

        merged = seam("merge_shared_nous_oauth_state", {"state": state})
        at = state.get("access_token")
        rt = state.get("refresh_token")
        if not isinstance(at, str) or not at:
            raise NS["AuthError"]("No access token found for Nous Portal login.", provider="nous")
        expiring = seam("is_expiring", {"expires_at": state.get("expires_at"),
                                        "skew_seconds": skew})
        if not expiring:
            if merged:
                seam("save_provider_state_to_source", {"auth_store": store,
                    "provider_id": "nous", "state": state, "state_source_path": path})
            return memo(at)
        if not isinstance(rt, str) or not rt:
            raise NS["AuthError"]("Session expired and no refresh token is available.",
                                  provider="nous")
        to2 = tmo or 15.0
        seam("httpx_client_enter", {"timeout": to2,
                                    "headers": {"Accept": "application/json"},
                                    "verify": verify})
        stack.append("httpx_client")
        try:
            client2 = CUR["trace"][-1]["ret"]
            refreshed = seam("refresh_nous_or_quarantine", {
                "client": client2, "auth_store": store, "state": state,
                "portal_base_url": portal, "client_id": cid, "refresh_token": rt,
                "reason": "managed_access_token_refresh_failure", "persist": PERSIST_MARKER})
        finally:
            stack.pop()
            seam("httpx_client_exit", {})
        ap = seam("apply_nous_refreshed_tokens", {"state": state, "refreshed": refreshed,
                                                   "refresh_token": rt})
        if isinstance(ap, dict) and "access_token" in ap:
            state["access_token"] = ap["access_token"]
        if portal is not None:
            state["portal_base_url"] = portal
        state["client_id"] = cid
        state["tls"] = seam("tls_state_from_verify", {"verify": verify})
        seam("save_provider_state_to_source", {"auth_store": store, "provider_id": "nous",
                                               "state": state, "state_source_path": path})
        seam("write_shared_nous_state", {"state": state, "renewal": True})
        return memo(state["access_token"])
    finally:
        for nm in reversed(stack):
            seam(nm + "_exit", {})


def shadow_mode():
    cases = load_cases()
    check_func_set(cases)
    ns = extract()
    install_stubs(ns)
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        try:
            shadow_ret = shadow_drive(c["func"], c["in"])
        except BaseException as ex:
            shadow_ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        got = {"ret": shadow_ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False))
    print("SHADOW auth_flow2: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    if "--shadow" in sys.argv:
        shadow_mode()
        return
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
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE auth_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE auth_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
