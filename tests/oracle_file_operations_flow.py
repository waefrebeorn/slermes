#!/usr/bin/env python3
"""Oracle for file_operations_flow.c — the UNCLAIMED tools/file_operations.py
FLOW cores + leaves (LIVE authority): the ShellFileOperations constructor, the
cat/head exec leaves, the sentinel-fenced compound read (_fenced_read) and its
byte-exact readers (_read_exact_bytes / _read_exact_bytes_hex) with the
_bounded / _failed_read / _matches_size static leaves, the stat probe
(_probe_regular_file) + _env_unavailable_error, _detect_binary, _native_path,
the backend-Python leaves (_python_interpreter_cmd / _exec_python_snippet),
read_file_bytes, the pre-write refusal gates (_reject_unencodable /
_json_nonstandard_constant / _refuse_introduced_json_constant /
_fail_closed_syntax_error), _verify_written_hash, the write_file orchestrator,
_no_match_result and _verify_patch_persisted.

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy tools import), module + class constants seeded, result dataclasses
extracted from tools/file_operations_common.py, and every claimed sibling
(_exec, _escape_shell_arg, _new_sentinel, _split_segments,
_strip_terminal_fence_leaks, _strip_bom, _has_bom, _normalize_line_endings,
_native_read_enabled, _decode_base64_sample, _sample_file_bytes,
_is_likely_binary_bytes, _is_likely_binary, _not_regular_error, _expand_path,
_probe_write_target, _atomic_write) plus every foreign collaborator
(has_binary_extension, get_write_denied_error, the LintMixin methods,
_native_io.sha256_of, time.monotonic, the fuzzy_match helpers, the
LINTERS_INPROC linters, and the native read's os.open/os.fstat/open(fd)/
fh.read/os.close) replaced by recording stubs returning the SAME scripted
results the C harness feeds its universal seam. Ported-to-ported calls compose
REAL on both sides, so their internal seam events appear in both traces.

bytes travel as lowercase hex strings; result dataclasses project to their
FULL field dicts; tuple returns project to lists; _TooLarge projects to the
plain ExecuteResult fields (the C harness strips its internal tag).
_reject_unencodable content travels as a codepoint array (lone surrogates
cannot cross a UTF-8 JSON transport); the oracle joins it to a str. os.path is
posixpath (POSIX backend semantics); os.O_NONBLOCK is the POSIX 0o4000.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; the live
`except OSError` / `except (ValueError, RecursionError)` / `except (IndexError,
ValueError)` / `except Exception` sites swallow exactly their live kinds.

Input: stdin JSON lines from t_port_file_operations_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE file_operations_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.
"""
import ast
import base64
import binascii
import builtins
import copy
import dataclasses
import difflib
import hashlib
import json
import logging
import os
import posixpath
import re
import secrets
import sys
import textwrap
import types
import __future__
from pathlib import Path
from typing import Any, ClassVar, Dict, List, Optional, Tuple

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE file_operations_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "file_operations_flow_home"))

