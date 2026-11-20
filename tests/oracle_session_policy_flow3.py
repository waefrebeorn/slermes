#!/usr/bin/env python3
"""Oracle for the gateway/session_policy.py third-wave FLOW cores
(src/gateway/session_policy_flow3.c).

AST-extracts the LIVE unclaimed defs (no heavy gateway import), seeds the
module constants + the real AuthorityRefusal (persistence/core/errors.py) + the
real LocalSessionPolicy dataclass, and replaces every live I/O dependency with
recording stubs driven by the SAME per-name scripts the C harness feeds its
universal seams (in.io). Compares the ordered seam-call trace plus the return
value and the mutated authority / value+private state.

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused; a
scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds (SystemExit /
KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec (AuthorityRefusal also carries
reason/field/detail). Scripted raise specs propagate VERBATIM (identity match).
make_exc resolves builtin exception kinds through the REAL builtins module, so
the full builtin exception hierarchy is preserved (no hand-rolled parent table).

Modeling: bind_launch_key / rebind_launch_key / release_launch_secrets mutate
in.authority (out.state.authority); _extract_config_secrets mutates in.value +
in.private (out.state.{value,private}). Tuple-path secret maps are encoded as
compact JSON-array string keys (path_key) on BOTH sides. LocalSessionPolicy is
denormed to its full 20-field dict (dataclass defaults applied). Claimed
siblings are seams (launch_skills, restore_policy, _key_slot,
unbind_turn_workspace); ported siblings compose inline. _ADOPTED_KEY_SETS is
cleared per case. Set-iteration domains: _secret_refs <=1 truthy credential_ref,
release_launch_secrets <=1 retired sid (deterministic ordered trace).

Input: stdin JSON lines from t_port_session_policy_flow3.
Summary: ORACLE session_policy_flow3: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import builtins
import copy
import json
import os
import sys
import types
import __future__
from dataclasses import MISSING, dataclass, fields, replace
from functools import cached_property
from pathlib import Path, PurePosixPath, PureWindowsPath, PurePath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "session_policy.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")

CUR = {"func": None, "in": {}, "trace": [], "idx": {}, "scripted": []}


def path_key(p):
    return json.dumps(list(p), separators=(",", ":"), ensure_ascii=True)


def raise_spec(e):
    if isinstance(e, AuthorityRefusal):
        return {"__raise__": "AuthorityRefusal", "msg": str(e),
                "reason": e.reason, "field": e.field, "detail": e.detail}
    return {"__raise__": type(e).__name__, "msg": str(e)}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "AuthorityRefusal":
        return AuthorityRefusal(msg, field=v.get("field"), detail=v.get("detail"))
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
            return e
        if kind in ("BaseExceptionGroup", "ExceptionGroup"):
            return cls(msg, [ValueError(msg)])
        return cls(msg)
    return type(str(kind), (Exception,), {})(msg)


def denorm(x):
    if isinstance(x, (PolicyDouble, AuthorityDouble, AttrDouble, PersistenceDouble)):
        return {k: denorm(v) for k, v in object.__getattribute__(x, "_d").items()}
    if isinstance(x, LocalSessionPolicyCls):
        return {f.name: denorm(getattr(x, f.name)) for f in fields(x)}
    if isinstance(x, PurePath):
        return x.as_posix()
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        if any(isinstance(k, tuple) for k in x):
            return {path_key(k): denorm(v) for k, v in x.items()}
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(denorm(v) for v in x)
    return x


def seam(name, args):
    args = {k: denorm(v) for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    if isinstance(r, dict) and "__raise__" in r:
        exc = make_exc(r)
        CUR["scripted"].append((exc, r))
        raise exc
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _norm_arg(self, a):
        if isinstance(a, BaseException):
            return raise_spec(a)
        return denorm(a)

    def _rec(self, name, fmt, a, k):
        args = {"fmt": fmt}
        if a:
            args["args"] = [self._norm_arg(x) for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return seam(name, args)

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", fmt, a, k)


class AttrDouble:
    """Read-only attribute carrier (getattr -> dict, AttributeError on miss)."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)


class AuthorityDouble:
    """Mutable authority carrier (getattr/setattr backed by _d)."""
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)

    def __setattr__(self, name, value):
        object.__getattribute__(self, "_d")[name] = value


class PolicyDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)

    def current_toolsets(self, config):
        return seam("policy.current_toolsets", {"config": config})


class BindingsDouble:
    def __init__(self, d):
        self._d = d

    def receipt(self, sid):
        return types.SimpleNamespace(policy=self._d.get("receipt_policy"))


class PersistenceDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def __getattr__(self, name):
        d = object.__getattribute__(self, "_d")
        if name == "bindings":
            return BindingsDouble(d)
        if name in d:
            return d[name]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % name)


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


# -- live-def extraction --
WANTED = (
    "_refuse", "_policy_refused", "_adopt_stored_policy", "_stored_policy",
    "_check_stored_surface", "_extract_config_secrets", "render_launch_skills",
    "bind_launch_key", "rebind_launch_key", "_secret_refs", "release_launch_secrets",
    "policy_for_source", "workspace_unavailable_result", "refused_turn_result",
    "release_session_workspaces", "policy_turn_toolsets",
)


def seed_errors(ns):
    tree = ast.parse(open(ERRORS, encoding="utf-8").read())
    by_name = {}
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            by_name[node.name] = node
    for name in ("PersistenceError", "AuthorityRefusal", "_one_line"):
        exec(compile(ast.Module(body=[by_name[name]], type_ignores=[]), ERRORS, "exec"), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    import contextlib
    import contextvars
    modname = "sp3_extracted_session_policy"
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": modname, "__file__": SRC,
        "json": json, "os": os, "Path": Path,
        "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "contextlib": contextlib, "contextvars": contextvars,
        "ContextVar": contextvars.ContextVar,
        "dataclass": dataclass, "fields": fields, "MISSING": MISSING,
        "cached_property": cached_property,
        "logging": types.SimpleNamespace(getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
    })
    seed_errors(ns)
    tree = ast.parse(src)
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=flags), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "LocalSessionPolicy":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


NS = extract()
AuthorityRefusal = NS["AuthorityRefusal"]
LocalSessionPolicyCls = NS["LocalSessionPolicy"]
NS["logger"] = RecLogger()

_POLICY_DEFAULTS = {
    "credential_ref": None, "config_secret_ref": None, "editor_mcp_json": None,
    "kanban_json": None, "safe_mode": False, "ignore_user_config": False,
    "skills_prompt": None, "coding_posture": False, "client_env_json": None,
    "client_env_remote": False, "client_env_rules_json": None, "edit_approval_policy": "ask",
}


def mk_policy(d):
    full = {f.name: _POLICY_DEFAULTS.get(f.name) for f in fields(LocalSessionPolicyCls)}
    if isinstance(d, dict):
        for f in fields(LocalSessionPolicyCls):
            if f.name in d:
                full[f.name] = d[f.name]
    return LocalSessionPolicyCls(**full)


