#!/usr/bin/env python3
"""Oracle for the pm/packages.py FLOW port (src/hermes_cli/pm_packages_flow.c).

Drives the LIVE pm/packages.py decision chains with SCRIPTED seams: every
foreign call (subprocess.run, native, current_target, flatten_single_dir,
_binary_reason, _probe_reason, the pm.update index functions, os.environ,
env_for, get_default_hermes_root, _get_json, try_install_libatomic,
read_bytes) and every cross-class dispatch (super().verify, DebPackage.verify,
BinaryPackage.verify, Nodejs self.verify, Ffmpeg self.missing_reason,
WhisperCppCpu self.fetch_url=Package.fetch_url) is a recording stub fed the
same scripts the C harness feeds its universal seam. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the final
return ("ret") and the mutated state ("state", the release-digest cache).

The live defs are AST-extracted from pm/packages.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; LOG re-set to silent AFTER
seeding). InstallError is AST-extracted from pm/package.py; ALL_TARGETS /
MUSL_TARGETS from pm/store.py. Every exec namespace is a registered
sys.modules module (py3.14). The heavy quarry import chain is never imported.

Within-class self calls between ported methods compose REAL (binary->_rel,
verify->binary/_probe_env, fetch_url->fetch_urls->_asset_names); the oracle
binds the extracted methods onto a types.SimpleNamespace self double built
from the fixture's class-attribute object. super().verify is text-rewritten
to SUPER_verify (a seam) because super() needs a class cell the extraction
does not carry.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE pm_packages_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds errno, message "[Errno N] msg"; KeyError message repr(key)).
- Paths are POSIX strings; the oracle drives PurePosixPath doubles and
  projects with as_posix(), so str(Path) never flips slashes on Windows.
- read_bytes scripts are latin-1 byte tokens (strings): token equality ==
  bytes equality.
- Plain JSON numbers are ints; integral floats normalize to int on both
  sides; bools stay bools.
- dict(os.environ) scripts: object or null (null -> the real TypeError /
  AttributeError at the live call site).
- str.format domain: templates carry only {toolkit} holes (LlamaCppCuda).
- BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except site; a missing/exhausted script
  is a RECORDED RuntimeError raise on both sides (trace parity).
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import asyncio
import hashlib
import logging
import shutil
import subprocess
import tempfile
import uuid
import __future__ as _fut
from pathlib import Path, PurePosixPath
from typing import Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "pm", "packages.py")
PKG_SRC = os.path.join(Q, "pm", "package.py")
STORE_SRC = os.path.join(Q, "pm", "store.py")

_FLAGS = _fut.annotations.compiler_flag


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


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ported methods per class (the composition closure bound onto self doubles)
PORTED = {
    "BinaryPackage": ["_rel", "stage", "binary", "verify", "_probe_env"],
    "Dmgbuild": ["fetch_url", "verify"],
    "_BionicDebArm": ["verify"],
    "Uv": ["fetch_url", "latest_versions"],
    "Python": ["fetch_url", "latest_versions", "verify"],
    "Nodejs": ["fetch_url", "latest_versions", "repair_staged_verification"],
    "TermuxDocker": ["missing_reason", "fetch_url"],
    "Npm": ["latest_versions"],
    "Pnpm": ["fetch_urls", "latest_versions"],
    "Git": ["fetch_url", "latest_versions", "env"],
    "Gh": ["fetch_url", "latest_versions"],
    "Ffmpeg": ["fetch_url", "latest_versions"],
    "Ripgrep": ["fetch_url", "latest_versions"],
    "Just": ["fetch_url", "known_sha256", "latest_versions"],
    "CuaDriver": ["fetch_url", "latest_versions"],
    "Chromium": ["store_entry", "fetch_url"],
    "LlamaCpp": ["_asset_names", "fetch_urls", "fetch_url", "latest_versions", "known_sha256"],
    "LlamaCppCuda": ["_asset_names"],
    "WhisperCppCpu": ["fetch_urls"],
}
MODULE_FUNCS = ["npm_env", "pnpm_env", "uv_cache_path", "_github_release_digests"]
# methods whose extracted body calls super().verify (rewritten to a seam)
SUPER_REWRITE = {("Dmgbuild", "verify"), ("Python", "verify")}


def extract():
    mod = new_module("ppf_oracle_packages")
    ns = mod.__dict__
    ns.update({"os": os, "sys": sys, "subprocess": subprocess, "shutil": shutil,
               "logging": logging, "uuid": uuid, "tempfile": tempfile,
               "hashlib": hashlib, "re": re, "json": json, "Path": Path,
               "PurePosixPath": PurePosixPath, "Optional": Optional})
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            exec_node(src, node, ns, SRC)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in PORTED:
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in PORTED[node.name]:
                    sub = copy.copy(sub)
                    sub.decorator_list = []
                    text = textwrap.dedent(seg(src, sub))
                    if (node.name, sub.name) in SUPER_REWRITE:
                        text = text.replace("super().verify(", "SUPER_verify(")
                    exec(compile(text, SRC, "exec", flags=_FLAGS, dont_inherit=True), ns)
                    ns[f"{node.name}.{sub.name}"] = ns.pop(sub.name)
    # the LOG Assign ran during seeding; re-set the silent recorder AFTER
    ns["LOG"] = _SilentLogger()
    # InstallError (real class) from pm/package.py
    psrc = open(PKG_SRC, encoding="utf-8").read()
    for node in ast.parse(psrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "InstallError":
            exec_node(psrc, node, ns, PKG_SRC)
    # ALL_TARGETS / MUSL_TARGETS from pm/store.py
    ssrc = open(STORE_SRC, encoding="utf-8").read()
    seed_constants(ssrc, ns, STORE_SRC)
    required = MODULE_FUNCS + [f"{c}.{m}" for c, ms in PORTED.items() for m in ms] + [
        "InstallError", "_RUST_TRIPLE", "_NODE_PLAT", "MUSL_TARGETS", "ALL_TARGETS",
        "_VC_RUNTIME", "WINDOWS_PYTHON3", "_release_digest_cache",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_pm_packages_flow: missing defs/constants: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL_DIGESTS = NS["_github_release_digests"]  # the real leaf, for its own cases

# in-function import stubs (packages registered once; attributes set per case)
for _name in ("pm", "pm.install", "pm.update", "pm.libatomic", "hermes_constants"):
    if _name not in sys.modules:
        _m = new_module(_name)
        if _name == "pm":
            _m.__path__ = []
sys.modules["pm"].install = sys.modules["pm.install"]
sys.modules["pm"].update = sys.modules["pm.update"]
sys.modules["pm"].libatomic = sys.modules["pm.libatomic"]


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
        if isinstance(x, bytes):
            return x.decode("latin-1")
        if isinstance(x, (PurePosixPath, Path)):
            return x.as_posix()
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- exception materialization / projection --
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ConnectionError": ConnectionError,
    "ConnectionResetError": ConnectionResetError, "BrokenPipeError": BrokenPipeError,
    "InterruptedError": InterruptedError, "NotADirectoryError": NotADirectoryError,
    "IsADirectoryError": IsADirectoryError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "IndexError": IndexError, "ImportError": ImportError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    if kind == "TimeoutExpired":
        return subprocess.TimeoutExpired(spec.get("cmd", []), spec.get("timeout", 60))
    if kind == "InstallError":
        return NS["InstallError"](spec.get("package", ""), spec.get("cause", ""),
                                  spec.get("remedy", ""))
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# -- per-case script backend (mirrors the C pp_call exactly) --
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def posix(p):
    if p is None:
        return None
    if isinstance(p, (PurePosixPath, Path)):
        return p.as_posix()
    return str(p)


def pathify(v):
    return PurePosixPath(v) if isinstance(v, str) else v

# -- per-case seam installation --
class _OsDouble:
    name = "nt"

    def __init__(self, ctx):
        self._ctx = ctx

    @property
    def environ(self):
        return self._ctx.call("os_environ", {})


def _run_args(argv, capture_output=None, timeout=None, cwd=None, env=None, check=None):
    args = {"argv": list(argv)}
    if capture_output is not None:
        args["capture_output"] = capture_output
    if timeout is not None:
        args["timeout"] = timeout
    if cwd is not None:
        args["cwd"] = cwd
    if env is not None:
        args["env"] = env
    if check is not None:
        args["check"] = check
    return args


def _grt(ctx):
    def github_release_tags(repo, strip_prefix=None, include_prereleases=False):
        args = {"repo": repo}
        if strip_prefix is not None:
            args["strip_prefix"] = strip_prefix
        if include_prereleases:
            args["include_prereleases"] = True
        return ctx.call("github_release_tags", args)
    return github_release_tags


def install_seams(ctx):
    NS["native"] = rec(ctx, "native", lambda path: {"path": posix(path)})
    NS["current_target"] = rec(ctx, "current_target", lambda: {})
    NS["flatten_single_dir"] = rec(ctx, "flatten_single_dir",
                                   lambda staged: drop_none({"staged": posix(staged)}))
    NS["_probe_reason"] = rec(ctx, "_probe_reason",
                              lambda binary, proc: drop_none({"binary": posix(binary), "proc": proc}))
    NS["github_release_tags"] = _grt(ctx)
    NS["node_latest_versions"] = rec(ctx, "node_latest_versions", lambda: {})
    NS["npm_dist_tags"] = rec(ctx, "npm_dist_tags", lambda name: {"name": name})
    NS["pbs_versions"] = rec(ctx, "pbs_versions", lambda minor, triple: {"minor": minor, "triple": triple})
    NS["btbn_index"] = rec(ctx, "btbn_index", lambda: {})
    NS["btbn_versions"] = rec(ctx, "btbn_versions", lambda target: {"target": target})
    NS["martin_riedl_index"] = rec(ctx, "martin_riedl_index", lambda: {})
    NS["martin_riedl_versions"] = rec(ctx, "martin_riedl_versions", lambda target: {"target": target})
    NS["llama_app_latest"] = rec(ctx, "llama_app_latest", lambda: {})
    NS["llama_app_bucket_versions"] = rec(ctx, "llama_app_bucket_versions", lambda: {})
    NS["os"] = _OsDouble(ctx)
    NS["subprocess"] = types.SimpleNamespace(
        run=rec(ctx, "subprocess_run", _run_args,
                build=lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r),
        TimeoutExpired=subprocess.TimeoutExpired,
        DEVNULL=subprocess.DEVNULL,
    )
    # known_sha256 flows seam the digest leaf (its own cases drive REAL_DIGESTS)
    NS["_github_release_digests"] = rec(ctx, "github_release_digests",
                                        lambda repo, tag: {"repo": repo, "tag": tag})
    NS["SUPER_verify"] = rec(ctx, "super_verify",
                             lambda entry, target: drop_none({"entry": posix(entry), "target": target}))
    NS["DebPackage"] = types.SimpleNamespace(verify=rec(
        ctx, "deb_verify", lambda slf, entry, target: drop_none({"entry": posix(entry), "target": target})))
    NS["BinaryPackage"] = types.SimpleNamespace(verify=rec(
        ctx, "bp_verify", lambda slf, entry, target: drop_none({"entry": posix(entry), "target": target})))
    sys.modules["hermes_constants"].get_default_hermes_root = rec(
        ctx, "get_default_hermes_root", lambda: {},
        build=lambda r: PurePosixPath(r) if isinstance(r, str) else r)
    sys.modules["pm.install"].env_for = rec(ctx, "env_for", lambda *a: {"deps": list(a)})
    sys.modules["pm.update"]._get_json = rec(ctx, "_get_json", lambda url: {"url": url})
    sys.modules["pm.libatomic"].try_install_libatomic = rec(ctx, "try_install_libatomic", lambda: {})


def make_self(cls, spec, ctx):
    slf = types.SimpleNamespace(**(spec or {}))
    for m in PORTED[cls]:
        setattr(slf, m, types.MethodType(NS[f"{cls}.{m}"], slf))
    # foreign self-methods are seamed (MRO targets outside the ported set)
    if cls in ("BinaryPackage", "Dmgbuild"):
        slf._binary_reason = rec(ctx, "_binary_reason", lambda binary, entry, target: drop_none(
            {"binary": posix(binary), "entry": posix(entry), "target": target}))
    if cls == "Nodejs":
        slf.verify = rec(ctx, "self_verify",
                         lambda entry, target: drop_none({"entry": posix(entry), "target": target}))
    if cls == "Ffmpeg":
        slf.missing_reason = rec(ctx, "missing_reason", lambda target: {"target": target})
    if cls == "WhisperCppCpu":
        slf.fetch_url = rec(ctx, "base_fetch_url",
                            lambda version, target: drop_none({"version": version, "target": target}))
    return slf


# -- per-case drive --
def drive(func, inp):
    args = inp.get("args") or {}
    state = {}
    ctx = Ctx(inp.get("io"))
    install_seams(ctx)
    cache = None
    if func == "_github_release_digests":
        cache = {}
        for r, subs in ((inp.get("state") or {}).get("release_digest_cache") or {}).items():
            for t, v in subs.items():
                cache[(r, t)] = dict(v)
        NS["_release_digest_cache"] = cache
    rb_installed = False
    if func == "Python.verify":
        PurePosixPath.read_bytes = rec(ctx, "read_bytes", lambda slf: {"path": slf.as_posix()},
                                       build=lambda r: r.encode("latin-1") if isinstance(r, str) else r)
        rb_installed = True
    try:
        if func == "npm_env":
            ret = NS["npm_env"](args.get("cache_dir"), base_env=args.get("base_env"))
        elif func == "pnpm_env":
            ret = NS["pnpm_env"](base_env=args.get("base_env"))
        elif func == "uv_cache_path":
            ret = NS["uv_cache_path"]()
        elif func == "_github_release_digests":
            ret = REAL_DIGESTS(args.get("repo"), args.get("tag"))
        elif "." in func:
            cls, meth = func.split(".", 1)
            slf = make_self(cls, args.get("self"), ctx)
            f = NS[func]
            if meth == "_rel":
                ret = f(slf, args.get("target"))
            elif meth == "stage":
                ret = f(slf, None, pathify(args.get("staged")), args.get("version"), args.get("target"))
            elif meth in ("binary", "verify", "env"):
                ret = f(slf, pathify(args.get("entry")), args.get("target"))
            elif meth == "_probe_env":
                ret = f(slf)
            elif meth in ("fetch_url", "store_entry", "_asset_names", "fetch_urls"):
                ret = f(slf, args.get("version"), args.get("target"))
            elif meth == "latest_versions":
                ret = f(slf, args.get("target"), locked=args.get("locked"))
            elif meth == "repair_staged_verification":
                ret = f(slf, pathify(args.get("entry")), args.get("target"), args.get("reason"))
            elif meth == "missing_reason":
                ret = f(slf, args.get("target"))
            elif meth == "known_sha256":
                ret = f(slf, args.get("version"), args.get("url"))
            else:
                raise RuntimeError(f"unknown method {meth}")
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    finally:
        if rb_installed:
            del PurePosixPath.read_bytes
        if cache is not None:
            nested = {}
            for (r, t), d in cache.items():
                nested.setdefault(r, {})[t] = dict(d)
            state["release_digest_cache"] = nested
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# -- three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) --
SEAM_NAMES = [
    "flatten_single_dir", "_binary_reason", "current_target", "native", "subprocess_run",
    "_probe_reason", "os_environ", "env_for", "super_verify", "deb_verify", "bp_verify",
    "self_verify", "missing_reason", "github_release_tags", "node_latest_versions",
    "npm_dist_tags", "pbs_versions", "btbn_index", "btbn_versions", "martin_riedl_index",
    "martin_riedl_versions", "llama_app_latest", "llama_app_bucket_versions",
    "try_install_libatomic", "_get_json", "get_default_hermes_root",
    "github_release_digests", "base_fetch_url", "read_bytes",
]


def cross_check():
    csrc = open(_REPO / "src" / "hermes_cli" / "pm_packages_flow.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_pm_packages_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_pm_packages_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    hsrc = open(_REPO / "tests" / "t_port_pm_packages_flow.c", encoding="utf-8").read()
    hfuncs = set(re.findall(r'\{\s*"([^"]+)",\s*\w+\s*\}', hsrc))
    fix = _REPO / "tests" / "fixtures" / "pm_packages_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_pm_packages_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - hfuncs
        if badf:
            print(f"FATAL oracle_pm_packages_flow: fixture funcs without a harness case: {sorted(badf)}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(got) else None
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
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE pm_packages_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


# hand-computed expectations (independent of the fixture corpus)
HAND_CHECKS = [
    ("npm_env",
     {"args": {"cache_dir": "/c", "base_env": {"PATH": "/usr/bin", "NPM_CONFIG_REGISTRY": "x",
                                               "node_options": "y", "NODE_PATH": "z", "A": "b"}},
      "io": {}},
     {"PATH": "/usr/bin", "A": "b", "npm_config_cache": "/c"}),
    ("Git.fetch_url",
     {"args": {"self": {}, "version": "2.56.0+1", "target": "win32-x64"}, "io": {}},
     "https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.1/PortableGit-2.56.0-64-bit.7z.exe"),
    ("Uv.fetch_url",
     {"args": {"self": {}, "version": "0.12.3", "target": "linux-arm64-bionic"}, "io": {}},
     "https://packages.termux.dev/apt/termux-main/pool/main/u/uv/uv_0.12.3_aarch64.deb"),
    ("TermuxDocker.missing_reason",
     {"args": {"self": {}, "target": "linux-arm64-bionic"}, "io": {}}, None),
    ("Chromium.store_entry",
     {"args": {"self": {"name": "chromium"}, "version": "1148+131.0.6778.85", "target": "linux-x64"}, "io": {}},
     "chromium-1148"),
]


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    bad = 0
    for func, inp, want_ret in HAND_CHECKS:
        out = drive(func, inp)
        if out["ret"] != want_ret:
            bad += 1
            print(f"SELFTEST HAND-CHECK FAILED {func}: want={want_ret!r} got={out['ret']!r}")
    fix = _REPO / "tests" / "fixtures" / "pm_packages_flow.jsonl"
    n = 0
    counts = {}
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
            # fail-closed cases are intentional when the fixture marks them
            if not (case.get("in") or {}).get("expect_fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if ((case.get("in") or {}).get("io")) and not out["trace"]:
            bad += 1
            print(f"SELFTEST SCRIPT UNUSED {func} case {n}: io scripted but no seam fired; ret={json.dumps(r)[:200]}")
    print(f"SELFTEST pm_packages_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
