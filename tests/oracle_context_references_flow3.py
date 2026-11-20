#!/usr/bin/env python3
"""Oracle for src/agent/context_references_flow3.c — agent/context_references.py
FLOW remainder BEYOND flow2 (LIVE authority:
C:/Users/eman5/hermes-agent/agent/context_references.py).

The 11 ported defs (+3 internal pure leaf helpers) are AST-extracted from the
quarry (no heavy agent.context_references import). Collaborators that other
PoP sets own, plus the foreign-module calls (regex finditer, filesystem /
pathlib / os.walk / subprocess, network, deny lists, token estimation, the
async gather fan-out, and the flow2/flow2-claimed leaves) are recording stubs
fed the SAME scripted results the C harness feeds its universal seam. Internal
composition and state transforms run REAL on both sides. Compares the ordered
decision sequence + return with order-sensitive canon(). Per-case
BaseException catch (SystemExit / KeyboardInterrupt / GeneratorExit /
CancelledError escape every `except Exception` and are compared as bare raise
specs). Full builtin exception hierarchy is materialised as REAL classes so
`except` binds by identity (incl. subprocess.TimeoutExpired and the quarry
LiveConnectionError).

Input: stdin JSON lines from t_port_context_references_flow3:
    {"func":..., "in":{...,"seams":{...}}, "out":{"ret":...,"trace":[...]}}
Summary: ORACLE context_references_flow3: N cases, M mismatches. Fail-closed (2)
on missing defs or zero cases.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script
returns a RuntimeError raise spec (presence test, NOT .get() is None); a
seams[name] ARRAY is a per-call queue (last repeats, empty -> None); a single
value is a constant; a scripted null is None; {"__raise__":cls,"msg":...}
scripts an exception (built by a REAL builtin class so `except` binds by
hierarchy identity).
"""
import ast
import asyncio
import builtins
import copy
import inspect
import json
import os
import pathlib
import re
import sys
import types
import warnings
warnings.filterwarnings("ignore", category=RuntimeWarning)

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = pathlib.Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cr3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "context_references.py")
SRC_TEXT = open(SRC, encoding="utf-8").read().replace("\r", "")
ANN_FLAG = __import__("__future__").annotations.compiler_flag

# func key -> live def name (the 11 FLOW remainder defs)
PY = {
    "parse_context_references": "parse_context_references",
    "preprocess": "preprocess_context_references",
    "preprocess_async": "preprocess_context_references_async",
    "expand_reference": "_expand_reference",
    "expand_git_reference": "_expand_git_reference",
    "fetch_url_content": "_fetch_url_content",
    "default_url_fetcher": "_default_url_fetcher",
    "resolve_path": "_resolve_path",
    "ensure_path_allowed": "_ensure_reference_path_allowed",
    "build_folder_listing": "_build_folder_listing",
    "iter_visible_entries": "_iter_visible_entries",
}
# internal pure leaves re-derived by the C port (flow2 owns their PoP)
LEAVES = ["_strip_trailing_punctuation", "_strip_reference_wrappers",
          "_parse_file_reference_value"]
TOP_DEFS = list(PY.values()) + LEAVES
ALL_FUNCS = set(PY)

CUR = {"scripts": {}, "idx": {}, "raised": [], "trace": []}