def mod(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    if "." in name:
        parent, child = name.rsplit(".", 1)
        if parent in sys.modules:
            setattr(sys.modules[parent], child, m)
    return m


# -- claimed-sibling seams (NS-level) --
NS["launch_skills"] = lambda params: seam("launch_skills", {"params": params})
NS["restore_policy"] = lambda data: PolicyDouble(seam("restore_policy", {"data": data}) or {})
NS["_key_slot"] = lambda credential_ref: seam("_key_slot", {"credential_ref": credential_ref})
NS["unbind_turn_workspace"] = lambda task_id: seam("unbind_turn_workspace", {"task_id": task_id})

# -- foreign-module stub packages --
mod("gateway")
mod("hermes_cli")
mod("agent")
mod("tools")
mod("session_rules")

mod("gateway.session_a2a",
    is_forward_policy=lambda policy: seam("is_forward_policy", {"policy": policy}))

mod("agent.credential_persistence",
    _is_secret_payload_key=lambda key: seam("_is_secret_payload_key", {"key": key}),
    fingerprint_secret_value=lambda api_key: seam("fingerprint_secret_value", {"api_key": api_key}))

mod("hermes_cli.config", _SECRET_CONFIG_KEYS=frozenset({
    "api_key", "apikey", "key", "token", "access_token", "refresh_token", "id_token",
    "secret", "client_secret", "password", "passwd", "authorization",
    "private_key", "bearer", "jwt"}))


def build_preloaded_skills_prompt(skills, task_id=None):
    return seam("build_preloaded_skills_prompt", {"skills": skills, "task_id": task_id})


mod("agent.skill_commands",
    build_preloaded_skills_prompt=build_preloaded_skills_prompt,
    format_missing_skills=lambda missing: seam("format_missing_skills", {"missing": missing}))


def bind_editor_mcp(authority, session_id, policy, editor_secret=None):
    r = seam("bind_editor_mcp", {"authority": authority, "session_id": session_id,
                                 "policy": policy, "editor_secret": editor_secret})
    return mk_policy(r)


mod("gateway.session_local_mcp", PRIVATE_KEY=("editor_mcp",), bind_editor_mcp=bind_editor_mcp)

mod("session_rules.local_create",
    same_create_retry=lambda owner, request_json, client_request: seam(
        "same_create_retry", {"owner": owner, "request_json": request_json,
                              "client_request": client_request}))

mod("gateway.session_policy_credentials",
    PREFIX="profile-config-v1:",
    config_reference=lambda authority, session_id, policy, config_secrets: seam(
        "config_reference", {"authority": authority, "session_id": session_id,
                             "policy": policy, "config_secrets": config_secrets}))


def authority_persistence(authority):
    r = seam("authority_persistence", {"authority": authority})
    return PersistenceDouble(r if isinstance(r, dict) else {})


mod("gateway.session_persistence", authority_persistence=authority_persistence)


class AdapterMapDouble:
    def __init__(self, d):
        self._d = d or {}

    def get(self, key):
        v = self._d.get(key)
        if not isinstance(v, dict):
            return None
        pol = {k: (mk_policy(p) if isinstance(p, dict) else p)
               for k, p in (v.get("policies") or {}).items()}
        return types.SimpleNamespace(policies=pol)


mod("gateway.session_local_recovery",
    local_adapter_map=lambda authority: AdapterMapDouble(
        seam("local_adapter_map", {"authority": authority})))

mod("gateway.config", Platform=types.SimpleNamespace(LOCAL="local"))


class LocalSessionAdapter:
    def __init__(self, d):
        self._d = d or {}
        self.policies = {k: (mk_policy(p) if isinstance(p, dict) else p)
                         for k, p in (self._d.get("policies") or {}).items()}

    def authorize_source(self, source):
        return seam("adapter.authorize_source", {"source": source})


mod("gateway.session_local", LocalSessionAdapter=LocalSessionAdapter)


class RunnerDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d or {}))

    def _adapter_for_source(self, source):
        r = seam("runner._adapter_for_source", {"source": source})
        if isinstance(r, dict):
            return LocalSessionAdapter(r)
        return r


mod("agent.onboarding_session",
    onboarding_chat_toolsets=lambda toolsets, store, session_id, persistence=None: seam(
        "onboarding_chat_toolsets", {"toolsets": toolsets, "store": store,
                                     "session_id": session_id, "persistence": persistence}))

mod("agent.i18n", t=lambda key, **kw: seam("t", {"key": key, "path": kw.get("path")}))


def stamp_failure(result, kind, flag):
    return seam("stamp_failure", {"result": result, "kind": kind, "flag": flag})


mod("agent.turn_failure_copy",
    stamp_failure=stamp_failure,
    launch_credentials_copy=lambda exc: seam("launch_credentials_copy", {"exc": exc}))

mod("agent.error_surface",
    build_error_surface_from_result=lambda result: seam(
        "build_error_surface_from_result", {"result": result}))


class OwnerScopeCM:
    def __init__(self, authority):
        self.authority = authority

    def __enter__(self):
        return seam("owner_scope.__enter__", {"authority": self.authority})

    def __exit__(self, et, ev, tb):
        spec = raise_spec(ev) if ev is not None else None
        return seam("owner_scope.__exit__", {"exc": spec})


mod("gateway.session_authorities", owner_scope=lambda authority: OwnerScopeCM(authority))


def _sha256(data):
    s = data.decode("utf-8") if isinstance(data, bytes) else data
    r = seam("hashlib.sha256", {"data": s})
    return types.SimpleNamespace(hexdigest=lambda: r)


mod("hashlib", sha256=_sha256)
mod("hmac", compare_digest=lambda a, b: seam("hmac.compare_digest", {"a": a, "b": b}))
mod("logging", getLogger=lambda name: RecLogger())


def install_case():
    CUR["idx"] = {}
    CUR["scripted"] = []
    NS["logger"] = RecLogger()
    NS["_ADOPTED_KEY_SETS"] = set()


AUTH_FUNCS = {"bind_launch_key", "rebind_launch_key", "release_launch_secrets"}


def wrap_sessions(auth_in):
    """Wrap authority.sessions entries so live entry.source.platform resolves."""
    sess = auth_in.get("sessions")
    if isinstance(sess, dict):
        wrapped = {}
        for sid, entry in sess.items():
            src = entry.get("source") if isinstance(entry, dict) else None
            src_d = AttrDouble(src) if isinstance(src, dict) else src
            wrapped[sid] = AttrDouble({"source": src_d})
        auth_in["sessions"] = wrapped
    return auth_in


