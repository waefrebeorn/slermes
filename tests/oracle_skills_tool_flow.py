#!/usr/bin/env python3
"""Oracle for the tools/skills_tool.py UNCLAIMED remainder
(src/tools/skills_tool_flow.c).

AST-extracts the 18 target defs, seeds module constants + the imported
plugin/setup/helpers as recording seams, and replaces every CLAIMED sibling and
foreign-module call with a recording stub returning the SAME scripted results
the C harness feeds its universal seam. Ported targets run INLINE (real
extracted defs) and compose with each other; pure Path algebra runs REAL on
PurePosixPath doubles (ns["Path"] = PurePosixPath so str() keeps '/' on Windows)
whose FS-touching methods (exists/is_dir/is_file/resolve/rglob/glob) are
monkeypatched to fire seams. The injected state mirror is the module-level
_SKILLS_CACHE dict; the oracle lets live mutate it then norms it back to JSON.
Compares ordered seam trace + return + final state. BaseException is caught per
case; scripted raises round-trip bare (identity-matched to the Raise instance).

Input: stdin JSON lines from t_port_skills_tool_flow.
Summary: ORACLE skills_tool_flow: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import builtins
import contextlib
import contextvars
import copy
import enum
import functools
import json
import os
import posixpath
import sys
import types
import typing
from pathlib import Path, PurePosixPath, PureWindowsPath
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "skt_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tools", "skills_tool.py")

TARGETS = [
    "clear_skills_cache", "_dir_tree_signature", "bind_turn_secret_capture",
    "_skill_utils_delegate", "skill_search_dirs", "_skill_catalog", "_host_skills",
    "_legacy_portable_skill_name", "_resolve_plugin_skill", "_under_any",
    "_is_package_owned_markdown", "_collect_skill_candidates", "_skill_linked_files",
    "_skill_readiness", "_owning_search_dir", "locate_skill", "_owned_relative",
    "_log_security_warnings",
]

EXCLUDED_SKILL_DIRS = frozenset((
    ".git", ".github", ".hub", ".archive", ".curator_backups", ".locks", ".hermes-dist",
    ".venv", "venv", "node_modules", "site-packages", "__pycache__",
    ".tox", ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache",
))
SKILL_SUPPORT_DIRS = frozenset(("references", "templates", "assets", "scripts"))
INJECTION_PATTERNS = [
    "ignore previous instructions", "ignore all previous", "you are now",
    "disregard your", "forget your instructions", "new instructions:",
    "system prompt:", "<system>", "]]>",
]
TIER_PROJECT, TIER_LOCAL, TIER_CREATE_DIR, TIER_EXTERNAL = range(4)
AMBIGUOUS_SKILL_PREFIX = "Ambiguous skill name "


class SkillReadinessStatus(str, enum.Enum):
    AVAILABLE = "available"
    SETUP_NEEDED = "setup_needed"
    UNSUPPORTED = "unsupported"


CUR = {"func": None, "in": {}, "trace": [], "io": {}, "pos": {}, "raises": [], "ns": None}


def rec(ev):
    CUR["trace"].append(ev)


def make_exc(spec):
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "KeyError":
        try:
            key = ast.literal_eval(msg)
        except Exception:
            key = msg
        return KeyError(key)
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        # real builtin needs 5 ctor args; subclass it so isinstance/except still
        # match the real family while str(e) == the scripted message.
        real = getattr(builtins, kind)
        sub = type(kind, (real,), {"__str__": lambda self: msg})
        e = sub.__new__(sub)
        Exception.__init__(e, msg)
        return e
    cls = getattr(builtins, kind, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


class Raise:
    def __init__(self, spec):
        self.spec = spec
        self.exc = make_exc(spec)


def next_script(name):
    io = CUR["io"]
    if name not in io:
        return Raise({"__raise__": "RuntimeError", "message": "no script for " + name})
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            return Raise({"__raise__": "RuntimeError", "message": "script exhausted for " + name})
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam(name, args):
    r = next_script(name)
    if isinstance(r, Raise):
        CUR["raises"].append(r)
        rec({"ev": name, "args": args, "ret": r.spec})
        raise r.exc
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = make_exc(r)
        CUR["raises"].append(Raise(r))
        raise e
    return copy.deepcopy(r) if isinstance(r, (dict, list)) else r


# ── projection (seam args / ret / state) ─────────────────────────────────────
def proj(x):
    if isinstance(x, PurePosixPath) or isinstance(x, PureWindowsPath):
        return x.as_posix()
    if isinstance(x, Path):
        return x.as_posix()
    if isinstance(x, SkillReadinessStatus):
        return x.value
    if isinstance(x, BaseException):
        return str(x)
    if isinstance(x, (set, frozenset)):
        return sorted(proj(v) for v in x)
    if isinstance(x, dict):
        return {k: proj(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [proj(v) for v in x]
    return x


def P(s):
    return PurePosixPath(s) if isinstance(s, str) else s


# ── seam recorders ───────────────────────────────────────────────────────────
class SeamFn:
    def __init__(self, name, argfn=None, mat=None):
        self.name = name
        self.argfn = argfn
        self.mat = mat

    def __call__(self, *a, **k):
        args = self.argfn(a, k) if self.argfn else {}
        r = seam(self.name, args)
        return self.mat(r) if self.mat else r


def _mat_path(r):
    return P(r) if isinstance(r, str) else r


def _mat_paths(r):
    return [P(x) for x in r] if isinstance(r, list) else r


def _mat_roots(r):
    return [[t, P(d)] for t, d in r] if isinstance(r, list) else r


def _mat_set(r):
    return set(r or [])


def _mat_pm(r):
    return mat_pm(r)


ARGMAP = {
    "_skills_dir": lambda a, k: {},
    "get_skill_search_roots": lambda a, k: {"local": proj(a[0])},
    "_get_disabled_skill_names": lambda a, k: {},
    "_skills_scan_signature": lambda a, k: {"dirs": proj(a[0]), "disabled": proj(a[1])},
    "monotonic": lambda a, k: {},
    "iter_project_skill_files": lambda a, k: {"dir": proj(a[0])},
    "iter_skill_index_files": lambda a, k: {"dir": proj(a[0]), "filename": a[1]},
    "_read_skill_text": lambda a, k: {"path": proj(a[0])},
    "_parse_frontmatter": lambda a, k: {"content": proj(a[0])},
    "_truncate_description": lambda a, k: {"description": proj(a[0])},
    "_get_category_from_path": lambda a, k: {"path": proj(a[0])},
    "skill_matches_platform": lambda a, k: {"frontmatter": proj(a[0])},
    "skill_matches_environment": lambda a, k: {"frontmatter": proj(a[0])},
    "skill_matches_apps": lambda a, k: {"frontmatter": proj(a[0])},
    "resolve_skill_catalog": lambda a, k: {"entries": proj(a[0])},
    "is_disabled_entry": lambda a, k: {"entry": proj(a[0]), "disabled": proj(a[1])},
    "_safe_frontmatter": lambda a, k: {"path": proj(a[0]) if a else None, "content": proj(k.get("content"))},
    "_is_skill_support_path": lambda a, k: {"path": proj(a[0])},
    "parse_qualified_name": lambda a, k: {"name": a[0]},
    "is_valid_namespace": lambda a, k: {"candidate": a[0]},
    "discover_plugins": lambda a, k: {},
    "get_plugin_manager": lambda a, k: {},
    "pm.find_plugin_skill": lambda a, k: {"name": a[0]},
    "pm.remove_plugin_skill": lambda a, k: {"name": a[0]},
    "pm.list_plugin_skills": lambda a, k: {"namespace": a[0]},
    "_get_active_memory_provider": lambda a, k: {},
    "_prune_inactive_memory_provider_skills": lambda a, k: {"provider": proj(a[0])},
    "load_memory_provider": lambda a, k: {"namespace": a[0]},
    "_serve_plugin_skill": lambda a, k: {"skill_md": proj(a[0]), "namespace": a[1], "bare": a[2],
                                         "file_path": proj(k.get("file_path")),
                                         "preprocess": k.get("preprocess", True),
                                         "session_id": proj(k.get("session_id"))},
    "_fail": lambda a, k: {"error": a[0], "extra": dict(k)},
    "host_feature_refusal": lambda a, k: {"feature": a[0], "fn_name": a[1]},
    "fn_call": lambda a, k: {"name": a[0], "args": proj(a[1]), "kwargs": proj(a[2])},
    "terminal_env": lambda a, k: {"key": a[0], "default": a[1]},
    "load_env": lambda a, k: {},
    "_is_env_var_persisted": lambda a, k: {"name": a[0], "snapshot": proj(a[1])},
    "_get_required_environment_variables": lambda a, k: {"frontmatter": proj(a[0])},
    "_capture_required_environment_variables": lambda a, k: {"skill_name": a[0], "missing": proj(a[1])},
    "register_env_passthrough": lambda a, k: {"names": proj(a[0])},
    "register_credential_files": lambda a, k: {"files": proj(a[0])},
    "_build_setup_note": lambda a, k: {"status": proj(a[0]), "missing": proj(a[1]), "setup_help": proj(a[2])},
    "_is_remote_env_backend": lambda a, k: {"backend": a[0]},
    "pick_skill_candidate": lambda a, k: {"candidates": proj(a[0])},
    "skill_candidate_rank": lambda a, k: {"skill_md": proj(a[0]), "root": proj(a[1])},
    "is_quarantined_project_skill": lambda a, k: {"skill_md": proj(a[0])},
    "sort_skills": lambda a, k: {"skills": proj(a[0])},
    "find_all_skills": lambda a, k: {"skip_disabled": k.get("skip_disabled", False)},
    "os.stat": lambda a, k: {"path": proj(a[0])},
    "turn_capture.set": lambda a, k: {"callback": proj(a[0])},
    "turn_capture.reset": lambda a, k: {"token": proj(a[0])},
    "cm_body": lambda a, k: {},
    "skill_utils_call": lambda a, k: {"attr": a[0], "args": proj(a[1])},
}
MAT = {
    "_skills_dir": _mat_path,
    "get_skill_search_roots": _mat_roots,
    "_get_disabled_skill_names": _mat_set,
    "iter_project_skill_files": _mat_paths,
    "iter_skill_index_files": _mat_paths,
    "get_plugin_manager": _mat_pm,
    "pm.find_plugin_skill": _mat_path,
    "os.stat": lambda r: SimpleNamespace(st_mtime_ns=r.get("mtime_ns")) if isinstance(r, dict) else r,
}
# module-global seams installed into ns (claimed siblings + foreign helpers)
NS_SEAMS = [
    "_skills_dir", "_get_disabled_skill_names", "_skills_scan_signature",
    "_get_category_from_path", "skill_matches_platform", "skill_matches_environment",
    "skill_matches_apps", "_parse_frontmatter", "_read_skill_text",
    "_truncate_description", "_safe_frontmatter", "_is_skill_support_path",
    "_fail", "_serve_plugin_skill", "load_env", "_is_env_var_persisted",
    "_get_required_environment_variables", "_capture_required_environment_variables",
    "_build_setup_note", "_is_remote_env_backend", "sort_skills", "find_all_skills",
]
# skill_utils functions resolved through the stub module __getattr__
SU_FUNCS = [
    "get_skill_search_roots", "iter_project_skill_files", "iter_skill_index_files",
    "resolve_skill_catalog", "is_disabled_entry", "pick_skill_candidate",
    "skill_candidate_rank", "is_quarantined_project_skill", "parse_qualified_name",
    "is_valid_namespace",
]


class RecLogger:
    def _log(self, level, msg, *a, **k):
        return seam("logger." + level, {"msg": msg, "args": [proj(x) for x in a],
                                        "exc_info": bool(k.get("exc_info", False))})

    def debug(self, msg, *a, **k):
        return self._log("debug", msg, *a, **k)

    def info(self, msg, *a, **k):
        return self._log("info", msg, *a, **k)

    def warning(self, msg, *a, **k):
        return self._log("warning", msg, *a, **k)


class ContextVarDouble:
    def set(self, cb):
        return seam("turn_capture.set", {"callback": proj(cb)})

    def reset(self, token):
        return seam("turn_capture.reset", {"token": proj(token)})


class DirEntryDouble:
    def __init__(self, path, name):
        self.path = path
        self.name = name

    def is_dir(self):
        return seam("entry.is_dir", {"path": self.path, "name": self.name})


class ScandirCM:
    def __init__(self, entries):
        self.entries = entries

    def __enter__(self):
        return iter(self.entries)

    def __exit__(self, *a):
        return False


def mat_pm(d):
    if not isinstance(d, dict):
        return d
    plugins = {}
    for key, loaded in (d.get("_plugins") or {}).items():
        manifest = (loaded or {}).get("manifest") or {}
        plugins[key] = SimpleNamespace(manifest=SimpleNamespace(**manifest))
    pskills = {q: dict(e) for q, e in (d.get("_plugin_skills") or {}).items()}
    pm = SimpleNamespace(_plugins=plugins, _plugin_skills=pskills)
    pm.find_plugin_skill = SeamFn("pm.find_plugin_skill", ARGMAP["pm.find_plugin_skill"], MAT["pm.find_plugin_skill"])
    pm.remove_plugin_skill = SeamFn("pm.remove_plugin_skill", ARGMAP["pm.remove_plugin_skill"])
    pm.list_plugin_skills = SeamFn("pm.list_plugin_skills", ARGMAP["pm.list_plugin_skills"])
    return pm


# ── Path FS monkeypatch (class-level, reads CUR dynamically) ─────────────────
def _patch_path():
    PurePosixPath.exists = lambda self: seam("path.exists", {"path": self.as_posix()})
    PurePosixPath.is_dir = lambda self: seam("path.is_dir", {"path": self.as_posix()})
    PurePosixPath.is_file = lambda self: seam("path.is_file", {"path": self.as_posix()})

    def _resolve(self):
        r = seam("path.resolve", {"path": self.as_posix()})
        return P(r) if isinstance(r, str) else r
    PurePosixPath.resolve = _resolve

    def _rglob(self, pattern):
        r = seam("path.rglob", {"path": self.as_posix(), "pattern": pattern})
        return [P(x) for x in r] if isinstance(r, list) else r
    PurePosixPath.rglob = _rglob

    def _glob(self, pattern):
        r = seam("path.glob", {"path": self.as_posix(), "pattern": pattern})
        return [P(x) for x in r] if isinstance(r, list) else r
    PurePosixPath.glob = _glob


# ── sys.modules stubs for in-function imports ────────────────────────────────
def _install_import_stubs():
    def pkg(name):
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
        return m

    agent = pkg("agent")
    su = types.ModuleType("agent.skill_utils")
    su.EXCLUDED_SKILL_DIRS = EXCLUDED_SKILL_DIRS
    su.SKILL_SUPPORT_DIRS = SKILL_SUPPORT_DIRS
    su.TIER_PROJECT, su.TIER_LOCAL = TIER_PROJECT, TIER_LOCAL
    su.TIER_CREATE_DIR, su.TIER_EXTERNAL = TIER_CREATE_DIR, TIER_EXTERNAL
    su.AMBIGUOUS_SKILL_PREFIX = AMBIGUOUS_SKILL_PREFIX

    def su_getattr(attr):
        if attr in SU_FUNCS:
            return SeamFn(attr, ARGMAP[attr], MAT.get(attr))
        # delegate target (_skill_utils_delegate) rides skill_utils_call
        def call(*a):
            return seam("skill_utils_call", {"attr": attr, "args": proj(list(a))})
        return call
    su.__getattr__ = su_getattr
    sys.modules["agent.skill_utils"] = su
    agent.skill_utils = su

    hc = pkg("hermes_cli")
    hcp = types.ModuleType("hermes_cli.plugins")
    hcp.discover_plugins = SeamFn("discover_plugins", ARGMAP["discover_plugins"])
    hcp.get_plugin_manager = SeamFn("get_plugin_manager", ARGMAP["get_plugin_manager"], MAT["get_plugin_manager"])
    sys.modules["hermes_cli.plugins"] = hcp
    hc.plugins = hcp

    pl = pkg("plugins")
    plm = types.ModuleType("plugins.memory")
    plm._get_active_memory_provider = SeamFn("_get_active_memory_provider", ARGMAP["_get_active_memory_provider"])
    plm._prune_inactive_memory_provider_skills = SeamFn("_prune_inactive_memory_provider_skills", ARGMAP["_prune_inactive_memory_provider_skills"])
    plm.load_memory_provider = SeamFn("load_memory_provider", ARGMAP["load_memory_provider"])
    sys.modules["plugins.memory"] = plm
    pl.memory = plm

    tools = pkg("tools")
    for sub, nm in (("terminal_scope", "terminal_env"), ("env_passthrough", "register_env_passthrough"),
                    ("credential_files", "register_credential_files"), ("hosted_tool_policy", "host_feature_refusal")):
        m = types.ModuleType("tools." + sub)
        setattr(m, nm, SeamFn(nm, ARGMAP[nm]))
        sys.modules["tools." + sub] = m
        setattr(tools, sub, m)


# ── extraction ───────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("skills_tool_flow_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    os_double = SimpleNamespace(
        stat=SeamFn("os.stat", ARGMAP["os.stat"], MAT["os.stat"]),
        path=SimpleNamespace(join=posixpath.join),
    )

    def os_scandir(path):
        r = seam("os.scandir", {"path": proj(path)})
        entries = [DirEntryDouble(str(path), d["name"]) for d in r] if isinstance(r, list) else r
        return ScandirCM(entries)
    os_double.scandir = os_scandir
    ns.update({
        "json": json, "os": os_double, "posixpath": posixpath,
        "time": SimpleNamespace(monotonic=SeamFn("monotonic", ARGMAP["monotonic"])),
        "logging": SimpleNamespace(getLogger=lambda name: RecLogger()),
        "contextvars": contextvars, "functools": functools,
        "suppress": contextlib.suppress, "contextmanager": contextlib.contextmanager,
        "Path": PurePosixPath, "PurePosixPath": PurePosixPath, "PureWindowsPath": PureWindowsPath,
        "Any": typing.Any, "Dict": typing.Dict, "List": typing.List,
        "Optional": typing.Optional, "Tuple": typing.Tuple,
        "get_hermes_home": lambda: PurePosixPath("/h/hermes"),
        "SkillReadinessStatus": SkillReadinessStatus,
        "MAX_NAME_LENGTH": 64, "MAX_DESCRIPTION_LENGTH": 1024,
        "_INJECTION_PATTERNS": INJECTION_PATTERNS,
        "_EXCLUDED_SKILL_DIRS": EXCLUDED_SKILL_DIRS,
    })
    tree = ast.parse(src)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            with contextlib.suppress(Exception):
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TARGETS:
            m2 = copy.copy(node)
            m2.decorator_list = []
            exec(compile(ast.Module(body=[m2], type_ignores=[]), SRC, "exec", flags=flags), ns)
    for name in TARGETS:
        assert name in ns, "extraction missed %s" % name
    # imported / claimed siblings -> recorders
    for nm in NS_SEAMS:
        ns[nm] = SeamFn(nm, ARGMAP[nm], MAT.get(nm))
    ns["_is_skill_support_path"] = SeamFn("_is_skill_support_path", ARGMAP["_is_skill_support_path"])
    ns["logger"] = RecLogger()
    _install_import_stubs()
    _patch_path()
    CUR["ns"] = ns
    return ns


# ── cache key conversion (mirror string <-> live tuple) ──────────────────────
def cache_key_str(k):
    return "%s|%s" % (k[0], "true" if k[1] else "false")


def parse_cache_key(s):
    a, b = s.split("|", 1)
    return (a, b == "true")


def setup_case(ns, func, inp):
    CUR["func"] = func
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["raises"] = []
    ns["_turn_secret_capture"] = ContextVarDouble()
    cache = {}
    for ks, entry in ((inp.get("state") or {}).get("skills_cache") or {}).items():
        cache[parse_cache_key(ks)] = [copy.deepcopy(entry[0]), entry[1], copy.deepcopy(entry[2])]
    ns["_SKILLS_CACHE"] = cache


def norm_state(ns):
    out = {}
    for k, (sig, ts, rows) in ns["_SKILLS_CACHE"].items():
        out[cache_key_str(k)] = [proj(sig), ts, [proj(r) for r in rows]]
    return {"skills_cache": out}


# ── per-func drive ───────────────────────────────────────────────────────────
def drive(ns, func, inp):
    g = inp.get
    if func == "clear_skills_cache":
        return ns[func]()
    if func == "_dir_tree_signature":
        sig = []
        ns[func](g("root"), sig, depth=g("depth", 6))
        return sig
    if func == "bind_turn_secret_capture":
        cm = contextlib.contextmanager(ns[func])(g("callback"))
        with cm:
            seam("cm_body", {})
        return None
    if func == "_skill_utils_delegate":
        d = ns[func](g("attr"))
        r = d(*g("args", []))
        return {"attr": g("attr"), "name": d.__name__, "qualname": d.__qualname__, "result": r}
    if func == "skill_search_dirs":
        return ns[func]()
    if func == "_skill_catalog":
        return ns[func](skip_disabled=g("skip_disabled", False), include_hidden=g("include_hidden", False))
    if func == "_host_skills":
        fn_name = g("fn_name")

        def fn(*a, **k):
            return seam("fn_call", {"name": fn_name, "args": proj(list(a)), "kwargs": proj(k)})
        fn.__name__ = fn_name
        fn.__qualname__ = fn_name
        gated = ns[func](fn)
        r = gated(*g("args", []), **g("kwargs", {}))
        return {"name": gated.__name__, "result": r}
    if func == "_legacy_portable_skill_name":
        return ns[func](mat_pm(g("pm")), g("namespace"), g("bare"))
    if func == "_resolve_plugin_skill":
        return ns[func](g("name"), g("file_path"), g("task_id"), g("preprocess", True))
    if func == "_under_any":
        return ns[func](P(g("path")), [P(d) for d in g("dirs", [])])
    if func == "_is_package_owned_markdown":
        return ns[func](P(g("path")), P(g("search_root")))
    if func == "_collect_skill_candidates":
        return ns[func](g("name"), g("local_category_name"), [P(d) for d in g("all_dirs", [])])
    if func == "_skill_linked_files":
        sd = g("skill_dir")
        return ns[func](P(sd) if sd is not None else None)
    if func == "_skill_readiness":
        return ns[func](g("frontmatter"), g("skill_name"))
    if func == "_owning_search_dir":
        return ns[func](P(g("skill_md")), [P(d) for d in g("all_dirs", [])])
    if func == "locate_skill":
        roots = [[t, P(d)] for t, d in g("roots", [])]
        return ns[func](g("name"), g("local_category_name"), roots)
    if func == "_owned_relative":
        sd = g("skill_dir")
        return ns[func](P(sd) if sd is not None else None, P(g("skill_md")), [P(d) for d in g("all_dirs", [])])
    if func == "_log_security_warnings":
        return ns[func](g("name"), P(g("skill_md")), g("content"),
                        [P(d) for d in g("all_dirs", [])], P(g("active_skills_dir")))
    raise ValueError("unknown func: %s" % func)


def run_case(ns, func, inp):
    setup_case(ns, func, inp)
    try:
        ret = drive(ns, func, inp)
        want_ret = proj(ret)
    except BaseException as e:
        want_ret = None
        for r in CUR["raises"]:
            if e is r.exc:
                want_ret = r.spec
                break
        if want_ret is None:
            want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"], "state": norm_state(ns)}


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


def main():
    ns = extract()
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
        want = run_case(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE skills_tool_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())