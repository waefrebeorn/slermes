#!/usr/bin/env python3
"""Oracle for src/tools/browser_tools_flow.c — drives the LIVE
tools/browser_use_cli.py UNCLAIMED browser-tools-surface defs (CDP-env / routing /
credentials / config cluster + pure leaves) under IDENTICAL scripted seams.

The task names the target "tools/browser_tools.py"; that file does not exist in
the quarry — the browser tools surface is tools/browser_use_cli.py (browser_exec).
PoP annotations credit the REAL defining module tools/browser_use_cli.py.

The 22 ported targets + the _CodeUrlVerdict NamedTuple are AST-extracted in source
order; every node is compiled with the PEP 563 future flag so annotations stay
strings, and the exec namespace is a registered sys.modules module (py3.14 needs a
real module alias). The heavy quarry import chain is never imported. Ported
functions call each other REAL (internal composition); every collaborator (claimed
siblings _resolve_real_profile_cdp / _resolve_backend_cdp / _find_cli, foreign
imports, importlib, supervisor registry, harness dirs, logger) is a recording stub
fed the SAME "io" scripts the C harness feeds bt_call.

Script shapes (shared with the C universal seam): absent key -> None;
{"$seq": [...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
<name>")); {"__raise__": cls, "msg": m} raises a REAL exception (trace ret is the
WHOLE marker); any other value reused. None-valued seam arg keys dropped on both
sides. Comparison: ordered trace + canonical ret + post-call state
({env, real_profile_fallback, real_profile_note_delivered}; {} on any raise).
Integral floats collapse to ints; tuples/NamedTuples -> arrays; the delivered set
echoes sorted. Fail-closed: 0 cases => exit 1; any per-case exception rides as
{"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE browser_tools_flow: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import json
import os
import re
import sys
import types
from pathlib import Path, PurePath

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "browser_use_cli.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "browser_use_cli.py")
NAME = "browser_tools_flow"

PORTED = [
    "_set_cdp_env", "_has_cdp_env", "_clamp_timeout", "_no_code_error",
    "_sidecar_daemon_name", "_served_profile_tag", "_browser_use_api_key_set",
    "_use_gateway", "_harness_credentials", "_export_real_profile_note",
    "_mark_local_browser", "_quiet", "_lazy_call", "_real_profile_consented",
    "_code_url_verdict", "_attach_vault_supervisor", "_record_driven_tab",
    "_published_daemon_pid", "_take_real_profile_note", "_route_backend",
    "_route_exec", "browser_exec_available",
]
# Claimed siblings + unclaimed I/O leaves that stay SEAMED here.
SEAMED_SIBLINGS = ["_resolve_real_profile_cdp", "_resolve_backend_cdp", "_find_cli"]
CLASSES = ["_CodeUrlVerdict"]
CONSTS = [
    "_HARNESS_PASSTHROUGH_KEYS", "_URL_RE", "_BROWSER_USE_PROVIDER",
    "_DEFAULT_TIMEOUT_S", "_MIN_TIMEOUT_S", "_MAX_TIMEOUT_S",
    "_REAL_PROFILE_FALLBACK_SENTINEL", "_LOCAL_FALLBACK_SENTINEL",
    "_BOT_DESKTOP_BROWSER_SENTINEL", "_PRIVATE_BROWSER_SENTINEL",
]
WANTED = set(PORTED)
TAKES_ENV = {
    "_set_cdp_env", "_has_cdp_env", "_mark_local_browser", "_export_real_profile_note",
    "_attach_vault_supervisor", "_record_driven_tab", "_published_daemon_pid",
    "_route_backend", "_route_exec",
}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tgts = _node.targets if isinstance(_node, ast.Assign) else [_node.target]
        for _t in tgts:
            if isinstance(_t, ast.Name) and _t.id in CONSTS:
                CONST_CODE[_t.id] = _compile_node(_node)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE)) | (set(CONSTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return v.as_posix()
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


class LazyModule:
    """importlib.import_module result: getattr(mod, name) is a recording callable."""
    def __getattr__(self, name):
        return SeamFn(name)


class SupervisorDouble:
    def __init__(self):
        self.set_driven_target = SeamFn("supervisor.set_driven_target")


def materialize(name, script):
    if script is None:
        return None
    if name == "import_module":
        return LazyModule()
    if name == "harness_pid_file":
        return Path(script) if isinstance(script, str) else script
    if name == "SUPERVISOR_REGISTRY.get":
        return SupervisorDouble()
    if name == "get_dialog_policy_config":
        return tuple(script) if isinstance(script, list) else script
    return script


# ── seam arg projections (identical field sets to the C args objects) ───────
def _get_secret_args(a, k):
    d = {"key": a[0]}
    if len(a) > 1:
        d["default"] = a[1]
    return d


SEAM_ARGS = {
    "get_hermes_home_override": lambda a, k: {},
    "hermes_home_key": lambda a, k: {},
    "get_secret": _get_secret_args,
    "is_truthy_value": lambda a, k: {"value": a[0], "default": k.get("default", False)},
    "nous_managed_provider": lambda a, k: {},
    "serves_routed_profile": lambda a, k: {},
    "current_secret_scope": lambda a, k: {},
    "real_profile_note": lambda a, k: {"key": a[0]},
    "import_module": lambda a, k: {"module": a[0]},
    "hybrid_routes_locally": lambda a, k: {"url": a[0]},
    "evaluate_url_safety": lambda a, k: {"url": a[0], "auto_local": k["auto_local"]},
    "get_dialog_policy_config": lambda a, k: {},
    "resolve_cdp_override": lambda a, k: {"cdp": a[0]},
    "SUPERVISOR_REGISTRY.get_or_start": lambda a, k: {
        "task_id": k["task_id"], "cdp_url": k["cdp_url"],
        "dialog_policy": k["dialog_policy"], "dialog_timeout_s": k["dialog_timeout_s"]},
    "SUPERVISOR_REGISTRY.get": lambda a, k: {"task_id": a[0]},
    "harness_current_target": lambda a, k: {"env": a[0]},
    "supervisor.set_driven_target": lambda a, k: {"target": a[0]},
    "harness_pid_file": lambda a, k: {"env": a[0]},
    "read_text": lambda a, k: {"path": a[0].as_posix(), "encoding": k.get("encoding")},
    "_resolve_real_profile_cdp": lambda a, k: {"env": a[0], "force_local": k["force_local"]},
    "real_profile_fallback_note": lambda a, k: {"err": a[0]},
    "_resolve_backend_cdp": lambda a, k: {
        "env": a[0], "task_id": a[1], "session_name": k["session_name"],
        "force_local_sidecar": k["force_local_sidecar"]},
    "_find_cli": lambda a, k: {},
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = materialize(name, script)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        proj = SEAM_ARGS.get(self.name, lambda a, k: {})
        return record(self.name, proj(a, kw))


class LoggerDouble:
    def _log(self, level, msg, args):
        return record("logger." + level, {"msg": msg, "args": list(args)})

    def debug(self, msg, *args, **kw):
        return self._log("debug", msg, args)

    def info(self, msg, *args, **kw):
        return self._log("info", msg, args)

    def warning(self, msg, *args, **kw):
        return self._log("warning", msg, args)


_LOGGER = LoggerDouble()


# ── stub modules for lazy imports (singletons dispatch to current scripts) ──
def _mod(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        m = sys.modules.get(nm)
        if m is None:
            m = types.ModuleType(nm)
            m.__path__ = []
            sys.modules[nm] = m
        if i > 1:
            setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], m)
    for kk, vv in attrs.items():
        setattr(sys.modules[name], kk, vv)
    return sys.modules[name]


def install_stubs():
    _mod("tools.browser_url_policy",
         evaluate_url_safety=SeamFn("evaluate_url_safety"),
         hybrid_routes_locally=SeamFn("hybrid_routes_locally"))
    _mod("agent.secret_scope",
         current_secret_scope=SeamFn("current_secret_scope"),
         get_secret=SeamFn("get_secret"),
         serves_routed_profile=SeamFn("serves_routed_profile"))
    _mod("hermes_constants",
         get_hermes_home_override=SeamFn("get_hermes_home_override"),
         hermes_home_key=SeamFn("hermes_home_key"))
    _mod("tools.browser_session", real_profile_note=SeamFn("real_profile_note"))
    _mod("tools.browser_session_cdp",
         get_dialog_policy_config=SeamFn("get_dialog_policy_config"),
         resolve_cdp_override=SeamFn("resolve_cdp_override"))
    _mod("tools.browser_supervisor",
         SUPERVISOR_REGISTRY=types.SimpleNamespace(
             get=SeamFn("SUPERVISOR_REGISTRY.get"),
             get_or_start=SeamFn("SUPERVISOR_REGISTRY.get_or_start")))
    _mod("tools.browser_session_real_profile",
         real_profile_fallback_note=SeamFn("real_profile_fallback_note"))
    # NOUS_MANAGED_PROVIDER is read via `from ... import` -> module __getattr__.
    # The module must NOT be a package (no __path__): otherwise _handle_fromlist
    # does a hasattr() probe AND IMPORT_FROM does getattr(), firing the seam twice.
    tbh = _mod("tools.tool_backend_helpers")
    if hasattr(tbh, "__path__"):
        del tbh.__path__

    def _ga(name):
        if name == "NOUS_MANAGED_PROVIDER":
            return record("nous_managed_provider", {})
        raise AttributeError(name)

    tbh.__getattr__ = _ga


def install_path_patches():
    def read_text_stub(self, encoding=None, errors=None):
        return SeamFn("read_text")(self, encoding=encoding)

    Path.read_text = read_text_stub


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "bt_live_browser_use_cli",
    "__builtins__": builtins,
    "json": json, "re": re,
    "Any": object, "Callable": object, "Optional": object, "Dict": object,
    "List": object, "NamedTuple": __import__("typing").NamedTuple,
    "logger": _LOGGER,
    "importlib": types.SimpleNamespace(import_module=SeamFn("import_module")),
    "harness_current_target": SeamFn("harness_current_target"),
    "harness_pid_file": SeamFn("harness_pid_file"),
    "is_truthy_value": SeamFn("is_truthy_value"),
}
_MOD = types.ModuleType("bt_live_browser_use_cli")
_MOD.__dict__.update(_BASE_NS)
sys.modules["bt_live_browser_use_cli"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CONSTS:
    exec(CONST_CODE[_nm], _BASE_NS)
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
_BASE_NS["logger"] = _LOGGER
# seamed module siblings are always recorders (the 22 targets stay real)
for _nm in SEAMED_SIBLINGS:
    _BASE_NS[_nm] = SeamFn(_nm)
install_stubs()
install_path_patches()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    st = inp.get("state") or {}
    _BASE_NS["_real_profile_fallback"] = dict(st.get("real_profile_fallback") or {})
    _BASE_NS["_real_profile_note_delivered"] = set(st.get("real_profile_note_delivered") or [])
    _BASE_NS["logger"] = _LOGGER
    env = None
    if rec["func"] in TAKES_ENV:
        env = dict(inp.get("env") or {})
    return _BASE_NS, trace, env


def drive(rec, ns, inp, env):
    func = rec["func"]
    if func == "_set_cdp_env":
        ns[func](env, inp.get("cdp", ""))
        return None
    if func == "_has_cdp_env":
        return ns[func](env)
    if func == "_clamp_timeout":
        return ns[func](inp.get("timeout_s"))
    if func == "_no_code_error":
        return ns[func](inp.get("code"))
    if func == "_sidecar_daemon_name":
        return ns[func](inp.get("name"))
    if func == "_served_profile_tag":
        return ns[func]()
    if func == "_browser_use_api_key_set":
        return ns[func]()
    if func == "_use_gateway":
        return ns[func](inp.get("browser_cfg"))
    if func == "_harness_credentials":
        return ns[func]()
    if func == "_export_real_profile_note":
        ns[func](env, inp.get("key", ""))
        return None
    if func == "_mark_local_browser":
        ns[func](env, inp.get("session_obj"), inp.get("key", ""))
        return None
    if func == "_quiet":
        return ns[func](SeamFn(inp.get("fn_seam", "fn")), inp.get("default"), inp.get("log_prefix", ""))
    if func == "_lazy_call":
        return ns[func](inp.get("module", ""), inp.get("lname", ""), inp.get("default"),
                        inp.get("log_prefix", ""))
    if func == "_real_profile_consented":
        return ns[func]()
    if func == "_code_url_verdict":
        return ns[func](inp.get("code"))
    if func == "_attach_vault_supervisor":
        ns[func](env, inp.get("task_id"))
        return None
    if func == "_record_driven_tab":
        ns[func](env, inp.get("task_id"))
        return None
    if func == "_published_daemon_pid":
        return ns[func](env)
    if func == "_take_real_profile_note":
        return ns[func](inp.get("route_key", ""))
    if func == "_route_backend":
        return ns[func](env, inp.get("session", ""), inp.get("task_id"), inp.get("local", False),
                        force_local_sidecar=inp.get("force_local_sidecar", False),
                        real_profile=inp.get("real_profile", True))
    if func == "_route_exec":
        return ns[func](env, inp.get("session", ""), inp.get("task_id"), inp.get("local", False),
                        inp.get("local_sidecar", False), inp.get("route_key", ""))
    if func == "browser_exec_available":
        return ns[func]()
    raise KeyError(f"unknown func {func}")


def out_state(ns, env):
    return {
        "env": norm(env) if env is not None else None,
        "real_profile_fallback": {k: norm(v) for k, v in ns["_real_profile_fallback"].items()},
        "real_profile_note_delivered": sorted(ns["_real_profile_note_delivered"]),
    }


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            ns, trace, env = build_ns(rec)
            try:
                ret = drive(rec, ns, rec["in"], env)
                out = {"ret": norm(ret), "trace": trace, "state": out_state(ns, env)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": {}}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:900]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag binding bugs / drive failures ──────
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(90, exit=True)
    bad = 0
    names = set(PORTED) | set(SEAMED_SIBLINGS)
    seen_funcs = set()
    for i, rec in enumerate(fixtures):
        seen_funcs.add(rec["func"])
        try:
            ns, trace, env = build_ns(rec)
            try:
                ret = drive(rec, ns, rec["in"], env)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError binding bug: {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    absent = set(PORTED) - seen_funcs
    if absent:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(absent)}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "browser_tools_flow.jsonl"
        with open(fx, encoding="utf-8") as fh:
            recs = [json.loads(l) for l in fh.read().splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()