# ── FULL builtin exception hierarchy (real classes => except binds by identity) ──
_EXC_PAIRS = [
    ("BaseExceptionGroup", "BaseException"), ("ExceptionGroup", "BaseExceptionGroup"),
    ("GeneratorExit", "BaseException"), ("KeyboardInterrupt", "BaseException"),
    ("SystemExit", "BaseException"), ("Exception", "BaseException"),
    ("ArithmeticError", "Exception"), ("FloatingPointError", "ArithmeticError"),
    ("OverflowError", "ArithmeticError"), ("ZeroDivisionError", "ArithmeticError"),
    ("AssertionError", "Exception"), ("AttributeError", "Exception"),
    ("BufferError", "Exception"), ("EOFError", "Exception"),
    ("ImportError", "Exception"), ("ModuleNotFoundError", "ImportError"),
    ("LookupError", "Exception"), ("IndexError", "LookupError"), ("KeyError", "LookupError"),
    ("MemoryError", "Exception"), ("NameError", "Exception"), ("UnboundLocalError", "NameError"),
    ("OSError", "Exception"), ("BlockingIOError", "OSError"), ("ChildProcessError", "OSError"),
    ("ConnectionError", "OSError"), ("BrokenPipeError", "ConnectionError"),
    ("ConnectionAbortedError", "ConnectionError"), ("ConnectionRefusedError", "ConnectionError"),
    ("ConnectionResetError", "ConnectionError"), ("FileExistsError", "OSError"),
    ("FileNotFoundError", "OSError"), ("InterruptedError", "OSError"),
    ("IsADirectoryError", "OSError"), ("NotADirectoryError", "OSError"),
    ("PermissionError", "OSError"), ("ProcessLookupError", "OSError"), ("TimeoutError", "OSError"),
    ("ReferenceError", "Exception"), ("RuntimeError", "Exception"),
    ("NotImplementedError", "RuntimeError"), ("RecursionError", "RuntimeError"),
    ("StopAsyncIteration", "Exception"), ("StopIteration", "Exception"),
    ("SyntaxError", "Exception"), ("IndentationError", "SyntaxError"), ("TabError", "IndentationError"),
    ("SystemError", "Exception"), ("TypeError", "Exception"), ("ValueError", "Exception"),
    ("UnicodeError", "ValueError"), ("UnicodeDecodeError", "UnicodeError"),
    ("UnicodeEncodeError", "UnicodeError"), ("UnicodeTranslateError", "UnicodeError"),
    ("Warning", "Exception"), ("BytesWarning", "Warning"), ("DeprecationWarning", "Warning"),
    ("EncodingWarning", "Warning"), ("FutureWarning", "Warning"), ("ImportWarning", "Warning"),
    ("PendingDeprecationWarning", "Warning"), ("ResourceWarning", "Warning"),
    ("RuntimeWarning", "Warning"), ("SyntaxWarning", "Warning"), ("UserWarning", "Warning"),
    ("LiveConnectionError", "Exception"), ("SubprocessError", "Exception"),
    ("TimeoutExpired", "SubprocessError"), ("CalledProcessError", "SubprocessError"),
]
EXC = {"BaseException": BaseException}
for _n, _p in _EXC_PAIRS:
    _base = getattr(builtins, _n, None)
    if isinstance(_base, type) and issubclass(_base, BaseException):
        EXC[_n] = _base
    else:
        EXC[_n] = type(_n, (EXC.get(_p, Exception),), {})
EXC["CancelledError"] = getattr(asyncio, "CancelledError",
                                 type("CancelledError", (BaseException,), {}))
_UNICODE_ERRS = ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError")


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    C = EXC.get(cls)
    if C is None:
        C = type(cls, (Exception,), {})
        EXC[cls] = C
    try:
        if cls in _UNICODE_ERRS:
            e = C.__new__(C)
            Exception.__init__(e, msg)
            return e
        return C(msg)
    except Exception:
        e = C.__new__(C)
        try:
            BaseException.__init__(e, msg)
        except Exception:
            pass
        return e


def seam(name, args):
    args = norm({k: v for k, v in (args or {}).items() if v is not None})
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r)) if r is not None else None
    CUR["trace"].append({"ev": name, "args": args, "ret": norm(r)})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──