SRC = str(QUARRY / "tools" / "file_operations.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
COMMON_TEXT = open(str(QUARRY / "tools" / "file_operations_common.py"), encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "init": "__init__",
    "_cat": "_cat",
    "_head": "_head",
    "_bounded": "_bounded",
    "_failed_read": "_failed_read",
    "_matches_size": "_matches_size",
    "_fenced_read": "_fenced_read",
    "_read_exact_bytes_hex": "_read_exact_bytes_hex",
    "_read_exact_bytes": "_read_exact_bytes",
    "_probe_regular_file": "_probe_regular_file",
    "_env_unavailable_error": "_env_unavailable_error",
    "_detect_binary": "_detect_binary",
    "_native_path": "_native_path",
    "_python_interpreter_cmd": "_python_interpreter_cmd",
    "_exec_python_snippet": "_exec_python_snippet",
    "read_file_bytes": "read_file_bytes",
    "_reject_unencodable": "_reject_unencodable",
    "_json_nonstandard_constant": "_json_nonstandard_constant",
    "_refuse_introduced_json_constant": "_refuse_introduced_json_constant",
    "_fail_closed_syntax_error": "_fail_closed_syntax_error",
    "_verify_written_hash": "_verify_written_hash",
    "write_file": "write_file",
    "_no_match_result": "_no_match_result",
    "_verify_patch_persisted": "_verify_patch_persisted",
}
# ported instance methods (bound with MethodType on the self double)
PORTED_INSTANCE = [
    "_cat", "_head", "_fenced_read", "_read_exact_bytes", "_read_exact_bytes_hex",
    "_probe_regular_file", "_env_unavailable_error", "_detect_binary", "_native_path",
    "_python_interpreter_cmd", "_exec_python_snippet", "read_file_bytes",
    "_reject_unencodable", "_verify_written_hash",
    "write_file", "_no_match_result", "_verify_patch_persisted",
]
# ported staticmethods (plain function attributes — no self passed)
PORTED_STATIC = ["_bounded", "_failed_read", "_matches_size", "_fail_closed_syntax_error"]
# ported module-level functions
PORTED_MODULE = ["_json_nonstandard_constant", "_refuse_introduced_json_constant"]
# class-body constants attached to the self double
CLASS_CONSTS = ["_LONE_SURROGATE_RE", "_UTF16_MAX_BYTES", "_UTF16_SAMPLE_BYTES"]
RESULT_CLASSES = ["ReadResult", "WriteResult", "PatchResult", "ExecuteResult", "LintResult"]

DEFAULT_LINTER_EXTS = [".py", ".json", ".yaml", ".yml", ".toml"]
DEFAULT_FAIL_CLOSED_EXTS = [".json", ".yaml", ".yml", ".toml"]
DEFAULT_SYS_EXECUTABLE = "/usr/bin/python3"

CUR = {"scripts": {}, "idx": {}, "trace": []}
CLS = {}     # result dataclasses (filled at extract)
NS = None    # extraction namespace (filled at extract)

# ── exception construction (REAL builtin classes; full hierarchy) ──
BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit, "BaseExceptionGroup": BaseExceptionGroup,
            "ExceptionGroup": ExceptionGroup}
BUILTIN_EXC = {}
for _n in dir(builtins):
    _c = getattr(builtins, _n)
    if isinstance(_c, type) and issubclass(_c, BaseException):
        BUILTIN_EXC[_n] = _c
BUILTIN_EXC["binascii.Error"] = binascii.Error
BUILTIN_EXC["JSONDecodeError"] = json.JSONDecodeError


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


