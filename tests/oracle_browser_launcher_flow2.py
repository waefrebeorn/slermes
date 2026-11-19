#!/usr/bin/env python3
"""Oracle for src/tools/browser_launcher_flow2.c — drives the LIVE
tools/browser_launcher.py UNCLAIMED-remainder defs (records / lookup / stop /
owned-launch tail) under IDENTICAL scripted seams.

The 33 ported targets + the LaunchedBrowser dataclass are AST-extracted in
source order; every node is compiled with the PEP 563 future flag so annotations
stay strings, and the exec namespace is a registered sys.modules module (py3.14
dataclasses needs a real module alias). The heavy quarry import chain is never
imported. Ported functions call each other REAL (internal composition); every
collaborator (claimed flow1 siblings, foreign-module imports, OS/file/process
facts, holders leases, clock, logger, proc methods) is a recording stub fed the
SAME "io" scripts the C harness feeds bl2_call.

Script shapes (shared with the C universal seam): absent key -> None;
{"$seq": [...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
<name>")); {"__raise__": cls, "msg": m} raises a REAL exception (trace ret is the
WHOLE marker); any other value reused. None-valued seam arg keys dropped on both
sides. Comparison: ordered trace + canonical ret + post-call state
({browsers,procs,launching}; {} on any raise). Path-bearing args/results project
via as_posix() to match the C port's POSIX path math; integral floats collapse to
ints; sets sort. Fail-closed: 0 cases => exit 1; any per-case exception rides as
{"__raise__": cls} (BaseException, so SystemExit/KeyboardInterrupt propagate).
Summary: 'ORACLE browser_launcher_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import dataclasses
import hashlib
import json
import os
import posixpath
import stat
import subprocess
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
if not (QUARRY / "tools" / "browser_launcher.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "browser_launcher.py")
NAME = "browser_launcher_flow2"

PORTED = [
    "_ephemeral_root", "_records_dir", "_profile_digest", "_same_profile",
    "_devtools_id", "_record_name", "_shared_profiles_root", "_is_private_dir",
    "_private_dir", "_shared_state_dir", "_state_dir", "_record_path",
    "_make_browser", "_record_is", "_owner_alive", "_load_record",
    "_write_record", "_remove_record", "_adopt_record", "_alive",
    "_foreign_alive", "_same_browser", "_heir_key", "_tracked_browser",
    "_terminate", "_forget", "get_browser", "_last_holder_out", "_stop_locked",
    "stop_browser", "stop_all_browsers", "_abandon_launch", "_launch_owned",
]
# Claimed flow1 siblings + unclaimed I/O leaves that stay SEAMED here.
SEAMED_SIBLINGS = [
    "_read_devtools_file", "_devtools_endpoint_live", "_pid_alive", "_start_time",
    "_prepare_dir", "build_chromium_argv", "_resolve_headed", "_wait_for_devtools",
    "_launch_timeout", "_spawn", "_key_lock",
]
CLASSES = ["LaunchedBrowser"]
WANTED = set(PORTED)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CLASS_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node)
    elif isinstance(_node, ast.ClassDef) and _node.name in CLASSES:
        CLASS_CODE[_node.name] = _compile_node(_node)
_missing = (WANTED - set(DEF_CODE)) | (set(CLASSES) - set(CLASS_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


# ── canonical projection ────────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, PurePath):
        return v.as_posix()
    if dataclasses.is_dataclass(v) and not isinstance(v, type):
        return norm(dataclasses.asdict(v))
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
    if cls_name == "TimeoutExpired":
        return subprocess.TimeoutExpired(marker.get("cmd", "cmd"), marker.get("timeout", 0))
    if cls_name == "SubprocessError":
        return subprocess.SubprocessError(msg)
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


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def mk_browser(d):
    if isinstance(d, dict):
        return _LaunchedBrowser(**d)
    return d


def mk_proc(d):
    if not isinstance(d, dict):
        return d
    ns = types.SimpleNamespace(pid=d.get("pid"))
    ns.poll = lambda: record("proc.poll", {"pid": ns.pid})
    ns.wait = lambda timeout=None: record("proc.wait", {"pid": ns.pid, "timeout": timeout})
    ns.kill = lambda: record("proc.kill", {"pid": ns.pid})
    return ns


def materialize(name, script):
    if name in ("records_lock", "_key_lock"):
        return NoopLock()  # the lock value is scripted but always a context manager
    if script is None:
        return None
    if name == "lstat":
        return types.SimpleNamespace(**script) if isinstance(script, dict) else script
    if name == "read_text":
        if isinstance(script, str) and script.startswith("\ufeff"):
            return script[1:]  # utf-8-sig BOM strip
        return script
    if name == "_get_lock_dir":
        return Path(script) if isinstance(script, str) else script
    if name == "_spawn":
        return mk_proc(script)
    if name in ("_wait_for_devtools", "_read_devtools_file"):
        return tuple(script) if isinstance(script, list) else script
    return script


# ── seam arg projections (identical field sets to the C args objects) ───────
SEAM_ARGS = {
    "getpid": lambda a, k: {},
    "getuid": lambda a, k: {},
    "monotonic": lambda a, k: {},
    "realpath": lambda a, k: {"path": a[0]},
    "lstat": lambda a, k: {"path": a[0].as_posix()},
    "mkdir": lambda a, k: {"path": a[0].as_posix(), "mode": k.get("mode", 0o777),
                           "parents": k.get("parents", False), "exist_ok": k.get("exist_ok", False)},
    "read_text": lambda a, k: {"path": a[0].as_posix(), "encoding": k.get("encoding")},
    "exists": lambda a, k: {"path": a[0].as_posix()},
    "unlink": lambda a, k: {"path": a[0].as_posix(), "missing_ok": k.get("missing_ok", False)},
    "rmtree": lambda a, k: {"path": a[0], "ignore_errors": k.get("ignore_errors", False)},
    "atomic_json_write": lambda a, k: {"path": a[0], "record": a[1], "indent": k.get("indent")},
    "same_namespace": lambda a, k: {"record": a[0]},
    "ns_tag": lambda a, k: {},
    "ns_identity": lambda a, k: {},
    "drop_lease": lambda a, k: {"record": a[0]},
    "other_live_holders": lambda a, k: {"record": a[0], "start_time": a[1]},
    "held_here": lambda a, k: {"data": a[0], "field": a[1]},
    "successor": lambda a, k: {"others": a[0]},
    "hand_over": lambda a, k: {"record": a[0], "data": a[1], "heir": a[2]},
    "records_lock": lambda a, k: {"path": a[0], "blocking": k.get("blocking")},
    "_get_lock_dir": lambda a, k: {},
    "ProcessRegistry._terminate_host_pid": lambda a, k: {"pid": a[0], "start_time": a[1]},
    "build_browser_env": lambda a, k: {},
    "_read_devtools_file": lambda a, k: {"user_data_dir": a[0]},
    "_prepare_dir": lambda a, k: {"user_data_dir": a[0], "ephemeral": a[1]},
    "build_chromium_argv": lambda a, k: {"executable": a[0], "user_data_dir": a[1],
                                         "packaged": k["packaged"], "headed": k["headed"],
                                         "sandbox_bypass": k["sandbox_bypass"],
                                         "extra_args": k.get("extra_args", ())},
    "_resolve_headed": lambda a, k: {"headed": a[0], "env": a[1]},
    "_launch_timeout": lambda a, k: {"timeout": a[0]},
    "_wait_for_devtools": lambda a, k: {"pid": a[0].pid, "user_data_dir": a[1], "deadline": a[2]},
    "_pid_alive": lambda a, k: {"pid": a[0]},
    "_start_time": lambda a, k: {"pid": a[0]},
    "_devtools_endpoint_live": lambda a, k: {"port": a[0], "path": a[1]},
    "_spawn": lambda a, k: {"argv": a[0], "env": a[1], "user_data_dir": a[2]},
    "_key_lock": lambda a, k: {"key": a[0]},
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
        return record(self.name, SEAM_ARGS[self.name](a, kw))


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
    _mod("tools.browser_launcher_holders",
         ns_tag=SeamFn("ns_tag"), same_namespace=SeamFn("same_namespace"),
         ns_identity=SeamFn("ns_identity"), drop_lease=SeamFn("drop_lease"),
         other_live_holders=SeamFn("other_live_holders"), held_here=SeamFn("held_here"),
         successor=SeamFn("successor"), hand_over=SeamFn("hand_over"),
         records_lock=SeamFn("records_lock"))
    _mod("gateway.status", _get_lock_dir=SeamFn("_get_lock_dir"))
    _mod("utils", atomic_json_write=SeamFn("atomic_json_write"))
    _mod("tools.process_registry",
         ProcessRegistry=types.SimpleNamespace(
             _terminate_host_pid=SeamFn("ProcessRegistry._terminate_host_pid")))
    _mod("tools.browser_session", build_browser_env=SeamFn("build_browser_env"))


def install_path_patches():
    def lstat_stub(self):
        return SeamFn("lstat")(self)

    def mkdir_stub(self, mode=0o777, parents=False, exist_ok=False):
        return SeamFn("mkdir")(self, mode=mode, parents=parents, exist_ok=exist_ok)

    def read_text_stub(self, encoding=None, errors=None):
        return SeamFn("read_text")(self, encoding=encoding)

    def exists_stub(self):
        return SeamFn("exists")(self)

    def unlink_stub(self, missing_ok=False):
        return SeamFn("unlink")(self, missing_ok=missing_ok)

    Path.lstat = lstat_stub
    Path.mkdir = mkdir_stub
    Path.read_text = read_text_stub
    Path.exists = exists_stub
    Path.unlink = unlink_stub


# ── base namespace ──────────────────────────────────────────────────────────
_BASE_NS = {
    "__name__": "bl2_live_browser_launcher",
    "__builtins__": builtins,
    "json": json, "hashlib": hashlib, "stat": stat, "subprocess": subprocess,
    "Path": Path, "dataclass": dataclasses.dataclass,
    "Any": object, "Callable": object, "Optional": object,
    "logger": _LOGGER,
    "_lock": NoopLock(),
}
_MOD = types.ModuleType("bl2_live_browser_launcher")
_MOD.__dict__.update(_BASE_NS)
sys.modules["bl2_live_browser_launcher"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm in CLASSES:
    exec(CLASS_CODE[_nm], _BASE_NS)
_LaunchedBrowser = _BASE_NS["LaunchedBrowser"]
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
_BASE_NS["logger"] = _LOGGER
# seamed module siblings are always recorders (the 33 targets stay real)
for _nm in SEAMED_SIBLINGS:
    _BASE_NS[_nm] = SeamFn(_nm)
install_stubs()
install_path_patches()


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    env = inp.get("env") or {}
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    st = inp.get("state") or {}
    _BASE_NS["_browsers"] = {k: mk_browser(v) for k, v in (st.get("browsers") or {}).items()}
    _BASE_NS["_procs"] = {k: mk_proc(v) for k, v in (st.get("procs") or {}).items()}
    _BASE_NS["_launching"] = {int(k): mk_browser(v) for k, v in (st.get("launching") or {}).items()}
    _BASE_NS["_served_homes"] = set()
    o = types.SimpleNamespace()
    o.getpid = SeamFn("getpid")
    if env.get("has_getuid", True):
        o.getuid = SeamFn("getuid")
    o.path = types.SimpleNamespace(realpath=SeamFn("realpath"), join=posixpath.join)
    _BASE_NS["os"] = o
    _BASE_NS["time"] = types.SimpleNamespace(monotonic=SeamFn("monotonic"))
    _BASE_NS["shutil"] = types.SimpleNamespace(rmtree=SeamFn("rmtree"))
    _BASE_NS["logger"] = _LOGGER
    return _BASE_NS, trace


def browser_arg(ns, inp):
    ref = inp.get("browser_ref")
    if ref is not None:
        return ns["_browsers"].get(ref)
    return mk_browser(inp.get("browser"))


def P(r):
    return r.as_posix() if isinstance(r, PurePath) else r


def drive(rec, ns, inp):
    func = rec["func"]
    br = browser_arg(ns, inp)
    if func == "_ephemeral_root":
        return P(ns[func](inp.get("home", "")))
    if func == "_records_dir":
        return P(ns[func](inp.get("home", "")))
    if func == "_profile_digest":
        return ns[func](inp.get("user_data_dir", ""))
    if func == "_same_profile":
        return ns[func](inp.get("recorded"), inp.get("user_data_dir", ""))
    if func == "_devtools_id":
        return ns[func](br)
    if func == "_record_name":
        return ns[func](br)
    if func == "_shared_profiles_root":
        return P(ns[func]())
    if func == "_is_private_dir":
        return ns[func](Path(inp.get("path", "")))
    if func == "_private_dir":
        return P(ns[func](Path(inp.get("path", ""))))
    if func == "_shared_state_dir":
        return P(ns[func](inp.get("user_data_dir", "")))
    if func == "_state_dir":
        return P(ns[func](br))
    if func == "_record_path":
        return P(ns[func](br))
    if func == "_make_browser":
        return ns[func](inp.get("key", ""), inp.get("port", 0), inp.get("path", ""),
                        pid=inp.get("pid"), start_time=inp.get("start_time"),
                        user_data_dir=inp.get("user_data_dir", ""), owned=inp.get("owned", False),
                        ephemeral=inp.get("ephemeral", False), home_key=inp.get("home_key", ""))
    if func == "_record_is":
        return ns[func](inp.get("record"), br)
    if func == "_owner_alive":
        return ns[func](inp.get("record"))
    if func == "_load_record":
        return ns[func](Path(inp.get("path", "")))
    if func == "_write_record":
        ns[func](br)
        return None
    if func == "_remove_record":
        ns[func](br)
        return None
    if func == "_adopt_record":
        ns[func](br)
        return None
    if func == "_alive":
        return ns[func](br)
    if func == "_foreign_alive":
        return ns[func](br)
    if func == "_same_browser":
        return ns[func](mk_browser(inp.get("a")), mk_browser(inp.get("b")))
    if func == "_heir_key":
        return ns[func](br)
    if func == "_tracked_browser":
        return ns[func](inp.get("record"))
    if func == "_terminate":
        ns[func](inp.get("pid"), inp.get("start_time"), mk_proc(inp.get("proc")))
        return None
    if func == "_forget":
        ns[func](inp.get("key", ""), br, drop_record=inp.get("drop_record", True))
        return None
    if func == "get_browser":
        return ns[func](inp.get("key", ""))
    if func == "_last_holder_out":
        return ns[func](Path(inp.get("record_path", "")), br)
    if func == "_stop_locked":
        ns[func](inp.get("key", ""))
        return None
    if func == "stop_browser":
        ns[func](inp.get("key", ""))
        return None
    if func == "stop_all_browsers":
        ns[func]()
        return None
    if func == "_abandon_launch":
        ns[func](mk_proc(inp.get("proc")), mk_browser(inp.get("starting")))
        return None
    if func == "_launch_owned":
        return ns[func](inp.get("key", ""), inp.get("executable", ""), inp.get("user_data_dir", ""),
                        ephemeral=inp.get("ephemeral", False), packaged=inp.get("packaged", False),
                        headed=inp.get("headed"), extra_args=inp.get("extra_args", ()),
                        env=inp.get("browser_env"), timeout=inp.get("timeout"),
                        home_key=inp.get("home_key", ""), sandbox_bypass=inp.get("sandbox_bypass", True))
    raise KeyError(f"unknown func {func}")


def out_state(ns):
    return {
        "browsers": {k: norm(v) for k, v in ns["_browsers"].items()},
        "procs": {k: norm(v) for k, v in ns["_procs"].items()},
        "launching": {str(k): norm(v) for k, v in ns["_launching"].items()},
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
            ns, trace = build_ns(rec)
            try:
                ret = drive(rec, ns, rec["in"])
                out = {"ret": norm(ret), "trace": trace, "state": out_state(ns)}
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
            ns, trace = build_ns(rec)
            try:
                ret = drive(rec, ns, rec["in"])
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
        fx = _HERE.parent / "fixtures" / "browser_launcher_flow2.jsonl"
        with open(fx, encoding="utf-8") as fh:  # builtin open: Path.read_text is seamed
            recs = [json.loads(l) for l in fh.read().splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()