class PathD:
    def __init__(self, s):
        self._spec = str(s)

    def __str__(self):
        return self._spec

    def __repr__(self):
        return self._spec

    def __eq__(self, o):
        return isinstance(o, PathD) and o._spec == self._spec

    def __hash__(self):
        return hash(self._spec)

    def __truediv__(self, other):
        o = str(other)
        if o.startswith("/"):
            return PathD(o)
        return PathD((self._spec.rstrip("/") + "/" + o) if self._spec else o)

    def expanduser(self):
        return self

    def resolve(self):
        return PathD(seam("resolve_path", {"path": self._spec}))

    def name(self):
        return self._spec.rstrip("/").rsplit("/", 1)[-1]

    @property
    def parts(self):
        return tuple(p for p in self._spec.split("/") if p)

    def relative_to(self, base):
        r = seam("relative_to", {"path": self._spec, "base": str(base)})
        return PathD(r)

    @property
    def parents(self):
        out = []
        s = self._spec.rstrip("/")
        while "/" in s:
            s = s.rsplit("/", 1)[0]
            if s:
                out.append(PathD(s))
        if self._spec.startswith("/"):
            out.append(PathD("/"))
        return out

    def exists(self):
        return seam("exists", {"path": self._spec})

    def is_dir(self):
        # Deterministic filesystem double: a path with no extension in its final
        # component is a directory. Pure (no seam) so the sorted()-key call order
        # over a set of Paths is not observable.
        name = self._spec.rstrip("/").rsplit("/", 1)[-1]
        return "." not in name

    def is_file(self):
        return not self.is_dir()


class EntryD:
    def __init__(self, d):
        self._d = d

    def __str__(self):
        return self._d.get("path", self._d.get("name", ""))

    @property
    def name(self):
        return self._d.get("name", "")

    def is_dir(self):
        return bool(self._d.get("is_dir", False))

    def relative_to(self, base):
        return types.SimpleNamespace(parts=[0] * int(self._d.get("relparts", 1)))


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, PathD):
        return norm(o._spec)
    if isinstance(o, ContextRefD):
        return o.as_dict()
    if isinstance(o, ResultD):
        return o.as_dict()
    if isinstance(o, MatchD):
        return o._d
    if isinstance(o, EntryD):
        return norm(o._d)
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    return str(o)


class ContextRefD:
    def __init__(self, raw, kind, target, start, end, line_start=None, line_end=None):
        self.raw, self.kind, self.target = raw, kind, target
        self.start, self.end = start, end
        self.line_start, self.line_end = line_start, line_end

    def as_dict(self):
        return {"raw": norm(self.raw), "kind": norm(self.kind), "target": norm(self.target),
                "start": norm(self.start), "end": norm(self.end),
                "line_start": norm(self.line_start), "line_end": norm(self.line_end)}


class ResultD:
    def __init__(self, message, original_message, references=None, warnings=None,
                 injected_tokens=0, expanded=False, blocked=False):
        self.message = message
        self.original_message = original_message
        self.references = references if references is not None else []
        self.warnings = warnings if warnings is not None else []
        self.injected_tokens = injected_tokens
        self.expanded = expanded
        self.blocked = blocked

    def as_dict(self):
        return {"message": norm(self.message), "original_message": norm(self.original_message),
                "references": norm(self.references), "warnings": norm(self.warnings),
                "injected_tokens": norm(self.injected_tokens), "expanded": norm(self.expanded),
                "blocked": norm(self.blocked)}


class MatchD:
    def __init__(self, d):
        self._d = d

    def group(self, g):
        if g == 0:
            return self._d.get("g0", "")
        return self._d.get(g)

    def start(self):
        return self._d.get("start", 0)

    def end(self):
        return self._d.get("end", 0)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", str(o))


# ── recording stubs ──
class PatternD:
    def __init__(self, seam_name):
        self.seam_name = seam_name

    def finditer(self, message):
        r = seam(self.seam_name, {"message": message})
        return [MatchD(x) for x in (r or [])]


class SeamFn:
    def __init__(self, name, argfn):
        self.name, self.argfn = name, argfn

    def __call__(self, *a, **k):
        return seam(self.name, self.argfn(*a, **k))


class AsyncSeamFn:
    """async callable whose body fires the seam when awaited."""
    def __init__(self, name, argfn):
        self.name, self.argfn = name, argfn

    def __call__(self, *a, **k):
        async def _aw():
            return seam(self.name, self.argfn(*a, **k))
        return _aw()