# ── result projection (FULL dataclass field dicts; bytes -> hex) ──
def proj(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return int(o) if o.is_integer() else o
    if isinstance(o, bytes):
        return o.hex()
    if isinstance(o, (list, tuple)):
        return [proj(v) for v in o]
    if isinstance(o, dict):
        return {str(k): proj(v) for k, v in o.items()}
    if dataclasses.is_dataclass(o) and not isinstance(o, type):
        return {k: proj(v) for k, v in o.__dict__.items()}
    if isinstance(o, BaseException):
        return {"__raise__": type(o).__name__, "msg": str(o)}
    return str(o)


def canon(o):
    return json.loads(json.dumps(proj(o), sort_keys=True, ensure_ascii=False, default=str))


# ── seam-result materializers ──
def mat_exec(r):
    return CLS["ExecuteResult"](**r) if isinstance(r, dict) else r


def mat_read(r):
    return CLS["ReadResult"](**r) if isinstance(r, dict) else r


def mat_lint(r):
    return CLS["LintResult"](**r) if isinstance(r, dict) else r


def mat_bytes(r):
    return bytes.fromhex(r) if isinstance(r, str) else r


def mat_tuple(r):
    return tuple(r) if isinstance(r, list) else r


def mat_fstat(r):
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


# ── recording stubs (read CUR at call time; installed once) ──
def exec_rec(command, cwd=None, timeout=None, stdin_data=None):
    a = {"command": command, "cwd": cwd, "timeout": timeout, "stdin_data": stdin_data}
    return mat_exec(seam("exec", a))


def escape_shell_arg_rec(arg, translate_path=True):
    return seam("escape_shell_arg", {"arg": arg, "translate_path": translate_path})


def new_sentinel_rec(prefix):
    return seam("new_sentinel", {"prefix": prefix})


def split_segments_rec(output, sentinel):
    return seam("split_segments", {"output": output, "sentinel": sentinel})


def strip_fence_rec(text):
    return seam("strip_fence", {"text": text})


def strip_bom_rec(text):
    return mat_tuple(seam("strip_bom", {"text": text}))


def has_bom_rec(text):
    return seam("has_bom", {"text": text})


def normalize_line_endings_rec(text, target):
    return seam("normalize_line_endings", {"text": text, "target": target})


def detect_line_ending_rec(sample):
    return seam("detect_line_ending", {"sample": sample})


def native_read_enabled_rec():
    return seam("native_read_enabled", {})


def decode_base64_sample_rec(text):
    return mat_bytes(seam("decode_base64_sample", {"text": text}))


def sample_file_bytes_rec(path, length=1000):
    return mat_bytes(seam("sample_file_bytes", {"path": path, "length": length}))


def is_likely_binary_bytes_rec(sample):
    return seam("is_likely_binary_bytes", {"sample": sample.hex()})


def is_likely_binary_rec(path, content_sample=None):
    return seam("is_likely_binary", {"path": path, "content_sample": content_sample})


def not_regular_error_rec(path):
    return mat_read(seam("not_regular_error", {"path": path}))


def expand_path_rec(path):
    return seam("expand_path", {"path": path})


def probe_write_target_rec(path, pre_content, want_pre):
    return mat_tuple(seam("probe_write_target",
                          {"path": path, "pre_content": pre_content, "want_pre": want_pre}))


def atomic_write_rec(path, content):
    return mat_exec(seam("atomic_write", {"path": path, "content": content}))


def has_binary_extension_rec(path):
    return seam("has_binary_extension", {"path": path})


def get_write_denied_error_rec(path, *, verb="Write", entry=False):
    return seam("get_write_denied_error", {"path": path, "verb": verb, "entry": entry})


def lsp_local_only_rec():
    return seam("lsp_local_only", {})


def lsp_handles_extension_rec(ext):
    return seam("lsp_handles_extension", {"ext": ext})


def check_lint_delta_rec(path, pre_content=None, post_content=None):
    return mat_lint(seam("check_lint_delta",
                         {"path": path, "pre_content": pre_content, "post_content": post_content}))


def maybe_lsp_diagnostics_rec(path, *, pre_content=None, post_content=None, started=None):
    return seam("maybe_lsp_diagnostics",
                {"path": path, "pre_content": pre_content, "post_content": post_content,
                 "started": started})


def native_sha256_rec(path):
    return seam("native_sha256", {"path": path})


def monotonic_rec():
    return seam("monotonic", {})


def linter_rec(ext):
    def linter(content):
        return mat_tuple(seam("linter", {"ext": ext, "content": content}))
    return linter


def is_already_applied_rec(content, old_string, new_string):
    return seam("is_already_applied",
                {"content": content, "old_string": old_string, "new_string": new_string})


def format_no_match_hint_rec(err_msg, match_count, old_string, content):
    return seam("format_no_match_hint",
                {"err_msg": err_msg, "match_count": match_count,
                 "old_string": old_string, "content": content})


def os_open_rec(path, flags):
    return seam("os_open", {"path": path, "flags": flags})


def os_fstat_rec(fd):
    return mat_fstat(seam("os_fstat", {"fd": fd}))


def os_close_rec(fd):
    return seam("os_close", {"fd": fd})


class FHDouble:
    """open(fd, 'rb', closefd=False) double: read() seamed; with-exit silent."""
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self, n=None):
        return mat_bytes(seam("fh_read", {} if n is None else {"n": n}))


def fh_open_rec(*a, **k):
    seam("fh_open", {"fd": a[0]})
    return FHDouble()


# self-method seam stubs (plain function attributes — no self passed)
SEAM_METHODS = {
    "_exec": exec_rec,
    "_escape_shell_arg": escape_shell_arg_rec,
    "_native_read_enabled": native_read_enabled_rec,
    "_decode_base64_sample": decode_base64_sample_rec,
    "_sample_file_bytes": sample_file_bytes_rec,
    "_is_likely_binary_bytes": is_likely_binary_bytes_rec,
    "_is_likely_binary": is_likely_binary_rec,
    "_not_regular_error": not_regular_error_rec,
    "_expand_path": expand_path_rec,
    "_probe_write_target": probe_write_target_rec,
    "_atomic_write": atomic_write_rec,
    "_lsp_local_only": lsp_local_only_rec,
    "_lsp_handles_extension": lsp_handles_extension_rec,
    "_check_lint_delta": check_lint_delta_rec,
    "_maybe_lsp_diagnostics": maybe_lsp_diagnostics_rec,
}


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


def install_module_stubs():
    """sys.modules stub for the in-function `from tools.fuzzy_match import ...`.
    `import stat` stays the REAL stdlib module (S_ISREG is pure)."""
    tools = types.ModuleType("tools")
    tools.__path__ = []
    sys.modules["tools"] = tools
    fm = types.ModuleType("tools.fuzzy_match")
    fm.is_already_applied = is_already_applied_rec
    fm.format_no_match_hint = format_no_match_hint_rec
    sys.modules["tools.fuzzy_match"] = fm
    tools.fuzzy_match = fm