def decode_secret_map(d):
    if not isinstance(d, dict):
        return None
    return {tuple(json.loads(k)): copy.deepcopy(v) for k, v in d.items()}


def run_flow(fn, state_fn):
    try:
        ret = fn()
        return {"ret": denorm(ret), "state": state_fn()}
    except BaseException as e:
        spec = None
        for exc, s in CUR["scripted"]:
            if e is exc:
                spec = s
                break
        if spec is None:
            spec = raise_spec(e)
        return {"ret": spec, "state": state_fn()}


def drive(func, inp):
    nostate = lambda: None
    if func in AUTH_FUNCS:
        auth_in = copy.deepcopy(inp.get("authority") or {})
        if func in ("rebind_launch_key", "release_launch_secrets"):
            wrap_sessions(auth_in)
        authority = AuthorityDouble(auth_in)
        state_fn = lambda: {"authority": denorm(authority)}
    else:
        authority = None
        state_fn = nostate

    if func == "_refuse":
        return run_flow(lambda: NS["_refuse"](inp.get("field"), inp.get("detail")), state_fn)
    if func == "_policy_refused":
        return run_flow(lambda: NS["_policy_refused"](inp.get("field"), inp.get("detail")), state_fn)
    if func == "_adopt_stored_policy":
        return run_flow(lambda: NS["_adopt_stored_policy"](inp.get("data")), state_fn)
    if func == "_stored_policy":
        return run_flow(lambda: NS["_stored_policy"](inp.get("data")), state_fn)
    if func == "_check_stored_surface":
        return run_flow(lambda: NS["_check_stored_surface"](PolicyDouble(inp.get("policy"))), state_fn)
    if func == "_extract_config_secrets":
        value = copy.deepcopy(inp.get("value"))
        private = decode_secret_map(inp.get("private"))
        st = lambda: {"value": denorm(value), "private": denorm(private)}
        return run_flow(lambda: NS["_extract_config_secrets"](value, private, ()), st)
    if func == "render_launch_skills":
        return run_flow(lambda: NS["render_launch_skills"](inp.get("params"), inp.get("session_id")), state_fn)
    if func == "bind_launch_key":
        cs = decode_secret_map(inp.get("config_secrets")) or {}
        return run_flow(lambda: NS["bind_launch_key"](
            authority, inp.get("session_id"), PolicyDouble(inp.get("policy")),
            inp.get("api_key"), config_secrets=cs, owner_key=inp.get("owner_key", False),
            client_request=inp.get("client_request")), state_fn)
    if func == "rebind_launch_key":
        return run_flow(lambda: NS["rebind_launch_key"](
            authority, AttrDouble(inp.get("session")), inp.get("api_key")), state_fn)
    if func == "_secret_refs":
        auth_in = wrap_sessions(copy.deepcopy(inp.get("authority") or {}))
        retired = set(inp.get("retired") or [])
        return run_flow(lambda: NS["_secret_refs"](AuthorityDouble(auth_in), retired), nostate)
    if func == "release_launch_secrets":
        return run_flow(lambda: NS["release_launch_secrets"](authority, inp.get("session_ids")), state_fn)
    if func == "policy_for_source":
        runner = RunnerDouble(inp.get("runner"))
        source = AttrDouble(inp.get("source")) if inp.get("source") is not None else None
        return run_flow(lambda: NS["policy_for_source"](runner, source), nostate)
    if func == "workspace_unavailable_result":
        return run_flow(lambda: NS["workspace_unavailable_result"](PolicyDouble(inp.get("policy"))), state_fn)
    if func == "refused_turn_result":
        e = inp.get("exc") or {}
        exc = AuthorityRefusal(e.get("reason"), field=e.get("field"), detail=e.get("detail"))
        return run_flow(lambda: NS["refused_turn_result"](PolicyDouble(inp.get("policy")), exc), nostate)
    if func == "release_session_workspaces":
        return run_flow(lambda: NS["release_session_workspaces"](
            inp.get("authority"), inp.get("session_ids")), nostate)
    if func == "policy_turn_toolsets":
        return run_flow(lambda: NS["policy_turn_toolsets"](
            inp.get("authority"), PolicyDouble(inp.get("policy")), inp.get("config"),
            inp.get("session_id")), nostate)
    raise ValueError("unknown func: %s" % func)


def main():
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        install_case()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE session_policy_flow3: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
