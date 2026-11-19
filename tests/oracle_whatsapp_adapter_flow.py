#!/usr/bin/env python3
"""Oracle for the WhatsApp adapter bridge-lifecycle FLOW port
(src/gateway/platforms/whatsapp_adapter_flow.c).

Drives the LIVE plugins/platforms/whatsapp/adapter.py process-management
tree with SCRIPTED seams: every SEAMED sibling call (and every foreign
import: subprocess / psutil / time / gateway.status / hermes_cli.* / pm /
os.kill / os.chmod / logger) is a recording stub fed the same scripts the C
harness feeds its universal seam wa_call. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret"); state is always {} (no by-reference accumulators in this cluster).

The live defs are AST-extracted from adapter.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; MessageType is a synthetic
enum with the quarry member values; logger re-set AFTER seeding). Every
exec namespace is a registered sys.modules module (py3.14 dataclasses).
The heavy quarry import chain is never imported; in-function imports bind
stub modules installed below. Paths are pathlib.PurePosixPath doubles whose
file ops record through the same Ctx (fixture domain: lexical POSIX joins,
no resolve()/case-folding; read_text is always utf-8-sig -> one leading BOM
stripped on both sides; read_bytes results are UTF-8 strings hashed as bytes).

Mode table (mirrors the C port): REAL-ALWAYS leaves run inline (_safe_ints,
_fmt_uptime, _bridge_auth_headers, _bridge_media_type, _bridge_auth_proof,
_bridge_config_fingerprint, _classify_bridge_message); every other ported
function is SEAMED when called by a sibling and REAL when the case target.
_IS_WINDOWS / _SIGKILL are overridden per case from args.is_windows
(SIGTERM=15, SIGKILL=9 POSIX / 15 Windows).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE whatsapp_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- MessageType members project to their .value strings.
- Integral floats == ints (canon folds). Tuples project to lists.
- subprocess.run / psutil results are attribute doubles (SimpleNamespace);
  a missing key models a missing attribute (py3.14 prefixed message).
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import asyncio
import copy
import enum
import json
import os
import re
import subprocess
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "whatsapp", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_safe_ints", "_fmt_uptime", "_bridge_auth_headers", "_bridge_media_type",
    "_bridge_auth_proof", "_bridge_config_fingerprint", "_classify_bridge_message",
    "_wenv", "_is_connected", "_cache_dirs", "_unlink_quietly", "_tighten_bridge_log",
    "_read_bridge_token", "_write_bridge_fingerprint", "_write_bridge_pidfile",
    "_read_bridge_pidfile", "_bridge_pid_is_ours", "_pids_alive", "_wait_pids_exit",
    "_pid_looks_like_node_bridge", "_windows_listener_pids", "_listener_pids_on_port",
    "_port_listener_pids", "_kill_port_process", "_kill_stale_bridge_by_pidfile",
    "check_whatsapp_requirements", "_file_content_hash", "_bridge_source_hash",
    "_spawn_bridge_process", "_normalize_outbound_mentions",
]
REAL_ALWAYS = {
    "_safe_ints", "_fmt_uptime", "_bridge_auth_headers", "_bridge_media_type",
    "_bridge_auth_proof", "_bridge_config_fingerprint", "_classify_bridge_message",
}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


class MessageType(enum.Enum):
    TEXT = "text"
    LOCATION = "location"
    PHOTO = "photo"
    VIDEO = "video"
    AUDIO = "audio"
    VOICE = "voice"
    DOCUMENT = "document"
    STICKER = "sticker"
    COMMAND = "command"


# ── Path double: PurePosixPath + recording file ops (universal seam) ──
class PathDouble(PurePosixPath):
    CTX = None

    def exists(self):
        return self.CTX.call("exists", {"path": str(self)})

    def read_text(self, encoding=None):
        r = self.CTX.call("read_text", {"path": str(self), "encoding": encoding})
        if isinstance(r, str) and encoding == "utf-8-sig" and r.startswith("\ufeff"):
            r = r[1:]
        return r

    def write_text(self, text, encoding=None):
        return self.CTX.call("write_text", {"path": str(self), "text": text, "encoding": encoding})

    def unlink(self):
        return self.CTX.call("unlink", {"path": str(self)})

    def read_bytes(self):
        r = self.CTX.call("read_bytes", {"path": str(self)})
        return r.encode("utf-8") if isinstance(r, str) else r


def extract():
    mod = new_module("wa_oracle_adapter")
    ns = mod.__dict__
    import contextlib
    import functools
    import hashlib
    import hmac
    import logging
    import platform
    import secrets
    from typing import Any, Dict, Optional
    ns.update({
        "re": re, "json": json, "os": types.SimpleNamespace(path=os.path),
        "subprocess": types.SimpleNamespace(DEVNULL=subprocess.DEVNULL,
                                            SubprocessError=subprocess.SubprocessError,
                                            TimeoutExpired=subprocess.TimeoutExpired),
        "signal": types.SimpleNamespace(SIGTERM=15, SIGKILL=9),
        "platform": platform, "logging": logging, "asyncio": asyncio,
        "contextlib": contextlib, "contextmanager": contextlib.contextmanager,
        "suppress": contextlib.suppress, "wraps": functools.wraps,
        "hashlib": hashlib, "hmac": hmac, "secrets": secrets,
        "Path": PathDouble, "MessageType": MessageType,
        "Any": Any, "Dict": Dict, "Optional": Optional,
    })
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # module-level functions (async defs exec too; never awaited)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    # _classify_bridge_message is a staticmethod on WhatsAppAdapter: extract
    # the plain function (decorator stripped, dedented).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "WhatsAppAdapter":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == "_classify_bridge_message":
                    sub2 = copy.copy(sub)
                    sub2.decorator_list = []
                    exec_node(src, sub2, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = TARGETS + [
        "_IS_WINDOWS", "_OWNER_REPLY_PREFIX", "_RUN_TEXT", "_SIGTERM_GRACE_S",
        "_BRIDGE_PORT_FREE_TIMEOUT_S", "_SIGKILL", "_BRIDGE_LOG_ROTATE_BYTES",
        "PASSKEY_REQUIRED_EXIT", "_BRIDGE_TOKEN_FILE", "_BRIDGE_PASSTHROUGH_ENV",
        "_BRIDGE_CONSUMED_ENV", "_BRIDGE_FINGERPRINT_FILE", "_TEXT_INJECT_EXTS",
        "_MAX_TEXT_INJECT_BYTES", "_NATIVE_MEDIA_TYPES", "_MEDIA_NEEDLES",
        "_MEDIA_INFO", "_MEDIA_TYPE_BY_BRIDGE_KIND", "_QUOTED_MIME_BY_BRIDGE_KIND",
        "_WA_EXT_MEDIA_TYPE", "_YAML_BRIDGE", "_PLATFORM_HINT", "_SEND_JID_RE",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_whatsapp_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── stub modules for the in-function imports (attrs swapped per case) ──
_psutil = new_module("psutil")
_psutil.CONN_LISTEN = "LISTEN"
_psutil.NoSuchProcess = type("NoSuchProcess", (Exception,), {})
_gw = new_module("gateway")
_gw.__path__ = []
_gstat = new_module("gateway.status")
_gw.status = _gstat
_gwp = new_module("gateway.platforms")
_gwp.__path__ = []
_gw.platforms = _gwp
_gbase = new_module("gateway.platforms.base")
_gwp.base = _gbase
_hc = new_module("hermes_cli")
_hc.__path__ = []
_sc = new_module("hermes_cli._subprocess_compat")
_hc._subprocess_compat = _sc
_hcg = new_module("hermes_cli.gateway")
_hc.gateway = _hcg
_pm = new_module("pm")
REAL_TIME = sys.modules.get("time")


# ── SEAMED names: regex-extracted from the C port's wa_call sites ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "whatsapp_adapter_flow.c",
                encoding="utf-8").read()
    names = set(re.findall(r'wa_call\(\s*ctx,\s*"([^"]+)"', csrc))
    # variable / ternary call sites the literal regex cannot see
    names |= {"get_image_cache_dir", "get_audio_cache_dir",
              "get_video_cache_dir", "get_document_cache_dir",
              "_windows_listener_pids", "_listener_pids_on_port"}
    return names


SEAMED = seam_names_from_c()
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, MessageType):
            return x.value
        if isinstance(x, PurePosixPath):
            return str(x)
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "IndexError": IndexError,
    "OverflowError": OverflowError, "OSError": OSError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ConnectionError": ConnectionError, "ConnectionRefusedError": ConnectionRefusedError,
    "SubprocessError": subprocess.SubprocessError, "TimeoutExpired": subprocess.TimeoutExpired,
    "CalledProcessError": subprocess.CalledProcessError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "StopIteration": StopIteration,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is KeyError:
        return cls(msg)  # str(KeyError(k)) == repr(k); spec round-trips bare
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam arg projections (live call -> trace args, mirrors the C sites) ──
ARGMAP = {
    "_bridge_pid_is_ours": lambda pid, expected_start: {"pid": pid, "expected_start": expected_start},
    "_listener_pids_on_port": lambda port: {"port": port},
    "_windows_listener_pids": lambda port: {"port": port},
    "_port_listener_pids": lambda port: {"port": port},
    "_pid_exists": lambda pid: {"pid": pid},
    "_pid_looks_like_node_bridge": lambda pid: {"pid": pid},
    "_pids_alive": lambda pids: {"pids": list(pids)},
    "_read_bridge_pidfile": lambda session_path: {"session_path": str(session_path)},
    "_unlink_quietly": lambda path: {"path": str(path)},
    "_wait_pids_exit": lambda pids, timeout=None: {"pids": list(pids), "timeout": timeout},
    "chmod": lambda path, mode: {"path": str(path), "mode": mode},
    "exists": lambda path: {"path": str(path)},
    "find_node_executable": lambda name: {"name": name},
    "get_audio_cache_dir": lambda: {},
    "get_document_cache_dir": lambda: {},
    "get_image_cache_dir": lambda: {},
    "get_video_cache_dir": lambda: {},
    "get_env_value": lambda name: {"name": name},
    "get_process_start_time": lambda pid: {"pid": pid},
    "get_scoped_secret": lambda name, default="": {"name": name, "default": default},
    "lazy_installs_allowed": lambda: {},
    "logger.info": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "logger.warning": lambda *a: {"msg": a[0], "args": list(a[1:])},
    "normalize_whatsapp_mention_jid": lambda m: {"mention": m},
    "os.kill": lambda pid, sig: {"pid": pid, "sig": sig},
    "psutil.Process": lambda pid: {"pid": pid},
    "psutil.net_connections": lambda kind=None: {"kind": kind},
    "read_bytes": lambda path: {"path": str(path)},
    "read_text": lambda path, encoding=None: {"path": str(path), "encoding": encoding},
    "spawn_owned": lambda argv, **kw: {"argv": list(argv), **kw},
    "subprocess.run": lambda argv, **kw: {"argv": list(argv), **kw},
    "time.monotonic": lambda: {},
    "time.sleep": lambda seconds: {"seconds": seconds},
    "unlink": lambda path: {"path": str(path)},
    "windows_detach_flags": lambda: {},
    "windows_hide_flags": lambda: {},
    "with_hermes_node_path": lambda: {},
    "write_text": lambda path, text, encoding=None: {"path": str(path), "text": text, "encoding": encoding},
}


def _proc_double(r):
    d = dict(r)

    class P:
        def name(self):
            if "name" not in d:
                raise AttributeError("'types.SimpleNamespace' object has no attribute 'name'")
            return d["name"]

        def cmdline(self):
            if "cmdline" not in d:
                raise AttributeError("'types.SimpleNamespace' object has no attribute 'cmdline'")
            return d["cmdline"]
    return P()


def _conn_double(c):
    if not isinstance(c, dict):
        return c
    kw = dict(c)
    if isinstance(kw.get("laddr"), dict):
        kw["laddr"] = types.SimpleNamespace(**kw["laddr"])
    return types.SimpleNamespace(**kw)


BUILD = {
    "subprocess.run": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "psutil.Process": lambda r: _proc_double(r) if isinstance(r, dict) else r,
    "psutil.net_connections": lambda r: [_conn_double(c) for c in r] if isinstance(r, list) else r,
    "read_bytes": lambda r: r.encode("utf-8") if isinstance(r, str) else r,
    "_read_bridge_pidfile": lambda r: tuple(r) if isinstance(r, list) else r,
    "_cache_dirs": lambda r: tuple(r) if isinstance(r, list) else r,
}


# ── per-case script backend (mirrors the C wa_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw) if build else raw
    return f


class _LoggerDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def warning(self, *a):
        return make_recorder(self._ctx, "logger.warning")(*a)

    def info(self, *a):
        return make_recorder(self._ctx, "logger.info")(*a)


def install_recorders(ctx, target, is_windows):
    for nm in SEAMED:
        if nm in REAL_DEFS:
            NS[nm] = REAL_DEFS[nm]
    NS["_IS_WINDOWS"] = is_windows
    NS["_SIGKILL"] = 15 if is_windows else 9
    PathDouble.CTX = ctx
    logger = _LoggerDouble(ctx)
    NS["logger"] = logger
    osdbl = NS["os"]
    spdbl = NS["subprocess"]
    timemod = sys.modules["time"]
    for nm in SEAMED:
        if nm == target:
            continue
        rec = make_recorder(ctx, nm)
        if nm == "subprocess.run":
            spdbl.run = rec
        elif nm == "os.kill":
            osdbl.kill = rec
        elif nm == "chmod":
            osdbl.chmod = rec
        elif nm == "time.monotonic":
            timemod.monotonic = rec
        elif nm == "time.sleep":
            timemod.sleep = rec
        elif nm == "psutil.Process":
            _psutil.Process = rec
        elif nm == "psutil.net_connections":
            _psutil.net_connections = rec
        elif nm in ("_pid_exists", "get_process_start_time"):
            setattr(_gstat, nm, rec)
        elif nm == "windows_hide_flags":
            _sc.windows_hide_flags = rec
        elif nm == "get_env_value":
            _hcg.get_env_value = rec
        elif nm == "lazy_installs_allowed":
            _pm.lazy_installs_allowed = rec
        elif nm in ("get_image_cache_dir", "get_audio_cache_dir",
                    "get_video_cache_dir", "get_document_cache_dir"):
            # _cache_dirs imports these from gateway.platforms.base at runtime
            setattr(_gbase, nm, rec)
        elif nm in ("exists", "read_text", "write_text", "unlink", "read_bytes"):
            pass  # PathDouble methods record through PathDouble.CTX
        else:
            # module-level foreign names (find_node_executable, spawn_owned,
            # get_scoped_secret, ...) are never bound by AST extraction
            NS[nm] = rec


# ── argument materialization ──
def mat_config(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**d)
    return d


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case live call ──
def live_call(func, args):
    f = NS[func]
    if func == "_safe_ints":
        return f(args["tokens"])
    if func == "_fmt_uptime":
        return f(args["seconds"])
    if func == "_bridge_auth_headers":
        return f(args["token"])
    if func == "_bridge_media_type":
        return f(args["file_path"], args["is_voice"], args["force_document"])
    if func == "_bridge_auth_proof":
        return f(args["token"], args["challenge"])
    if func == "_bridge_config_fingerprint":
        return f(args["bridge_env"], args.get("package_hash", ""))
    if func == "_classify_bridge_message":
        return f(args["data"])
    if func == "_wenv":
        return f(args["name"], args.get("default", ""))
    if func == "_is_connected":
        return f(mat_config(args.get("config")))
    if func == "_cache_dirs":
        return f()
    if func == "_unlink_quietly":
        return f(PathDouble(args["path"]))
    if func == "_tighten_bridge_log":
        return f(PathDouble(args["path"]), private=args["private"])
    if func == "_read_bridge_token":
        return f(PathDouble(args["session_path"]))
    if func == "_write_bridge_fingerprint":
        return f(PathDouble(args["session_path"]), args["fingerprint"])
    if func == "_write_bridge_pidfile":
        return f(PathDouble(args["session_path"]), args["pid"], args.get("port"))
    if func == "_read_bridge_pidfile":
        return f(PathDouble(args["session_path"]))
    if func == "_bridge_pid_is_ours":
        return f(args["pid"], args["expected_start"])
    if func == "_pids_alive":
        return f(args["pids"])
    if func == "_wait_pids_exit":
        return f(args["pids"], args.get("timeout"))
    if func == "_pid_looks_like_node_bridge":
        return f(args["pid"])
    if func == "_windows_listener_pids":
        return f(args["port"])
    if func == "_listener_pids_on_port":
        return f(args["port"])
    if func == "_port_listener_pids":
        return f(args["port"])
    if func == "_kill_port_process":
        return f(args["port"])
    if func == "_kill_stale_bridge_by_pidfile":
        return f(PathDouble(args["session_path"]))
    if func == "check_whatsapp_requirements":
        return f()
    if func == "_file_content_hash":
        return f(PathDouble(args["path"]))
    if func == "_bridge_source_hash":
        return f(PathDouble(args["bridge_path"]))
    if func == "_spawn_bridge_process":
        return f(args["argv"], args["log_fh"], args["env"], args.get("umask", -1))
    if func == "_normalize_outbound_mentions":
        return f(args.get("mentions"))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    isw = args.get("is_windows", True)
    tstub = types.ModuleType("time")
    sys.modules["time"] = tstub
    try:
        install_recorders(ctx, func, isw)
        try:
            ret = live_call(func, args)
        except BaseException as exc:  # SystemExit included
            ret = exc_spec(exc, ctx)
        return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}
    finally:
        if REAL_TIME is not None:
            sys.modules["time"] = REAL_TIME


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_whatsapp_adapter_flow: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_whatsapp_adapter_flow: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "whatsapp_adapter_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_whatsapp_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_whatsapp_adapter_flow: fixture funcs without a target: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE whatsapp_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved).
    Flags drive exceptions, UNINTENTIONAL script gaps, and AttributeError
    families naming a ported method / seam stub (binding-bug signature)."""
    fix = _REPO / "tests" / "fixtures" / "whatsapp_adapter_flow.jsonl"
    cross_check()
    n = bad = 0
    counts = {}
    names = SEAMED | set(TARGETS)
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            hit = [nm for nm in names if f"'{nm}'" in msg]
            if hit:
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg} (names {hit})")
    print(f"SELFTEST whatsapp_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    thin = [f for f in TARGETS if counts.get(f, 0) < 5]
    if thin:
        print(f"SELFTEST THIN FUNCS (<5 cases): {thin}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
