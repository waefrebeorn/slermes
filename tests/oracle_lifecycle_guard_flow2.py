#!/usr/bin/env python3
"""Oracle for the cron/lifecycle_guard.py UNCLAIMED-remainder FLOW port
(src/cron/lifecycle_guard_flow2.c).

Drives the LIVE module defs with SCRIPTED seams: claimed siblings, stdlib
operations (shlex / Path / os / stat), heredoc helpers, sqlite-safe-read and
logger are recording stubs fed the SAME scripts the C harness feeds lg2_call.
Compares the DECISION SEQUENCE (ordered {ev,args,ret} trace), the canonical
return and the post-call state carrier. Runs with an UNTOUCHED runner.
"""
import builtins
import importlib.util
import json
import math
import os
import re
import shlex as _real_shlex
import signal
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
_REPO = _HERE.parent.parent
_QUARRY = _HERE.parents[2] / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(_QUARRY / "cron" / "lifecycle_guard.py")
if not Path(SRC).is_file():
    SRC = str(Path.home() / "hermes-agent" / "cron" / "lifecycle_guard.py")

NAME = "lifecycle_guard_flow2"
LIVE = None
TRACE = []
SCRIPTS = {}
CURS = {}
HOLDER = {}
BASE = {}


# ── load the live module by path (no cron/__init__ import chain) ────────────
def load_live():
    spec = importlib.util.spec_from_file_location("lg2_live_lifecycle_guard", SRC)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