def extract():
    os_double = types.SimpleNamespace(
        path=posixpath, O_RDONLY=0, O_NONBLOCK=0o4000,
        open=os_open_rec, fstat=os_fstat_rec, close=os_close_rec,
        environ=os.environ,
    )
    sys_double = types.SimpleNamespace(executable=DEFAULT_SYS_EXECUTABLE, platform="linux")
    time_double = types.SimpleNamespace(monotonic=monotonic_rec)
    native_io = types.SimpleNamespace(
        sha256_of=native_sha256_rec,
        probe=lambda *a, **k: seam("native_probe", {}),
        atomic_write=lambda *a, **k: seam("native_atomic_write", {}),
    )
    ns = {
        "__name__": "tools.file_operations_flow_extracted",
        "__builtins__": {**builtins.__dict__, "open": fh_open_rec},
        "base64": base64, "binascii": binascii, "os": os_double, "re": re,
        "sys": sys_double, "difflib": difflib, "hashlib": hashlib, "json": json,
        "logging": logging, "secrets": secrets, "time": time_double,
        "Optional": Optional, "Dict": Dict, "Path": Path,
        "_native_io": native_io,
        "logger": RecLogger(),
        # claimed module-level siblings (recorders override the real defs)
        "_strip_bom": strip_bom_rec, "_has_bom": has_bom_rec,
        "_normalize_line_endings": normalize_line_endings_rec,
        "_detect_line_ending": detect_line_ending_rec,
        "_strip_terminal_fence_leaks": strip_fence_rec,
        "_new_sentinel": new_sentinel_rec, "_split_segments": split_segments_rec,
        "has_binary_extension": has_binary_extension_rec,
        "get_write_denied_error": get_write_denied_error_rec,
        "_UTF8_BOM": "\ufeff",
        "LINTERS_INPROC": {e: linter_rec(e) for e in DEFAULT_LINTER_EXTS},
        "_FAIL_CLOSED_INPROC_EXTS": frozenset(DEFAULT_FAIL_CLOSED_EXTS),
    }

    # result dataclasses from the common leaf module
    cns = {"__name__": "tools.file_operations_common_extracted",
           "__builtins__": builtins.__dict__, "re": re,
           "dataclass": dataclasses.dataclass, "field": dataclasses.field,
           "Any": Any, "ClassVar": ClassVar, "Dict": Dict, "List": List,
           "Optional": Optional, "Tuple": Tuple}
    ctree = ast.parse(COMMON_TEXT)
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name in RESULT_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), "common.py", "exec"), cns)
    for name in RESULT_CLASSES:
        CLS[name] = cns[name]
        ns[name] = cns[name]

    tree = ast.parse(SRC_TEXT)

    def exec_node(node, namespace=None):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), namespace if namespace is not None else ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()
    # _TooLarge(ExecuteResult)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "_TooLarge":
            exec_node(node)
    ns["_TooLarge"] = ns.get("_TooLarge")
    # ported module-level functions
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in PORTED_MODULE:
            exec_node(node)
    # ported class methods (dedent; decorators stripped by segment extraction)
    cls_node = next(n for n in tree.body
                    if isinstance(n, ast.ClassDef) and n.name == "ShellFileOperations")
    wanted = set(PORTED_INSTANCE) | set(PORTED_STATIC) | {"__init__"}
    for sub in cls_node.body:
        if isinstance(sub, ast.FunctionDef) and sub.name in wanted:
            seg = ast.get_source_segment(SRC_TEXT, sub)
            seg = textwrap.dedent(seg)
            code = compile(seg, SRC, "exec", flags=ANN_FLAG)
            exec(code, ns)
        elif isinstance(sub, ast.Assign):
            for t in sub.targets:
                if isinstance(t, ast.Name) and t.id in CLASS_CONSTS:
                    try:
                        exec_node(sub)
                    except Exception:
                        pass
    seed_consts()
    ns["logger"] = RecLogger()   # logger = logging.getLogger(...) is an Assign

    missing = [n for n in list(wanted) + PORTED_MODULE if n not in ns]
    if missing:
        print("ORACLE file_operations_flow: FATAL missing defs: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive ──
def mat_env(d):
    if not isinstance(d, dict):
        return d
    out = {}
    for k, v in d.items():
        if k == "config" and isinstance(v, dict):
            out[k] = types.SimpleNamespace(**v)
        else:
            out[k] = v
    return types.SimpleNamespace(**out)


def make_self(inp):
    slf = types.SimpleNamespace()
    slf.env = mat_env(inp.get("env") or {})
    slf.cwd = (inp.get("self") or {}).get("cwd", "/")
    for k in CLASS_CONSTS:
        if k in NS:
            setattr(slf, k, NS[k])
    for name in PORTED_INSTANCE:
        setattr(slf, name, types.MethodType(NS[name], slf))
    for name in PORTED_STATIC:
        setattr(slf, name, NS[name])
    for attr, rec in SEAM_METHODS.items():
        setattr(slf, attr, rec)
    return slf


def drive(ns, func, inp):
    amb = inp.get("amb") or {}
    ns["sys"].executable = amb.get("sys_executable", DEFAULT_SYS_EXECUTABLE)
    ns["LINTERS_INPROC"] = {e: linter_rec(e) for e in amb.get("linter_exts", DEFAULT_LINTER_EXTS)}
    ns["_FAIL_CLOSED_INPROC_EXTS"] = frozenset(amb.get("fail_closed_exts", DEFAULT_FAIL_CLOSED_EXTS))
    live = PY[func]
    if func == "init":
        slf = types.SimpleNamespace()
        env = mat_env(inp.get("env") or {})
        ns["__init__"](slf, env, inp.get("cwd"))
        state = {k: getattr(slf, k) for k in
                 ("cwd", "_command_cache", "_rg_resolution_cache", "_rg_modified_capability")}
        return None, state
    if func in PORTED_MODULE:
        if func == "_json_nonstandard_constant":
            return ns[live](inp.get("text", "")), None
        return ns[live](inp.get("path", ""), inp.get("content", ""), inp.get("pre_content")), None
    if func in PORTED_STATIC:
        if func == "_bounded":
            return ns[live](inp.get("encoder", ""), inp.get("arg", ""), inp.get("max_bytes")), None
        if func == "_failed_read":
            return ns[live](inp.get("path", ""), inp.get("payload", ""), inp.get("read_rc", 0)), None
        if func == "_fail_closed_syntax_error":
            return ns[live](inp.get("path", ""), inp.get("ext", ""), inp.get("content", "")), None
        return ns[live](bytes.fromhex(inp.get("data", "")), inp.get("size_segment", "")), None
    slf = make_self(inp)
    m = getattr(slf, live)
    if func == "_cat":
        return m(inp.get("path", "")), None
    if func == "_head":
        return m(inp.get("path", ""), inp.get("nbytes", 0)), None
    if func == "_fenced_read":
        return m(inp.get("body", ""), *(inp.get("more") or [])), None
    if func in ("_read_exact_bytes", "_read_exact_bytes_hex", "read_file_bytes"):
        return m(inp.get("path", ""), inp.get("max_bytes")), None
    if func == "_probe_regular_file":
        return m(inp.get("path", "")), None
    if func == "_env_unavailable_error":
        return m(inp.get("path", ""), inp.get("status", "env_unavailable")), None
    if func == "_detect_binary":
        return m(inp.get("path", "")), None
    if func == "_native_path":
        return m(inp.get("path", "")), None
    if func == "_python_interpreter_cmd":
        return m(), None
    if func == "_exec_python_snippet":
        return m(inp.get("snippet", ""), inp.get("py")), None
    if func == "_reject_unencodable":
        content = "".join(chr(c) for c in inp.get("content", []))
        return m(inp.get("path", ""), content), None
    if func == "_fail_closed_syntax_error":
        return m(inp.get("path", ""), inp.get("ext", ""), inp.get("content", "")), None
    if func == "_verify_written_hash":
        return m(inp.get("path", ""), bytes.fromhex(inp.get("content_bytes", ""))), None
    if func == "write_file":
        return m(inp.get("path", ""), inp.get("content", ""), inp.get("pre_content")), None
    if func == "_no_match_result":
        return m(inp.get("path", ""), inp.get("content", ""), inp.get("old_string", ""),
                 inp.get("new_string", ""), inp.get("match_count", 0), inp.get("error")), None
    if func == "_verify_patch_persisted":
        return m(inp.get("path", ""), inp.get("new_content", "")), None
    raise ValueError("unknown func: %s" % func)


def main():
    global NS
    NS = extract()
    install_module_stubs()
    cases = 0
    mism = 0
    per_func = {}
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
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(NS, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = None
        want = {"ret": proj(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = proj(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE file_operations_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE file_operations_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()