def make_path_factory():
    return lambda s: PathD(s)


# ── extraction ──
NEEDS_QUOTING_RE = re.compile(r"""[\s()\[\]{}<>"'`]""")
FILE_VALUE_RE = re.compile(
    r'^(?:(?P<quote>`|"|\')(?P<qpath>.+?)(?P=quote)|(?P<path>.+?))'
    r'(?::(?P<start>\d+)(?:-(?P<end>\d+))?)?$'
)
TEXT_EXTENSIONS = (".py", ".md", ".txt", ".json", ".yaml", ".yml", ".toml", ".js", ".ts")


def extract():
    tree = ast.parse(SRC_TEXT)
    nodes = {}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TOP_DEFS:
            nodes[node.name] = node
    missing = [n for n in TOP_DEFS if n not in nodes]
    if missing:
        print("ORACLE context_references_flow3: FATAL AST node missing: %s" % missing, file=sys.stderr)
        sys.exit(2)
    for name in TOP_DEFS:
        nd = copy.copy(nodes[name])
        nd.decorator_list = []
        exec(compile(ast.Module(body=[nd], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), NS)
    missing = [n for n in TOP_DEFS if n not in NS]
    if missing:
        print("ORACLE context_references_flow3: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    for name in TOP_DEFS:
        REAL[name] = NS[name]
    return NS


NS = {"__name__": "context_references_flow3_extracted", "__file__": SRC}
REAL = {}


def install_modules(inp, func):
    """Register dotted stub modules the extracted defs import in-function."""
    # agent.file_safety (nt namespace / deny-key / read-block)
    m_af = types.ModuleType("agent.file_safety")
    m_af.is_nt_namespace_path = SeamFn("is_nt_namespace", lambda target: {"target": target})
    m_af._hermes_dirs = SeamFn("composer_roots_seam", lambda: {})
    m_af.get_cache_directory_mounts = SeamFn("cache_mounts", lambda: {})
    m_af.is_nt_namespace_path = SeamFn("is_nt_namespace", lambda target: {"target": target})
    c = inp.get("constants") or {}
    m_af.HOME_CREDENTIAL_DIRS = tuple(c.get("credential_dirs", []))
    m_af.deny_key = lambda p: str(p)
    m_af.deny_path_under = lambda p, b: (str(p) == str(b) or
                                         str(p).startswith(str(b).rstrip("/") + "/"))
    m_af.get_read_block_error = SeamFn("read_block", lambda path: {"path": str(path)})
    sys.modules["agent.file_safety"] = m_af
    agent = sys.modules.setdefault("agent", types.ModuleType("agent"))
    agent.file_safety = m_af
    # hermes_constants
    m_hc = types.ModuleType("hermes_constants")
    hh = inp.get("hermes_home", "")
    m_hc.get_hermes_home = lambda: PathD(hh)
    sys.modules["hermes_constants"] = m_hc
    # tools.hosted_tool_policy
    m_htp = types.ModuleType("tools.hosted_tool_policy")
    m_htp.host_feature_refusal = SeamFn(
        "host_refusal", lambda feature, label: {"feature": feature, "label": label})
    sys.modules["tools.hosted_tool_policy"] = m_htp
    # tools.web_tools
    m_wt = types.ModuleType("tools.web_tools")
    m_wt.web_extract_tool = AsyncSeamFn(
        "web_extract", lambda urls, format: {"urls": list(urls), "format": format})
    sys.modules["tools.web_tools"] = m_wt
    tools = sys.modules.setdefault("tools", types.ModuleType("tools"))
    tools.hosted_tool_policy = m_htp
    tools.web_tools = m_wt


def install_stubs(inp, func):
    for name in TOP_DEFS:
        NS[name] = REAL[name]
    st = inp.get("state") or {}
    c = inp.get("constants") or {}
    home = inp.get("home", "")
    hh = inp.get("hermes_home", "")
    install_modules(inp, func)

    NS["ContextReference"] = ContextRefD
    NS["ContextReferenceResult"] = ResultD
    NS["BUILTIN_PREFIXES"] = frozenset({"diff", "staged", "file", "folder", "git", "url"})
    NS["_context_reference_providers"] = {
        k: types.SimpleNamespace(
            expand=AsyncSeamFn("provider_expand", lambda target, kk=k: {"kind": kk, "target": target}))
        for k in (inp.get("providers") or {})
    }
    NS["_context_reference_owners"] = {}
    NS["_context_reference_lock"] = __import__("contextlib").nullcontext()
    # module constants required by extracted defs / leaves
    NS["_NEEDS_QUOTING"] = NEEDS_QUOTING_RE
    NS["TRAILING_PUNCTUATION"] = ",.;!?"
    NS["_OPENERS"] = {")": "(", "]": "[", "}": "{"}
    NS["_FILE_VALUE_PATTERN"] = FILE_VALUE_RE
    NS["_TEXT_EXTENSIONS"] = TEXT_EXTENSIONS
    NS["_GIT_REFERENCE_ARGS"] = {
        "diff": lambda ref: ["diff"],
        "staged": lambda ref: ["diff", "--staged"],
        "git": lambda ref: ["log", "-%d" % max(1, min(int(getattr(ref, "target", "") or "1"), 10)), "-p"],
    }
    NS["FILTER_DISCOVERY_FAILED"] = "git filter discovery failed"
    NS["_MAX_EXPANDED_REFERENCES"] = 16
    NS["COMPOSER_PASTES_DIRNAME"] = "composer-pastes"
    NS["_SENSITIVE_HOME_FILES"] = tuple(c.get("sensitive_home_files", []))
    NS["_SENSITIVE_HERMES_DIRS"] = tuple(c.get("sensitive_hermes_dirs", []))
    NS["HOME_CREDENTIAL_DIRS"] = tuple(c.get("credential_dirs", []))
    NS["Path"] = make_path_factory()
    def _os_walk_stub(path):
        rows = seam("os_walk", {"path": str(path)})
        if isinstance(rows, list):
            out = []
            for r in rows:
                if isinstance(r, dict):
                    out.append((r.get("root", ""), list(r.get("dirs") or []),
                                list(r.get("files") or [])))
                elif isinstance(r, (list, tuple)) and len(r) >= 3:
                    out.append((r[0], list(r[1]), list(r[2])))
            return out
        return rows

    NS["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(expanduser=lambda p: home if p == "~" else p),
        walk=_os_walk_stub)
    NS["subprocess"] = types.SimpleNamespace(
        TimeoutExpired=EXC["TimeoutExpired"],
        SubprocessError=EXC["SubprocessError"],
        Popen=SeamFn("popen", lambda *a, **k: {}),
        DEVNULL=-3, PIPE=-1,
        CompletedProcess=lambda *a, **k: types.SimpleNamespace(
            args=a[0] if a else None, returncode=a[1] if len(a) > 1 else 0,
            stdout=a[2] if len(a) > 2 else "", stderr=a[3] if len(a) > 3 else ""),
    )
    NS["asyncio"] = types.SimpleNamespace(
        get_running_loop=lambda: seam("get_running_loop", {}),
        run=lambda coro: asyncio.run(coro),
        gather=asyncio.gather,
    )
    if func == "preprocess_async":
        def _expand_ref_stub(ref, cwd, url_fetcher=None, allowed_root=None,
                             max_inline_tokens=None):
            async def _aw():
                return seam("expand_ref", {
                    "raw": ref.raw, "kind": ref.kind, "target": ref.target,
                    "cwd": str(cwd), "max_inline_tokens": max_inline_tokens})
            return _aw()
        NS["_expand_reference"] = _expand_ref_stub
    if func == "preprocess":
        def _preprocess_async_stub(message, **kw):
            async def _c():
                return seam("preprocess_async_result", {"message": message})
            return _c()
        NS["preprocess_context_references_async"] = _preprocess_async_stub
    NS["json"] = json
    NS["re"] = re
    NS["inspect"] = inspect
    # foreign helper seams
    NS["estimate_tokens_rough"] = SeamFn("estimate_tokens", lambda text: {"text": text})
    NS["format_bytes"] = lambda n: "%d bytes" % n
    NS["windows_hide_flags"] = lambda: 0
    NS["IS_WINDOWS"] = False
    NS["noninteractive_repo_git_env"] = SeamFn("noninteractive_env", lambda cwd: {"cwd": str(cwd)})
    NS["harden_git_argv"] = SeamFn("harden_argv", lambda args: {"args": list(args)})
    def _run_quiet_stub(cmd, cwd, timeout, env=None):
        sname = "run_rg" if func == "iter_visible_entries" else "run_quiet"
        r = seam(sname, {
            "cmd": list(cmd), "cwd": str(cwd), "timeout": timeout, "env": env})
        if isinstance(r, dict) and "__raise__" not in r:
            return types.SimpleNamespace(returncode=r.get("returncode", 0),
                                         stdout=r.get("stdout", ""),
                                         stderr=r.get("stderr", ""))
        return r  # raise spec / None

    NS["_run_quiet"] = _run_quiet_stub
    # module-level names the live defs read as globals (module-level imports are
    # not executed when we AST-extract only the function bodies)
    NS["deny_key"] = lambda p: str(p)
    NS["deny_path_under"] = lambda p, b: (str(p) == str(b) or
                                          str(p).startswith(str(b).rstrip("/") + "/"))
    NS["offline_file_access"] = __import__("contextlib").nullcontext
    NS["LiveConnectionError"] = EXC["LiveConnectionError"]
    # claimed siblings / leaves -> seams. Each stub is installed ONLY when the
    # function under test is NOT that def (never override the live def being driven).
    def _parse_refs_stub(message, plugins=True):
        raw = seam("parse_refs", {"message": message, "plugins": plugins})
        return [_ref_from(x) for x in (raw or [])]

    if func != "parse_context_references":
        NS["parse_context_references"] = _parse_refs_stub
    NS["_is_under"] = SeamFn(
        "is_under", lambda path, root: {"path": str(path), "root": str(root)})
    NS["_composer_paste_roots"] = SeamFn("composer_roots", lambda: {})
    NS["_agent_staged_path"] = SeamFn("agent_staged", lambda path: {"path": str(path)})
    def _iter_entries_stub(path, cwd, limit):
        raw = seam("iter_entries", {"path": str(path), "cwd": str(cwd), "limit": limit})
        return [EntryD(x) for x in (raw or [])]

    if func != "iter_visible_entries":
        NS["_iter_visible_entries"] = _iter_entries_stub
    def _meta_args(path):
        nm = getattr(path, "name", "")
        if callable(nm):
            nm = nm()
        return {"path": str(path), "name": nm}

    NS["_file_metadata"] = SeamFn("file_metadata", _meta_args)
    NS["_expand_path_reference"] = SeamFn(
        "expand_path",
        lambda ref, cwd, allowed_root=None, max_inline_tokens=None: {
            "raw": ref.raw, "kind": ref.kind, "target": ref.target,
            "cwd": str(cwd), "allowed_root": None if allowed_root is None else str(allowed_root),
            "max_inline_tokens": max_inline_tokens})
    if func != "fetch_url_content":
        NS["_fetch_url_content"] = AsyncSeamFn(
            "fetch_url", lambda url, url_fetcher=None: {"url": url})
    if func != "expand_git_reference":
        NS["_expand_git_reference"] = SeamFn(
            "expand_git", lambda ref, cwd, args, label: {
                "raw": ref.raw, "cwd": str(cwd), "args": list(args), "label": label})
    # `_default_url_fetcher` stays REAL: F7 drives it directly (its web_extract_tool
    # collaborator is the seam). F6 passes an explicit fetcher so the default is unused.
    # regex finditer seams (only F1 uses them)
    NS["REFERENCE_PATTERN"] = PatternD("builtin_matches")
    NS["_PLUGIN_REFERENCE_PATTERN"] = PatternD("plugin_matches")

    if func == "parse_context_references":
        pass  # leaves stay REAL (re-derived in C)
    # `_default_url_fetcher` is restored to the REAL extracted def above; no stub.


# ── drivers ──
def _ref_from(d):
    return ContextRefD(d.get("raw", ""), d.get("kind", ""), d.get("target", ""),
                       d.get("start", 0), d.get("end", 0),
                       d.get("line_start"), d.get("line_end"))


def drive(func, inp):
    try:
        if func == "parse_context_references":
            refs = NS["parse_context_references"](
                inp.get("message", ""), plugins=bool(inp.get("plugins", True)))
            return norm(refs)

        elif func == "preprocess":
            return norm(NS["preprocess_context_references"](
                inp.get("message", ""), cwd=inp.get("cwd", ""),
                context_length=inp.get("context_length", 0),
                url_fetcher=None, allowed_root=inp.get("allowed_root")))

        elif func == "preprocess_async":
            res = asyncio.run(NS["preprocess_context_references_async"](
                inp.get("message", ""), cwd=inp.get("cwd", ""),
                context_length=inp.get("context_length", 0),
                url_fetcher=None, allowed_root=inp.get("allowed_root"),
                plugins=bool(inp.get("plugins", True))))
            return norm(res)

        elif func == "expand_reference":
            ref = _ref_from(inp.get("ref") or {})
            res = asyncio.run(NS["_expand_reference"](
                ref, PathD(inp.get("cwd", "")),
                url_fetcher=None,
                allowed_root=(PathD(inp["allowed_root"]) if inp.get("allowed_root") is not None else None),
                max_inline_tokens=inp.get("max_inline_tokens")))
            return norm(res)

        elif func == "expand_git_reference":
            ref = _ref_from(inp.get("ref") or {})
            res = NS["_expand_git_reference"](
                ref, PathD(inp.get("cwd", "")), list(inp.get("args") or []),
                inp.get("label", ""))
            return norm(res)

        elif func == "fetch_url_content":
            def fetcher(url):
                r = seam("fetch", {"url": url})
                if isinstance(r, dict) and "__await__" in r:
                    val = r["__await__"]
                    async def _aw():
                        return val
                    return _aw()
                return r
            res = asyncio.run(NS["_fetch_url_content"](inp.get("url", ""), url_fetcher=fetcher))
            return norm(res)

        elif func == "default_url_fetcher":
            res = asyncio.run(NS["_default_url_fetcher"](inp.get("url", "")))
            return norm(res)

        elif func == "resolve_path":
            res = NS["_resolve_path"](PathD(inp.get("cwd", "")), inp.get("target", ""),
                                      allowed_root=(PathD(inp["allowed_root"])
                                                    if inp.get("allowed_root") is not None else None))
            return norm(res)

        elif func == "ensure_path_allowed":
            res = NS["_ensure_reference_path_allowed"](PathD(inp.get("path", "")))
            return norm(res)

        elif func == "build_folder_listing":
            res = NS["_build_folder_listing"](
                PathD(inp.get("path", "")), PathD(inp.get("cwd", "")),
                limit=inp.get("limit", 200),
                display_base=(PathD(inp["display_base"]) if inp.get("display_base") is not None else None))
            return norm(res)

        elif func == "iter_visible_entries":
            res = NS["_iter_visible_entries"](PathD(inp.get("path", "")),
                                              PathD(inp.get("cwd", "")),
                                              inp.get("limit", 200))
            return norm(res)

        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
        return norm(ret)


def main():
    extract()
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
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        CUR["trace"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        install_stubs(inp, func)
        ret = drive(func, inp)
        want = {"ret": ret, "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE context_references_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE context_references_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