# ── canonical projection ───────────────────────────────────────────────────
def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if v is None or isinstance(v, bool) or isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        if isinstance(v, float):
            if not math.isfinite(v):
                return str(v)
            if v == int(v):
                return int(v)
        return v
    if isinstance(v, (PurePath, PathDouble)):
        return str(v)
    if isinstance(v, (list, tuple, set, frozenset)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {(norm(k) if isinstance(k, (PurePath, PathDouble)) else k): norm(x) for k, x in v.items()}
    return str(v)


class ScriptRaise(Exception):
    def __init__(self, spec):
        self.spec = spec
        super().__init__(spec.get("msg", ""))


def _make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    builtin = getattr(builtins, cls, None)
    if isinstance(builtin, type) and issubclass(builtin, BaseException):
        if cls in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
            e = builtin.__new__(builtin)
            Exception.__init__(e, msg)
            return e
        try:
            return builtin(msg)
        except Exception:
            e = builtin.__new__(builtin)
            Exception.__init__(e, msg)
            return e
    return type(cls, (Exception,), {})(msg)


def next_script(name):
    """Return (value, spec-or-None). Absent -> (None, None); raise -> spec."""
    if name not in SCRIPTS:
        return None, None
    val = SCRIPTS[name]
    if isinstance(val, dict) and "$seq" in val:
        seq = val["$seq"]
        i = int(CURS.get(name, 0))
        CURS[name] = i + 1
        if i >= len(seq):
            return None, {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
        val = seq[i]
    if isinstance(val, dict) and "__raise__" in val:
        return None, val
    return val, None


def record(name, args_builder, cast=None):
    """Build a recorder; args_builder(*a, **kw) -> dict of seam args."""

    def fn(*a, **kw):
        raw = args_builder(*a, **kw) or {}
        args = {k: norm(v) for k, v in raw.items() if v is not None}
        val, spec = next_script(name)
        TRACE.append({"ev": name, "args": args, "ret": norm(spec) if spec is not None else norm(val)})
        if spec is not None:
            raise _make_exc(spec)
        if cast is not None:
            return cast(val)
        return val

    return fn


def list_of_strings(val):
    if val is None:
        return []
    if isinstance(val, list):
        return [str(x) for x in val]
    return []


def segments_cast(val):
    if not isinstance(val, list):
        return []
    return [[str(t) for t in seg] if isinstance(seg, list) else [] for seg in val]


# ── Path / os / stat doubles ───────────────────────────────────────────────
class PathDouble:
    """Minimal path double: verbatim string spelling + seamed OS operations."""

    def __init__(self, s=""):
        self.s = str(s)

    def __eq__(self, other):
        return isinstance(other, PathDouble) and self.s == other.s

    def __hash__(self):
        return hash(self.s)

    def __str__(self):
        return self.s

    def __repr__(self):
        return f"PathDouble({self.s!r})"

    def __truediv__(self, other):
        o = str(other)
        return PathDouble(self.s.rstrip("/") + "/" + o.lstrip("/") if self.s else o)

    @property
    def parts(self):
        return tuple(x for x in self.s.split("/") if x) if self.s else ()

    @property
    def name(self):
        b = self.s.rstrip("/").split("/")[-1] if self.s else ""
        return "" if b in (".", "..") else b

    @property
    def suffix(self):
        b = self.name
        i = b.rfind(".")
        return b[i:] if i > 0 else ""

    @property
    def parent(self):
        s = self.s.rstrip("/")
        return PathDouble(s.rsplit("/", 1)[0] if "/" in s else "")

    def is_absolute(self):
        return self.s.startswith("/")

    def resolve(self, strict=False):
        return _path_rec("path.resolve", self.s)

    def expanduser(self):
        return _path_rec("path.expanduser", self.s)

    @classmethod
    def cwd(cls):
        return _path_rec("path.cwd", None)


def _path_rec(name, path):
    if path is None:
        val, spec = next_script(name)
        TRACE.append({"ev": name, "args": {}, "ret": norm(spec) if spec is not None else norm(val)})
    else:
        val, spec = next_script(name)
        TRACE.append({"ev": name, "args": {"path": path}, "ret": norm(spec) if spec is not None else norm(val)})
    if spec is not None:
        raise _make_exc(spec)
    return PathDouble(str(val) if val is not None else "")


class _OSDouble:
    O_RDONLY = os.O_RDONLY
    O_NONBLOCK = getattr(os, "O_NONBLOCK", 0)

    def stat(self, path):
        def ab():
            return {"path": str(path)}
        val = record("os.stat", ab)()
        if isinstance(val, dict):
            return types.SimpleNamespace(**val)
        return val

    def open(self, path, flags):
        def ab():
            return {"path": str(path), "flags": flags}
        return record("os.open", ab)()

    def fstat(self, fd):
        val = record("os.fstat", lambda: {"fd": fd})()
        if isinstance(val, dict):
            return types.SimpleNamespace(**val)
        return val

    def read(self, fd, n):
        val, spec = next_script("os.read")
        TRACE.append({"ev": "os.read", "args": {"fd": fd, "n": n}, "ret": norm(spec) if spec is not None else norm(val)})
        if spec is not None:
            raise _make_exc(spec)
        if val is None:
            return b""
        if isinstance(val, str):
            return val.encode("latin-1", "replace")
        return bytes(val)

    def close(self, fd):
        return record("os.close", lambda: {"fd": fd})()


class _StatDouble:
    @staticmethod
    def _mode(x):
        if isinstance(x, int):
            return x
        try:
            return int(x.st_mode)
        except Exception:
            return 0o100000

    @staticmethod
    def S_ISREG(x):
        return (_StatDouble._mode(x) & 0o170000) == 0o100000

    @staticmethod
    def S_ISDIR(x):
        return (_StatDouble._mode(x) & 0o170000) == 0o040000


class _LoggerDouble:
    def warning(self, msg, *args, **kwargs):
        a = [norm(x) if not isinstance(x, BaseException) else str(x) for x in args]
        d = {"msg": msg, "args": a}
        if kwargs.get("exc_info"):
            d["exc_info"] = True
        val, spec = next_script("logger.warning")
        TRACE.append({"ev": "logger.warning", "args": d, "ret": norm(spec) if spec is not None else norm(val)})
        if spec is not None:
            raise _make_exc(spec)

    def debug(self, msg, *args, **kwargs):
        self.warning(msg, *args, **kwargs)


# ── per-case namespace setup ───────────────────────────────────────────────
CLAIMED = [
    "contains_gateway_lifecycle_command", "contains_launchctl_submit_command",
    "_iter_command_segments", "_executable_name", "_command_token_index",
    "_peel_transparent_prefixes", "_direct_lifecycle_scan", "_contains_unsafe_gateway_action",
    "lifecycle_command_scan_with_data_exemption", "mask_data_sink_arguments",
    "_read_referenced_script", "_resolve_script_path", "_resolve_script_directory",
    "_sanitize_remote_script_text", "_iter_referenced_shell_scripts",
]


def _shlex_double(line, *, posix=True, punctuation_chars=None):
    # AST drives the real _shlex_tokens; patch shlex.shlex to emit scripted tokens.

    class _Lex:
        def __init__(self, *a, **kw):
            self._line = line

        def __iter__(self):
            val, spec = next_script("shlex_tokens")
            TRACE.append({"ev": "shlex_tokens",
                          "args": {"line": line, "punctuation": punctuation_chars if punctuation_chars is not None else ";&|()"},
                          "ret": norm(spec) if spec is not None else norm(val)})
            if spec is not None:
                raise _make_exc(spec)
            return iter(list_of_strings(val))

    return _Lex()


def install(target):
    global TRACE, SCRIPTS, CURS, HOLDER, BASE
    TRACE = []
    CURS = {}
    HOLDER = {}
    # stubs for in-function imports
    sh = types.ModuleType("tools.shell_heredoc")
    sh.strip_inert_heredoc_bodies = record(
        "strip_inert_heredoc_bodies",
        lambda text, mask_code_consumers=True: {"text": text},
    )
    sh.iter_heredoc_bodies = record(
        "iter_heredoc_bodies",
        lambda command: {"command": command},
        lambda v: [tuple(x) for x in v] if isinstance(v, list) else [],
    )
    tools_pkg = types.ModuleType("tools")
    tools_pkg.__path__ = []
    tools_pkg.shell_heredoc = sh
    sys.modules["tools"] = tools_pkg
    sys.modules["tools.shell_heredoc"] = sh

    sq = types.ModuleType("hermes_cli.sqlite_safe_read")
    sq.has_live_connection = record("has_live_connection", lambda path: {"path": str(path)})
    class LiveConnectionError(Exception):
        pass
    sq.LiveConnectionError = LiveConnectionError
    sq.offline_file_access = _real_shlex  # replaced below by a contextmanager double

    class _Offline:
        def __init__(self, path, what=""):
            self.path = path

        def __enter__(self):
            return self

        def __exit__(self, *a):
            return False
    sq.offline_file_access = lambda path, what="": _Offline(path, what)
    hp = types.ModuleType("hermes_cli")
    hp.__path__ = []
    hp.sqlite_safe_read = sq
    sys.modules["hermes_cli"] = hp
    sys.modules["hermes_cli.sqlite_safe_read"] = sq

    hc = types.ModuleType("hermes_constants")
    hc.get_hermes_home = lambda: Path(os.environ.get("HERMES_HOME", "."))
    sys.modules["hermes_constants"] = hc

    # recorders on claimed module globals
    def one(name):
        return record(name, lambda *a, **kw: {("segment" if len(a) == 1 else "command"): (a[0] if a else kw.get("command", ""))})

    LIVE._iter_command_segments = record("iter_command_segments", lambda command: {"command": command}, segments_cast)
    LIVE._executable_name = record("executable_name", lambda token: {"token": token})
    LIVE._command_token_index = record("command_token_index", lambda segment: {"segment": segment})
    LIVE._peel_transparent_prefixes = record("peel_transparent_prefixes", lambda segment, index: {"segment": segment})
    LIVE.contains_gateway_lifecycle_command = record("contains_gateway_lifecycle_command", lambda text: {"text": text})
    LIVE.contains_launchctl_submit_command = record("contains_launchctl_submit_command", lambda command: {"command": command})
    LIVE._direct_lifecycle_scan = record("_direct_lifecycle_scan", lambda command: {"command": command})
    LIVE._contains_unsafe_gateway_action = record(
        "_contains_unsafe_gateway_action",
        lambda command, *, cwd=None, depth=0, **kw: {"command": command, "cwd": cwd, "depth": depth},
        lambda v: v,
    )
    LIVE.lifecycle_command_scan_with_data_exemption = record(
        "lifecycle_command_scan_with_data_exemption", lambda text: {"text": text})
    LIVE.mask_data_sink_arguments = record("mask_data_sink_arguments", lambda text: {"text": text})
    LIVE._read_referenced_script = record(
        "_read_referenced_script", lambda path, *, max_bytes=None: {"path": str(path), "max_bytes": max_bytes})
    LIVE._resolve_script_path = record("_resolve_script_path", lambda script_path: {"script_path": script_path})
    LIVE._resolve_script_directory = record("_resolve_script_directory", lambda script_path: {"script_path": script_path})
    LIVE._sanitize_remote_script_text = record(
        "_sanitize_remote_script_text", lambda text, *, max_bytes=None: {"text": text, "max_bytes": max_bytes})
    LIVE._iter_referenced_shell_scripts = record(
        "iter_referenced_shell_scripts", lambda command, *, cwd=None: {"command": command, "cwd": cwd},
        list_of_strings,
    )
    LIVE.logger = _LoggerDouble()
    LIVE.os = _OSDouble()
    LIVE.stat = _StatDouble()
    LIVE.Path = PathDouble
    # shlex: only drive the double when the target itself is _shlex_tokens
    if target == "_shlex_tokens":
        sys.modules["shlex"] = types.SimpleNamespace(shlex=_shlex_double)
    else:
        sys.modules["shlex"] = _real_shlex
        LIVE._shlex_tokens = record("shlex_tokens", lambda line, punctuation_chars=";&|()": {
            "line": line, "punctuation": punctuation_chars}, list_of_strings)


def drive(rec, target):
    inp = rec.get("in", {})
    SCRIPTS = dict(inp.get("io", {}) or {})
    globals()["SCRIPTS"] = SCRIPTS
    env = inp.get("env", {}) or {}
    # restore module constants to their pristine values before seeding this case
    for nm, val in BASE.items():
        setattr(LIVE, nm, val)
    # seed module constants from env (mirrors C env reads)
    if "host_interpreter_name" in env:
        LIVE._HOST_INTERPRETER_NAME = env["host_interpreter_name"]
    for nm, key in (("_MAX_LIFECYCLE_SCAN_BYTES", "max_scan_bytes"),
                    ("_MAX_LIFECYCLE_SCAN_LINES", "max_scan_lines"),
                    ("_MAX_LIFECYCLE_SCAN_PATHS", "max_scan_paths"),
                    ("_MAX_LIFECYCLE_SCAN_REMOTE_READS", "max_remote_reads"),
                    ("_MAX_LIFECYCLE_SCAN_LINE_BYTES", "max_line_bytes")):
        if key in env:
            setattr(LIVE, nm, int(env[key]))
    install(target)
    a = {k: v for k, v in inp.items() if k not in ("io", "env")}
    fn = getattr(LIVE, target, None)
    if target in ("__init__", "charge_text", "charge_path", "charge_remote_read"):
        b = LIVE._LifecycleScanBudget() if target == "__init__" else _budget_from(a.get("budget"))
        HOLDER["state"] = norm_budget(b)
        if target == "__init__":
            return norm_budget(b)
        if target == "charge_text":
            r = b.charge_text(a.get("text", ""), written_body=bool(a.get("written_body", False)))
        elif target == "charge_path":
            r = b.charge_path()
        else:
            r = b.charge_remote_read()
        HOLDER["state"] = norm_budget(b)
        return norm(r)
    if target == "_budget_exhausted":
        b = _budget_from(a.get("budget"))
        r = LIVE._budget_exhausted(b, a.get("what", "text"), int(a.get("depth", 0)))
        HOLDER["state"] = norm_budget(b)
        return norm(r)
    if target == "_refuse_unreadable":
        b = _budget_from(a.get("budget"))
        r = LIVE._refuse_unreadable(b, PathDouble(str(a.get("path", ""))), a.get("reason", ""))
        HOLDER["state"] = norm_budget(b)
        return norm(r)
    if target == "_collect_heredoc_written_scripts":
        written = a.get("written")
        wd = {}
        if isinstance(written, dict):
            wd = {PathDouble(k): list(v) for k, v in written.items()}
        r = LIVE._collect_heredoc_written_scripts(
            a.get("command", ""), a.get("cwd"), wd if written is not None else None)
        HOLDER["state"] = {str(k): v for k, v in wd.items()}
        return norm(r)
    if target == "_scan_written_bodies":
        HOLDER["state"] = {}
        recurse = record("recurse", lambda text, cwd=None, executed=True, charge=True: {
            "text": text, "cwd": cwd, "executed": executed, "charge": charge})
        return norm(LIVE._scan_written_bodies(a.get("bodies", []), recurse,
                                              a.get("cwd"), bool(a.get("executed", True))))
    if target == "_capped_read_limit":
        return norm(LIVE._capped_read_limit(a.get("max_bytes")))
    if target == "lifecycle_scan_root_within_budget":
        return norm(LIVE.lifecycle_scan_root_within_budget(a.get("text", "")))
    if target == "_is_interpreter_image":
        return norm(fn(a.get("value", ""), substring=bool(a.get("substring", False))))
    if target == "_pattern_reaches_host_interpreter":
        return norm(fn(a.get("pattern", ""), full_cmdline=bool(a.get("full_cmdline", False)),
                       exact=bool(a.get("exact", False))))
    if target == "_killer_targets_host_interpreter":
        return norm(fn(a.get("name", ""), list(a.get("args", []))))
    if target == "_segment_names_host_interpreter":
        return norm(fn(list(a.get("tokens", [])), frozenset({"pkill","killall","taskkill","stop-process"}
                    if a.get("killers") else {"pgrep","pidof","get-process"})))
    if target == "_shlex_tokens":
        return norm(fn(a.get("line", ""), a.get("punctuation", ";&|()")))
    if target == "_split_segments":
        return norm(list(fn(list(a.get("tokens", [])), keep_controls=bool(a.get("keep_controls", False)))))
    if target == "_executed_command_index":
        r = fn(list(a.get("segment", [])))
        return norm(r)
    if target == "_is_nonlethal_signal":
        return norm(fn(a.get("spec", "")))
    if target == "_is_nonlethal_signal_option":
        return norm(fn(a.get("arg", "")))
    if target == "_is_pid_operand":
        return norm(fn(a.get("token", ""), int(a.get("pid", 0)), group=bool(a.get("group", False))))
    if target == "_kill_targets_pid":
        return norm(fn(list(a.get("args", [])), int(a.get("pid", 0))))
    if target == "_preview_words":
        return norm(fn(a.get("command", "")))
    if target == "_is_stop_process_whatif_preview":
        return norm(fn(a.get("command", "")))
    if target == "_windows_killer_targets_pid":
        return norm(fn(a.get("name", ""), list(a.get("args", [])), int(a.get("pid", 0)),
                       whatif_preview=bool(a.get("whatif_preview", False))))
    if target == "_string_runner_payload":
        return norm(fn(a.get("name", ""), list(a.get("args", []))))
    if target == "_targets_gateway_pid":
        return norm(fn(a.get("command", ""), int(a.get("pid", 0)), depth=int(a.get("depth", 0))))
    if target == "_resolve_lenient":
        return norm(fn(PathDouble(str(a.get("path", "")))))
    if target == "_is_cloud_placeholder_path":
        return norm(fn(PathDouble(str(a.get("path", "")))))
    if target == "_on_cloud_path":
        return norm(fn(PathDouble(str(a.get("path", "")))))
    if target == "_expand_candidate_path":
        r = fn(a.get("candidate", ""))
        return norm(r) if r is not None else None
    if target == "_resolved_or_nothing":
        return norm([str(x) for x in fn(a.get("candidate", ""), a.get("cwd"))])
    if target == "_heredoc_write_targets":
        return norm(list(fn(a.get("unit", ""))))
    if target == "_walk_candidates":
        return norm([[str(p), bool(e)] for p, e in fn(a.get("command", ""), a.get("walk_command", ""),
                     a.get("cwd"), bool(a.get("executed", True)))])
    if target == "_has_binary_magic":
        return norm(fn(str(a.get("data", "")).encode("latin-1", "replace")))
    if target == "_read_referenced_script_unlocked":
        r = fn(PathDouble(str(a.get("path", ""))), max_bytes=a.get("max_bytes"))
        return norm([r[0], r[1]])
    if target == "scan_gateway_lifecycle":
        r = fn(a.get("command", ""), cwd=a.get("cwd"),
               read_remote_script=None, gateway_pid=a.get("gateway_pid"))
        return norm([r[0], r[1]])
    if target == "_split_logical_lines":
        return norm(list(fn(a.get("text", ""))))
    if target in ("contains_host_interpreter_kill", "first_gateway_lifecycle_match",
                  "_iter_command_segments"):
        return norm(fn(a.get("text", a.get("command", ""))))
    HOLDER["state"] = {}
    return norm(fn(**a))


def norm_budget(b):
    return {
        "bytes_remaining": b.bytes_remaining,
        "lines_remaining": b.lines_remaining,
        "paths_remaining": b.paths_remaining,
        "remote_reads_remaining": b.remote_reads_remaining,
        "refusal": b.refusal,
        "scanned_bodies": {str(x): True for x in (b.scanned_bodies.keys() if isinstance(b.scanned_bodies, dict) else b.scanned_bodies)},
    }


def _budget_from(d):
    b = LIVE._LifecycleScanBudget()
    if not isinstance(d, dict):
        return b
    for k, v in d.items():
        if k == "scanned_bodies":
            b.scanned_bodies = set(v or [])
        elif hasattr(b, k):
            setattr(b, k, v)
    return b


# ── main: read harness output, drive live, compare ─────────────────────────
def main():
    global LIVE
    LIVE = load_live()
    for nm in ("_MAX_LIFECYCLE_SCAN_BYTES", "_MAX_LIFECYCLE_SCAN_LINES",
               "_MAX_LIFECYCLE_SCAN_PATHS", "_MAX_LIFECYCLE_SCAN_REMOTE_READS",
               "_MAX_LIFECYCLE_SCAN_LINE_BYTES", "_HOST_INTERPRETER_NAME"):
        BASE[nm] = getattr(LIVE, nm)
    n = 0
    mism = 0
    shown = 0
    for line in sys.stdin:
        if not line.strip():
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        try:
            ret = drive(rec, rec.get("func", ""))
            out = {"ret": ret, "trace": TRACE, "state": HOLDER.get("state", {})}
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)},
                   "trace": TRACE, "state": HOLDER.get("state", {})}
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:1000]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:1000]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
