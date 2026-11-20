#!/usr/bin/env python3
"""Oracle for i18n_flow2.c — the agent/i18n.py FLOW remainder (LIVE authority):

  agent/i18n.py : _locales_dir, _bundled_surfaces, _normalize_lang,
      _flatten_into, _load_catalog, _config_language_cached,
      reset_language_cache, get_language, t
  agent/i18n_layers.py : clear_cache (composed REAL inside reset_language_cache)

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli / agent import), module constants seeded (Assign AND
AnnAssign, twice), and every claimed sibling (_current_home, _resolve_language,
resolve_language_id, _load_bundled — the cli_output_flow ports) plus every
foreign collaborator (os.getenv, Path.is_dir, project_root_str,
load_config_readonly, i18n_layers.scan_locale_dir / flatten / overlay_layer /
pack_layer) replaced by recording stubs returning the SAME scripted results
the C harness feeds its universal seam. Ported-to-ported calls compose REAL on
both sides (_normalize_lang inside _config_language_cached / t; _load_catalog
inside t; clear_cache inside reset_language_cache). Lazy imports resolve
through sys.modules stub modules (hermes_cli._startup_fast / hermes_cli.config).

The two @lru_cache defs run under an injectable store double with live
hit/miss/cache_clear semantics (key PRESENCE hits, a cached None hits; fixture
domain stays below maxsize so LRU eviction never fires). Path-valued results
materialize as PathDouble (a PurePosixPath subclass whose is_dir fires seams)
so str()/join keep POSIX separators on this Windows host; Path("") is ".".
_bundled_surfaces returns a real frozenset — norm() sorts it, matching the C
sorted-pair projection. Locks are silent CM doubles; logger is a silent
RecLogger (no seam events on either side).

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the real BaseException trio) are caught into a ret spec;
`except Exception` (_config_language_cached) and t's (KeyError, IndexError,
ValueError) format step swallow exactly their live kinds (full builtin
exception hierarchy in make_exc + the real quarry UnscopedSecretError).

Input: stdin JSON lines from t_port_i18n_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE i18n_flow2: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import functools
import json
import logging
import os
import sys
import threading
import types
import __future__
from pathlib import Path, PurePosixPath
from typing import Any

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE i18n_flow2: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

ANN_FLAG = __future__.annotations.compiler_flag
I18N_SRC = str(QUARRY / "agent" / "i18n.py")
LAYERS_SRC = str(QUARRY / "agent" / "i18n_layers.py")
SCOPE_SRC = str(QUARRY / "agent" / "secret_scope.py")

# harness dispatch name -> live def name (all identical here)
PY = {
    "_locales_dir": "_locales_dir",
    "_bundled_surfaces": "_bundled_surfaces",
    "_normalize_lang": "_normalize_lang",
    "_flatten_into": "_flatten_into",
    "_load_catalog": "_load_catalog",
    "_config_language_cached": "_config_language_cached",
    "reset_language_cache": "reset_language_cache",
    "get_language": "get_language",
    "t": "t",
    "clear_cache": "clear_cache",
}
I18N_TARGETS = ["_locales_dir", "_bundled_surfaces", "_normalize_lang",
                "_flatten_into", "_load_catalog", "_config_language_cached",
                "reset_language_cache", "get_language", "t"]
LAYERS_TARGETS = ["clear_cache"]

CUR = {"scripts": {}, "idx": {}, "trace": [], "echo": None}
REAL_EXC = {}   # real quarry exception classes (UnscopedSecretError)
LRU_STORES = {"_bundled_surfaces": {}, "_config_language_cached": {}}

# Full builtin exception hierarchy (Python 3.14) for make_exc.
BUILTIN_EXC = {
    "BaseException": BaseException,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "Exception": Exception,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError, "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "PythonFinalizationError": PythonFinalizationError,
    "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "_IncompleteInputError": _IncompleteInputError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
    "EnvironmentError": OSError, "IOError": OSError, "WindowsError": OSError,
}
_UNICODE_BYPASS = {"UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"}


def make_exc(kind, msg):
    cls = BUILTIN_EXC.get(kind) or REAL_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if kind in _UNICODE_BYPASS:
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
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
                r = copy.deepcopy(sc[min(i, len(sc) - 1)])
        else:
            r = copy.deepcopy(sc)
    # Snapshot args/ret at call time: the C harness json_copy's the script per
    # call and json_copy's the trace ret, so a flow that mutates the returned
    # dict in place (_load_catalog's merged.update) must not rewrite the trace
    # or contaminate a reused single-value script on the next call.
    CUR["trace"].append({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(r)})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PurePosixPath):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


# ── doubles ──

class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


class SilentCM:
    def __enter__(self): return self
    def __exit__(self, *a): return False


class PathDouble(PurePosixPath):
    """PurePosixPath whose filesystem probes fire seams; str()/join stay POSIX."""
    def is_file(self): return seam("path.is_file", {"path": str(self)})
    def is_dir(self): return seam("path.is_dir", {"path": str(self)})
    def iterdir(self):
        r = seam("path.iterdir", {"path": str(self)})
        return [PathDouble(p) for p in r]


def lru_double(maxsize=None, **kw):
    """Injectable functools.lru_cache double: key PRESENCE hits (a cached None
    hits), miss calls the wrapped def and stores the result, cache_clear drops
    the store. Fixture domain stays below maxsize (no eviction)."""
    def deco(f):
        name = f.__name__
        @functools.wraps(f)
        def wrapper(*args):
            store = LRU_STORES[name]
            key = args[0] if len(args) == 1 else args
            if key in store:
                return store[key]
            r = f(*args)
            store[key] = r
            return r
        def cache_clear():
            LRU_STORES[name].clear()
        wrapper.cache_clear = cache_clear
        wrapper.__wrapped__ = f
        return wrapper
    return deco


# ── stable recording stubs (read CUR at call time) ──

def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def current_home_rec():
    return seam("_current_home", {})


def resolve_language_rec(home):
    return seam("_resolve_language", {"home": home})


def resolve_language_id_rec(value, home=None):
    return seam("resolve_language_id", {"value": value, "home": home})


def load_bundled_rec(lang):
    return seam("_load_bundled", {"lang": lang})


def getenv_rec(key, default=None):
    return seam("os.getenv", {"key": key, "default": default})


def project_root_str_rec():
    return seam("project_root_str", {})


def load_config_readonly_rec():
    return seam("load_config_readonly", {})


def scan_locale_dir_rec(directory):
    return seam("scan_locale_dir", {"directory": str(directory)})


def flatten_rec(node, prefix="", out=None):
    return seam("flatten", {"node": node, "prefix": prefix, "out": out})


def overlay_layer_rec(home, lang, surface="core"):
    return seam("overlay_layer", {"home": str(home), "lang": lang, "surface": surface})


def pack_layer_rec(lang, surface="core"):
    return seam("pack_layer", {"lang": lang, "surface": surface})


OS_DOUBLE = types.SimpleNamespace(getenv=getenv_rec)

LAYERS_DOUBLE = types.SimpleNamespace(
    CORE_SURFACE="core", SURFACES=("core", "tui", "desktop"),
    scan_locale_dir=scan_locale_dir_rec,
    flatten=flatten_rec,
    overlay_layer=overlay_layer_rec,
    pack_layer=pack_layer_rec,
    clear_cache=None,   # wired to the real extracted def below
)


def install_module_stubs():
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sys.modules["hermes_cli"] = hc
    sf = types.ModuleType("hermes_cli._startup_fast")
    sf.project_root_str = project_root_str_rec
    sys.modules["hermes_cli._startup_fast"] = sf
    hc._startup_fast = sf
    cfg = types.ModuleType("hermes_cli.config")
    cfg.load_config_readonly = load_config_readonly_rec
    sys.modules["hermes_cli.config"] = cfg
    hc.config = cfg


# ── extraction ──

def exec_node(node, ns, src, flags=ANN_FLAG):
    exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec", flags=flags), ns)


def seed_consts(tree, ns, src):
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
            exec_node(node, ns, src)
        except Exception:
            pass


def extract():
    # real UnscopedSecretError class from the quarry
    stree = ast.parse(open(SCOPE_SRC, encoding="utf-8").read())
    sns = {"__builtins__": builtins.__dict__}
    for node in stree.body:
        if isinstance(node, ast.ClassDef) and node.name == "UnscopedSecretError":
            exec_node(node, sns, SCOPE_SRC)
    if "UnscopedSecretError" not in sns:
        print("ORACLE i18n_flow2: FATAL missing UnscopedSecretError", file=sys.stderr)
        sys.exit(2)
    REAL_EXC["UnscopedSecretError"] = sns["UnscopedSecretError"]

    # ns_layers: clear_cache (composed real inside reset_language_cache)
    ns_layers = {"__name__": "i18n_layers_extracted", "__builtins__": builtins.__dict__,
                 "threading": threading, "logger": RecLogger(), "_lock": SilentCM(),
                 "_pack_cache": {}, "_overlay_cache": {}, "_overlay_files_cache": {}}
    ltree = ast.parse(open(LAYERS_SRC, encoding="utf-8").read())
    for node in ltree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LAYERS_TARGETS:
            exec_node(node, ns_layers, LAYERS_SRC)
    ns_layers["logger"] = RecLogger()
    ns_layers["_lock"] = SilentCM()
    LAYERS_DOUBLE.clear_cache = ns_layers["clear_cache"]

    # ns_i18n: the remainder flows
    ns_i18n = {"__name__": "i18n_extracted", "__builtins__": builtins.__dict__,
               "os": OS_DOUBLE, "logging": logging, "threading": threading,
               "Path": PathDouble, "Any": Any, "lru_cache": lru_double,
               "i18n_layers": LAYERS_DOUBLE, "language_options": lambda: [],
               "logger": RecLogger(),
               "_current_home": current_home_rec,
               "_resolve_language": resolve_language_rec,
               "resolve_language_id": resolve_language_id_rec,
               "_load_bundled": load_bundled_rec,
               "_catalog_lock": SilentCM()}
    itree = ast.parse(open(I18N_SRC, encoding="utf-8").read())
    seed_consts(itree, ns_i18n, I18N_SRC)
    for node in itree.body:
        if isinstance(node, ast.FunctionDef) and node.name in I18N_TARGETS:
            exec_node(node, ns_i18n, I18N_SRC)
    seed_consts(itree, ns_i18n, I18N_SRC)
    ns_i18n["logger"] = RecLogger()
    ns_i18n["_catalog_lock"] = SilentCM()

    missing = []
    missing += [n for n in I18N_TARGETS if n not in ns_i18n]
    missing += [n for n in LAYERS_TARGETS if n not in ns_layers]
    if missing:
        print("ORACLE i18n_flow2: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return {"i18n": ns_i18n, "layers": ns_layers}


# ── state conversion ──

def catalog_cache_to_real(d):
    return {tuple(k.split("\x1f")): copy.deepcopy(v) for k, v in (d or {}).items()}


def catalog_cache_to_json(d):
    return {"\x1f".join(k): v for k, v in d.items()}


def bundled_store_to_real(d):
    return {k: frozenset(tuple(p) for p in v) for k, v in (d or {}).items()}


def bundled_store_to_json(d):
    return {k: [list(p) for p in sorted(v)] for k, v in d.items()}


def overlay_files_to_real(d):
    return {k: frozenset(tuple(p) for p in v) for k, v in (d or {}).items()}


def overlay_files_to_json(d):
    return {k: [list(p) for p in sorted(v)] for k, v in d.items()}


# ── drive ──

def drive(NS, func, inp):
    CUR["echo"] = None
    i18n, layers = NS["i18n"], NS["layers"]

    if func == "_locales_dir":
        return i18n["_locales_dir"](), None
    if func == "_bundled_surfaces":
        store = bundled_store_to_real((inp.get("state") or {}).get("bundled_surfaces_cache"))
        LRU_STORES["_bundled_surfaces"] = store
        CUR["echo"] = lambda: {"bundled_surfaces_cache": bundled_store_to_json(store)}
        ret = i18n["_bundled_surfaces"](inp.get("locales_dir", ""))
        return ret, CUR["echo"]()
    if func == "_normalize_lang":
        return i18n["_normalize_lang"](inp.get("value"), inp.get("home")), None
    if func == "_flatten_into":
        node = copy.deepcopy(inp.get("node"))
        out = copy.deepcopy(inp.get("out"))
        CUR["echo"] = lambda: {"out": out}
        ret = i18n["_flatten_into"](node, inp.get("prefix", ""), out)
        return ret, CUR["echo"]()
    if func == "_load_catalog":
        real = catalog_cache_to_real((inp.get("state") or {}).get("catalog_cache"))
        i18n["_catalog_cache"] = real
        CUR["echo"] = lambda: {"catalog_cache": catalog_cache_to_json(real)}
        ret = i18n["_load_catalog"](inp.get("lang", ""), inp.get("home"))
        return ret, CUR["echo"]()
    if func == "_config_language_cached":
        store = copy.deepcopy((inp.get("state") or {}).get("config_lang_cache") or {})
        LRU_STORES["_config_language_cached"] = store
        CUR["echo"] = lambda: {"config_lang_cache": store}
        ret = i18n["_config_language_cached"](inp.get("hermes_home", ""))
        return ret, CUR["echo"]()
    if func == "reset_language_cache":
        st = inp.get("state") or {}
        cat = catalog_cache_to_real(st.get("catalog_cache"))
        sup = {k: list(v) for k, v in (st.get("supported_cache") or {}).items()}
        cfgl = copy.deepcopy(st.get("config_lang_cache") or {})
        bstore = bundled_store_to_real(st.get("bundled_surfaces_cache"))
        pc = catalog_cache_to_real(st.get("pack_cache"))
        oc = catalog_cache_to_real(st.get("overlay_cache"))
        ofc = overlay_files_to_real(st.get("overlay_files_cache"))
        i18n["_catalog_cache"] = cat
        i18n["_supported_cache"] = sup
        LRU_STORES["_config_language_cached"] = cfgl
        LRU_STORES["_bundled_surfaces"] = bstore
        layers["_pack_cache"] = pc
        layers["_overlay_cache"] = oc
        layers["_overlay_files_cache"] = ofc
        CUR["echo"] = lambda: {
            "catalog_cache": catalog_cache_to_json(cat),
            "supported_cache": sup,
            "config_lang_cache": cfgl,
            "bundled_surfaces_cache": bundled_store_to_json(bstore),
            "pack_cache": catalog_cache_to_json(pc),
            "overlay_cache": catalog_cache_to_json(oc),
            "overlay_files_cache": overlay_files_to_json(ofc),
        }
        ret = i18n["reset_language_cache"]()
        return ret, CUR["echo"]()
    if func == "get_language":
        return i18n["get_language"](), None
    if func == "t":
        real = catalog_cache_to_real((inp.get("state") or {}).get("catalog_cache"))
        i18n["_catalog_cache"] = real
        CUR["echo"] = lambda: {"catalog_cache": catalog_cache_to_json(real)}
        ret = i18n["t"](inp.get("key", ""), inp.get("lang"), **(inp.get("kwargs") or {}))
        return ret, CUR["echo"]()
    if func == "clear_cache":
        st = inp.get("state") or {}
        pc = catalog_cache_to_real(st.get("pack_cache"))
        oc = catalog_cache_to_real(st.get("overlay_cache"))
        ofc = overlay_files_to_real(st.get("overlay_files_cache"))
        layers["_pack_cache"] = pc
        layers["_overlay_cache"] = oc
        layers["_overlay_files_cache"] = ofc
        CUR["echo"] = lambda: {
            "pack_cache": catalog_cache_to_json(pc),
            "overlay_cache": catalog_cache_to_json(oc),
            "overlay_files_cache": overlay_files_to_json(ofc),
        }
        ret = layers["clear_cache"]()
        return ret, CUR["echo"]()
    raise ValueError("unknown func: %s" % func)


def main():
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
        CUR["echo"] = None
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(NS, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = CUR["echo"]() if CUR["echo"] else None
        want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE i18n_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE i18n_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()