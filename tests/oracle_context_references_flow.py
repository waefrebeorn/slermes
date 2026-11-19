#!/usr/bin/env python3
"""Oracle for src/agent/context_references_flow.c — drives the LIVE
agent/context_references.py UNCLAIMED FLOW + leaf defs (18 functions) under
IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted in source
order (top-level defs, the nested publish / _next_line / _drain closures, the
ContextCompletionItem.__post_init__ / abstract autocomplete / expand method
bodies, and the three _GIT_REFERENCE_ARGS lambdas); module constants are
seeded (plain-Name Assign, plus CHARS_PER_TOKEN from agent/model_metadata.py).
Ported siblings stay REAL so composition is exercised (_read_file_reference ->
_oversized_text_reference_block -> _on_disk_reference_block; _agent_staged_path
-> _is_under; _expand_path_reference -> the file chain). Claimed siblings
(_resolve_path, _ensure_reference_path_allowed, _is_binary_file,
_binary_reference_block, _build_folder_listing, _agent_visible_path) and
foreign I/O (estimate_tokens_rough, format_bytes, windows_hide_flags,
_hermes_dirs, get_cache_directory_mounts, Path filesystem methods,
subprocess.Popen / wait / kill / read1, readline, offline_file_access) are
recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every except Exception / OSError / LiveConnectionError /
TimeoutExpired guard exactly as live. Per-case BaseException catch ->
__raise__ spec; scripted raises round-trip BARE via identity match against
Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (tagged
with __cr_raw__ so arg projection re-emits the raw wrapper form the C seam
carries); callable doubles {"__callable__": "<seam>"}; paths are POSIX strings
<-> PurePosixPath (ns["Path"] IS PurePosixPath; filesystem methods patched to
per-case seams); Expansion tuples -> lists; integral floats -> ints; object
identity (publish `is not held`) modeled by "token" field equality.

Domain (ASCII scope, documented in the port header): ASCII text; clean
absolute POSIX paths; ref doubles carry all 7 ContextReference fields;
readline/read1 scripts are str pieces (""/null = EOF, read1 materializes as
bytes); stat scripts {"__attr__": {"st_size": int}} or OSError; publish owners
str|None; int() coercion ASCII sign/digits/underscores-between-digits with the
int-strip ws set (excludes \\x1c-\\x1f); IS_WINDOWS parameterized per case;
_run_quiet drains serialize stdout->stderr before wait (Thread double starts
synchronously); kill does not mutate the scripted returncode; CompletedProcess
projects to {args, returncode, stdout, stderr}.

Summary: 'ORACLE context_references_flow: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
import copy
import json
import os
import re as _re
import subprocess as _subprocess
import sys
import threading as _threading
import types
import __future__
from pathlib import Path, PurePosixPath
from types import SimpleNamespace

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "context_references.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "context_references.py")
MM_SRC = str(QUARRY / "agent" / "model_metadata.py")

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "__post_init__", "autocomplete", "expand", "publish",
    "git_args_diff", "git_args_staged", "git_args_git",
    "_expand_path_reference", "_read_file_reference", "_next_line",
    "_format_file_reference", "_run_quiet", "_drain", "_is_under",
    "_composer_paste_roots", "_agent_staged_path",
    "_on_disk_reference_block", "_oversized_text_reference_block",
]

TOP_FUNCS = [
    "_expand_path_reference", "_read_file_reference", "_format_file_reference",
    "_run_quiet", "_is_under", "_composer_paste_roots", "_agent_staged_path",
    "_on_disk_reference_block", "_oversized_text_reference_block",
]
NESTED = {"publish": "register_context_reference_provider",
          "_next_line": "_read_file_reference", "_drain": "_run_quiet"}
METHODS = {"__post_init__": "ContextCompletionItem",
           "autocomplete": "ContextReferenceProvider",
           "expand": "ContextReferenceProvider"}

# module-global seams (imported names / claimed siblings; live name == seam name)
SEAM_GLOBALS = [
    "estimate_tokens_rough", "format_bytes", "windows_hide_flags",
    "_resolve_path", "_ensure_reference_path_allowed", "_is_binary_file",
    "_binary_reference_block", "_build_folder_listing", "_agent_visible_path",
]

WANT_CONST = {
    "_FENCE_LANGUAGES", "_MAX_QUIET_OUTPUT_BYTES",
    "_OUTPUT_CAP_EXCEEDED_RETURNCODE", "COMPOSER_PASTES_DIRNAME",
}
MM_CONST = {"CHARS_PER_TOKEN"}

CURRENT = {"rec": None}
NS = {}


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__seam_name__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = None
    if cls is None:
        if kind == "CancelledError":
            cls = asyncio.CancelledError
        elif kind in ("TimeoutExpired", "SubprocessError"):
            cls = getattr(_subprocess, kind)
        elif kind == "LiveConnectionError":
            cls = NS.get("LiveConnectionError") or type("LiveConnectionError", (RuntimeError,), {})
        else:
            cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    if kind == "TimeoutExpired":
        # real ctor needs (cmd, timeout); the scripted message is irrelevant —
        # live catches by class identity and re-raises a fresh TimeoutExpired
        return cls([], 0)
    return cls(msg)


_MISSING = object()


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def norm_value(v):
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if k != "__cr_raw__"}
    if isinstance(v, PurePosixPath):
        return v.as_posix()
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, bytes):
        return v.decode("utf-8", "replace")
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float) and v == int(v):
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def double_proj(v):
    if isinstance(v, SimpleNamespace) and hasattr(v, "__cr_raw__"):
        return norm_value(v.__cr_raw__)
    return norm_value(v)


def st_proj(v):
    """State-window projection: the MUTATED attrs in raw wrapper form (the
    pristine __cr_raw__ snapshot would lose in-place writes)."""
    if isinstance(v, SimpleNamespace) and hasattr(v, "__cr_raw__"):
        return {"__attr__": {k: norm_value(x) for k, x in vars(v).items() if k != "__cr_raw__"}}
    return norm_value(v)


def posix(v):
    if isinstance(v, PurePosixPath):
        return v.as_posix()
    return v


def proj(name, a, k):
    if name == "estimate_tokens_rough":
        return {"text": a[0]}
    if name == "format_bytes":
        return {"size": a[0]}
    if name in ("windows_hide_flags", "_hermes_dirs", "get_cache_directory_mounts",
                "kill", "offline_file_access.__exit__", "open.__exit__"):
        return {}
    if name == "_resolve_path":
        return {"cwd": posix(a[0]), "target": a[1], "allowed_root": posix(k.get("allowed_root"))}
    if name == "_ensure_reference_path_allowed":
        return {"path": posix(a[0])}
    if name in ("exists", "is_dir", "is_file", "stat", "expanduser", "resolve",
                "_agent_visible_path", "_is_binary_file"):
        return {"path": posix(a[0])}
    if name == "read_text":
        return {"path": posix(a[0]), "encoding": k.get("encoding")}
    if name == "open":
        return {"path": posix(a[0]), "encoding": k.get("encoding")}
    if name == "readline":
        return {"cap": a[0] if a else None}
    if name in ("read1_stdout", "read1_stderr"):
        return {"size": a[0] if a else None}
    if name == "_binary_reference_block":
        return {"ref": double_proj(a[0]), "path": posix(a[1])}
    if name == "_build_folder_listing":
        return {"path": posix(a[0]), "cwd": posix(a[1]), "display_base": posix(k.get("display_base"))}
    if name == "offline_file_access":
        return {"path": posix(a[0]), "what": k.get("what")}
    if name == "wait":
        return {"timeout": k.get("timeout")}
    if name == "popen":
        return {"cmd": norm_value(a[0]), "cwd": posix(k.get("cwd")),
                "stdin": k.get("stdin"), "stdout": k.get("stdout"), "stderr": k.get("stderr"),
                "creationflags": k.get("creationflags"), "env": norm_value(k.get("env"))}
    return {**{("arg%d" % i): v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return materialize_result(name, raw)
        return call


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            ns = SimpleNamespace(**{k: materialize_double(v) for k, v in inner.items()})
            ns.__cr_raw__ = j
            return ns
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def materialize_result(name, raw):
    if name in ("_resolve_path", "expanduser", "resolve"):
        return PurePosixPath(raw) if isinstance(raw, str) else raw
    if name == "_hermes_dirs":
        return [PurePosixPath(x) if isinstance(x, str) else x for x in raw] if isinstance(raw, list) else raw
    if name in ("read1_stdout", "read1_stderr"):
        return raw.encode("utf-8") if isinstance(raw, str) else raw
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw):
        return materialize_double(raw)
    return raw


# ---- PurePosixPath filesystem methods are seams (per-case backend) ----
def _p_exists(self):
    return CURRENT["rec"]("exists")(self)


def _p_is_dir(self):
    return CURRENT["rec"]("is_dir")(self)


def _p_is_file(self):
    return CURRENT["rec"]("is_file")(self)


def _p_stat(self):
    return CURRENT["rec"]("stat")(self)


def _p_read_text(self, encoding=None, errors=None, newline=None):
    return CURRENT["rec"]("read_text")(self, encoding=encoding)


def _p_expanduser(self):
    return CURRENT["rec"]("expanduser")(self)


def _p_resolve(self, strict=False):
    return CURRENT["rec"]("resolve")(self)


class _FileCM:
    """path.open(...) result: the open seam fires at call time; __enter__
    hands back the readline double; __exit__ fires open.__exit__."""
    def __init__(self, fh):
        self.fh = fh

    def __enter__(self):
        return self.fh

    def __exit__(self, *a):
        r = CURRENT["rec"]("open.__exit__")()
        return False


def _p_open(self, mode="r", buffering=-1, encoding=None, errors=None, newline=None):
    CURRENT["rec"]("open")(self, encoding=encoding)
    fh = SimpleNamespace(readline=CURRENT["rec"]("readline"))
    return _FileCM(fh)


PurePosixPath.exists = _p_exists
PurePosixPath.is_dir = _p_is_dir
PurePosixPath.is_file = _p_is_file
PurePosixPath.stat = _p_stat
PurePosixPath.read_text = _p_read_text
PurePosixPath.expanduser = _p_expanduser
PurePosixPath.resolve = _p_resolve
PurePosixPath.open = _p_open


class _CMStub:
    def __init__(self, name, a, k):
        self.name = name
        self.r = CURRENT["rec"](name)(*a, **k)

    def __enter__(self):
        return self.r

    def __exit__(self, *a):
        CURRENT["rec"](self.name + ".__exit__")()
        return False


def _cm_factory(name):
    return lambda *a, **k: _CMStub(name, a, k)


class _FakeThread:
    """Deterministic Thread double: start() runs the target synchronously
    (the C port serializes drains stdout->stderr before wait)."""
    def __init__(self, group=None, target=None, name=None, args=(), kwargs=None, daemon=None):
        self._target = target
        self._args = args
        self._kwargs = kwargs or {}

    def start(self):
        if self._target:
            self._target(*self._args, **self._kwargs)

    def join(self, timeout=None):
        pass


def _popen_stub(*a, **k):
    return CURRENT["rec"]("popen")(*a, **k)


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _assign_consts(src, path, want, ns):
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if names & want:
                _exec_seg(src, node, ns, path)
        elif isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name):
            if node.target.id in want:
                _exec_seg(src, node, ns, path)


def _strip_decorators(node):
    n = copy.copy(node)
    n.decorator_list = []
    return n


def load_ns():
    global NS
    mod = types.ModuleType("cr_live")
    sys.modules["cr_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cr_live", "json": json, "re": _re,
        "Path": PurePosixPath, "PurePosixPath": PurePosixPath,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    _assign_consts(src, SRC, WANT_CONST, ns)
    msrc = Path(MM_SRC).read_text(encoding="utf-8")
    _assign_consts(msrc, MM_SRC, MM_CONST, ns)
    tree = ast.parse(src)
    found = set()
    # top-level target defs (source order)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TOP_FUNCS:
            _exec_seg(src, node, ns, SRC)
            found.add(node.name)
    # nested closures + class methods + the git-argv lambdas
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in NESTED.values():
            for child in ast.walk(node):
                if isinstance(child, ast.FunctionDef) and child.name in NESTED and NESTED[child.name] == node.name:
                    _exec_seg(src, child, ns, SRC)
                    found.add(child.name)
        if isinstance(node, ast.ClassDef) and node.name in METHODS.values():
            for child in node.body:
                if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)) and child.name in METHODS and METHODS[child.name] == node.name:
                    _exec_seg(src, _strip_decorators(child), ns, SRC)
                    found.add(child.name)
        git_assign = None
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if "_GIT_REFERENCE_ARGS" in names:
                git_assign = node.value
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) and node.target.id == "_GIT_REFERENCE_ARGS":
            git_assign = node.value
        if git_assign is not None and isinstance(git_assign, ast.Dict):
            for key, val in zip(git_assign.keys, git_assign.values):
                if isinstance(key, ast.Constant) and isinstance(val, ast.Lambda):
                    fname = "git_args_" + key.value
                    seg = ast.get_source_segment(src, val)
                    exec(compile(fname + " = " + seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
                    found.add(fname)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    for nm in sorted(WANT_CONST | MM_CONST):
        if nm not in ns:
            raise RuntimeError("constant not seeded: %s" % nm)
    # module-global seams
    for nm in SEAM_GLOBALS:
        ns[nm] = _stub_fn(nm)
    # offline_file_access CM + LiveConnectionError identity class
    lce = type("LiveConnectionError", (RuntimeError,), {})
    ns["LiveConnectionError"] = lce
    ns["offline_file_access"] = _cm_factory("offline_file_access")
    # subprocess double (real exception classes; Popen is a seam)
    ns["subprocess"] = SimpleNamespace(
        Popen=_popen_stub, DEVNULL=_subprocess.DEVNULL, PIPE=_subprocess.PIPE,
        TimeoutExpired=_subprocess.TimeoutExpired, CompletedProcess=_subprocess.CompletedProcess,
        SubprocessError=_subprocess.SubprocessError,
    )
    # threading double (real Event; synchronous Thread)
    ns["threading"] = SimpleNamespace(Event=_threading.Event, Thread=_FakeThread, Lock=_threading.Lock)
    ns["IS_WINDOWS"] = True
    # in-function import stubs (parent packages linked; None halts the real
    # editable-install finder from resolving the quarry submodule)
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    fs = types.ModuleType("agent.file_safety")
    fs._hermes_dirs = _stub_fn("_hermes_dirs")
    sys.modules["agent.file_safety"] = fs
    agent_pkg.file_safety = fs
    tools_pkg = types.ModuleType("tools"); tools_pkg.__path__ = []
    sys.modules["tools"] = tools_pkg
    cf = types.ModuleType("tools.credential_files")
    cf.get_cache_directory_mounts = _stub_fn("get_cache_directory_mounts")
    sys.modules["tools.credential_files"] = cf
    tools_pkg.credential_files = cf
    hc_pkg = types.ModuleType("hermes_cli"); hc_pkg.__path__ = []
    sys.modules["hermes_cli"] = hc_pkg
    sr = types.ModuleType("hermes_cli.sqlite_safe_read")
    sr.LiveConnectionError = lce
    sr.offline_file_access = ns["offline_file_access"]
    sys.modules["hermes_cli.sqlite_safe_read"] = sr
    hc_pkg.sqlite_safe_read = sr
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def _ref(inp):
    return materialize_double(inp.get("ref"))


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    if func == "__post_init__":
        slf = materialize_double(_state(inp, "self"))
        NS["__post_init__"](slf)
        return None, {"self": st_proj(slf)}
    if func == "autocomplete":
        slf = materialize_double(inp.get("self"))
        return asyncio.run(NS["autocomplete"](slf, inp.get("query", ""), limit=inp.get("limit", 10))), {}
    if func == "expand":
        slf = materialize_double(inp.get("self"))
        return asyncio.run(NS["expand"](slf, inp.get("target", ""))), {}
    if func == "publish":
        prefix = inp.get("prefix", "")
        providers_raw = _state(inp, "providers") or {}
        owners = _state(inp, "owners") or {}
        registry = {k: materialize_double(v) for k, v in providers_raw.items()}
        held_in = inp.get("held")
        held = None
        cur = registry.get(prefix)
        if cur is not None and isinstance(held_in, dict):
            ht = (held_in.get("__attr__") or {}).get("token")
            if ht is not None and getattr(cur, "token", None) == ht:
                held = cur
        if held is None:
            held = materialize_double(held_in)
        provider = materialize_double(inp.get("provider"))
        NS["prefix"] = prefix
        NS["held"] = held
        NS["provider"] = provider
        NS["owner"] = inp.get("owner")
        NS["_context_reference_providers"] = registry
        NS["_context_reference_owners"] = owners
        NS["_context_reference_lock"] = _threading.Lock()
        ret = NS["publish"]()
        return ret, {"providers": {k: st_proj(v) for k, v in registry.items()}, "owners": owners}
    if func in ("git_args_diff", "git_args_staged", "git_args_git"):
        return NS[func](_ref(inp)), {}
    if func == "_expand_path_reference":
        cwd = PurePosixPath(inp.get("cwd", "/"))
        ar = inp.get("allowed_root")
        ar = PurePosixPath(ar) if ar is not None else None
        ret = NS[func](_ref(inp), cwd, allowed_root=ar, max_inline_tokens=inp.get("max_inline_tokens"))
        return ret, {}
    if func == "_read_file_reference":
        return NS[func](_ref(inp), PurePosixPath(inp.get("path", "/")), inp.get("max_inline_tokens")), {}
    if func == "_next_line":
        fh = materialize_double(inp.get("fh"))
        NS["line_cap"] = inp.get("line_cap")
        return NS["_next_line"](fh, bool(inp.get("collect")), inp.get("remaining")), {}
    if func == "_format_file_reference":
        return NS[func](_ref(inp), PurePosixPath(inp.get("path", "/")),
                        inp.get("text"), inp.get("max_inline_tokens")), {}
    if func == "_run_quiet":
        NS["IS_WINDOWS"] = bool(inp.get("is_windows", True))
        cmd = inp.get("cmd") or []
        cwd = PurePosixPath(inp.get("cwd", "/"))
        ret = NS[func](cmd, cwd, inp.get("timeout", 30), env=inp.get("env"))
        if isinstance(ret, _subprocess.CompletedProcess):
            ret = {"args": ret.args, "returncode": ret.returncode,
                   "stdout": ret.stdout, "stderr": ret.stderr}
        return ret, {}
    if func == "_drain":
        stream_name = inp.get("stream", "stdout")
        stream = SimpleNamespace(read1=CURRENT["rec"]("read1_" + stream_name))
        proc = materialize_double(inp.get("proc"))
        truncated = _threading.Event()
        NS["proc"] = proc
        NS["truncated"] = truncated
        sink = []
        NS["_drain"](stream, sink)
        return None, {"sink": sink, "truncated": truncated.is_set()}
    if func == "_is_under":
        return NS[func](PurePosixPath(inp.get("path", "/")), PurePosixPath(inp.get("root", "/"))), {}
    if func == "_composer_paste_roots":
        return NS[func](), {}
    if func == "_agent_staged_path":
        return NS[func](PurePosixPath(inp.get("path", "/"))), {}
    if func == "_on_disk_reference_block":
        return NS[func](_ref(inp), PurePosixPath(inp.get("path", "/")),
                        inp.get("descriptor", ""), inp.get("reason", ""), inp.get("guidance", "")), {}
    if func == "_oversized_text_reference_block":
        return NS[func](_ref(inp), PurePosixPath(inp.get("path", "/")), inp.get("text_tokens", 0)), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


def main():
    try:
        load_ns()
    except BaseException as e:
        print("PYERR load_ns: %s: %s" % (type(e).__name__, e))
        return 1
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE context_references_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())