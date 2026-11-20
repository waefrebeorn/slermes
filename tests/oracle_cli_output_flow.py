#!/usr/bin/env python3
"""Oracle for cli_output_flow.c — the UNCLAIMED hermes_cli/cli_output.py
rendering-cluster FLOW cores + leaves (LIVE authority):

  hermes_cli/cli_output.py : print_truncated
  hermes_cli/secret_prompt.py : _write
  agent/i18n.py : _current_home, _config_language, _resolve_language,
      supported_languages, resolve_language_id, _load_bundled,
      surface_catalog, language_surfaces
  agent/i18n_layers.py : normalize_language_id, is_language_id, flatten,
      non_text_leaves, overlay_dir, load_locale_source, pack_layer,
      pack_info, overlay_layer, scan_locale_dir

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli / agent import), module constants seeded (Assign AND
AnnAssign, twice), and every claimed sibling (t, color, _normalize_lang,
_config_language_cached, _locales_dir, _load_catalog, get_language,
reset_language_cache, _flatten_into) plus every foreign collaborator
(get_hermes_home, get_secret, os.environ.get, parse_locale_file,
layered_languages, overlay_surfaces, pack_surfaces, overlay_layer, pack_layer,
layers.surface_catalog, _bundled_surfaces, Path.is_file/is_dir/iterdir,
sys.stdout.write/flush, builtin print) replaced by recording stubs returning the
SAME scripted results the C harness feeds its universal seam. Ported-to-ported
SAME-MODULE calls compose REAL on both sides; cross-module calls are seamed
(the i18n_layers module double carries recorders), except the pure
normalize_language_id leaf composed real inside resolve_language_id.

Path-valued results materialize as PathDouble (a PurePosixPath subclass whose
is_file/is_dir/iterdir fire seams) so str()/join keep POSIX separators on this
Windows host. Set-valued seam results materialize as OSet doubles whose union
iterates LEFT items first then NEW right items (live frozenset iteration is
hash-randomized; the only order-sensitive observable left — the language_surfaces
result dict key order — is sorted away by canon()). Locks are silent CM doubles;
logger is a silent RecLogger (no seam events on either side).

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the real BaseException trio) are caught into a ret spec;
`except Exception` / `except UnscopedSecretError` sites swallow exactly their
live kinds (full builtin exception hierarchy in make_exc).

Input: stdin JSON lines from t_port_cli_output_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE cli_output_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import dataclasses
import functools
import itertools
import json
import logging
import os
import re
import sys
import threading
import types
import __future__
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Any

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE cli_output_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cli_output_flow_home"))

ANN_FLAG = __future__.annotations.compiler_flag
CLI_SRC = str(QUARRY / "hermes_cli" / "cli_output.py")
SP_SRC = str(QUARRY / "hermes_cli" / "secret_prompt.py")
I18N_SRC = str(QUARRY / "agent" / "i18n.py")
LAYERS_SRC = str(QUARRY / "agent" / "i18n_layers.py")
COLORS_SRC = str(QUARRY / "hermes_cli" / "colors.py")
SCOPE_SRC = str(QUARRY / "agent" / "secret_scope.py")

# harness dispatch name -> live def name (all identical here)
PY = {
    "print_truncated": "print_truncated",
    "_write": "_write",
    "_current_home": "_current_home",
    "_config_language": "_config_language",
    "_resolve_language": "_resolve_language",
    "supported_languages": "supported_languages",
    "resolve_language_id": "resolve_language_id",
    "_load_bundled": "_load_bundled",
    "surface_catalog": "surface_catalog",
    "language_surfaces": "language_surfaces",
    "normalize_language_id": "normalize_language_id",
    "is_language_id": "is_language_id",
    "flatten": "flatten",
    "non_text_leaves": "non_text_leaves",
    "overlay_dir": "overlay_dir",
    "load_locale_source": "load_locale_source",
    "pack_layer": "pack_layer",
    "pack_info": "pack_info",
    "overlay_layer": "overlay_layer",
    "scan_locale_dir": "scan_locale_dir",
}
I18N_TARGETS = ["_current_home", "_config_language", "_resolve_language",
                "supported_languages", "resolve_language_id", "_load_bundled",
                "surface_catalog", "language_surfaces"]
LAYERS_TARGETS = ["normalize_language_id", "is_language_id", "flatten",
                  "non_text_leaves", "overlay_dir", "load_locale_source",
                  "pack_layer", "pack_info", "overlay_layer", "scan_locale_dir"]

CUR = {"scripts": {}, "idx": {}, "trace": [], "echo": None}
REAL_EXC = {}   # real quarry exception classes (UnscopedSecretError)

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
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
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
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
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


class OSet:
    """Ordered frozenset double: union iterates left items first then new right."""
    def __init__(self, items):
        self._items = list(dict.fromkeys(items))
    def __iter__(self): return iter(self._items)
    def __len__(self): return len(self._items)
    def __or__(self, other):
        out = list(self._items)
        for x in other:
            if x not in out:
                out.append(x)
        return OSet(out)
    def __ror__(self, other):
        return set(other) | set(self._items)


class StdoutDouble:
    def write(self, text): return seam("stdout.write", {"text": text})
    def flush(self): return seam("stdout.flush", {})


# ── stable recording stubs (read CUR at call time) ──

def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def t_rec(key, lang=None, **kw):
    a = {"key": key}
    if lang is not None:
        a["lang"] = lang
    if kw:
        a["kwargs"] = dict(kw)
    return seam("t", a)


def color_rec(text, *codes):
    return seam("color", {"text": text, "codes": list(codes)})


def print_rec(*a, **k):
    return seam("print", {"args": list(a)})


def get_hermes_home_rec():
    return seam("get_hermes_home", {})


def config_language_cached_rec(hermes_home):
    return seam("_config_language_cached", {"hermes_home": hermes_home})


def normalize_lang_rec(value, home=None):
    a = {"value": value}
    if home is not None:
        a["home"] = home
    return seam("_normalize_lang", a)


def get_secret_rec(name):
    return seam("get_secret", {"name": name})


def locales_dir_rec():
    r = seam("_locales_dir", {})
    return PathDouble(r) if isinstance(r, str) else r


def parse_locale_file_rec(path):
    return seam("parse_locale_file", {"path": str(path)})


def _pairs(r):
    return OSet([tuple(p) for p in r]) if isinstance(r, list) else r


def layered_languages_rec(home):
    r = seam("layered_languages", {"home": home})
    return OSet(r) if isinstance(r, list) else r


def overlay_surfaces_rec(home):
    return _pairs(seam("overlay_surfaces", {"home": str(home)}))


def pack_surfaces_rec():
    return _pairs(seam("pack_surfaces", {}))


def overlay_layer_rec(home, lang, surface="core"):
    return seam("overlay_layer", {"home": str(home), "lang": lang, "surface": surface})


def pack_layer_rec(lang, surface="core"):
    return seam("pack_layer", {"lang": lang, "surface": surface})


def layers_surface_catalog_rec(home, lang, surface="core"):
    return seam("layers.surface_catalog", {"home": str(home), "lang": lang, "surface": surface})


def bundled_surfaces_rec(locales_dir):
    return _pairs(seam("_bundled_surfaces", {"locales_dir": locales_dir}))


OS_DOUBLE = types.SimpleNamespace(
    environ=types.SimpleNamespace(get=lambda key: seam("environ.get", {"key": key})))

LAYERS_DOUBLE = types.SimpleNamespace(
    CORE_SURFACE="core", SURFACES=("core", "tui", "desktop"),
    layered_languages=layered_languages_rec,
    overlay_surfaces=overlay_surfaces_rec,
    pack_surfaces=pack_surfaces_rec,
    overlay_layer=overlay_layer_rec,
    pack_layer=pack_layer_rec,
    surface_catalog=layers_surface_catalog_rec,
    parse_locale_file=parse_locale_file_rec,
    scan_locale_dir=mk("scan_locale_dir", lambda a, k: {"directory": str(a[0])}),
    normalize_language_id=None,   # wired to the real extracted leaf below
)


def install_module_stubs():
    hc = types.ModuleType("hermes_constants")
    hc.get_hermes_home = get_hermes_home_rec
    sys.modules["hermes_constants"] = hc
    agent = types.ModuleType("agent")
    agent.__path__ = []
    sys.modules["agent"] = agent
    ss = types.ModuleType("agent.secret_scope")
    ss.UnscopedSecretError = REAL_EXC["UnscopedSecretError"]
    ss.get_secret = get_secret_rec
    sys.modules["agent.secret_scope"] = ss
    agent.secret_scope = ss


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
        print("ORACLE cli_output_flow: FATAL missing UnscopedSecretError", file=sys.stderr)
        sys.exit(2)
    REAL_EXC["UnscopedSecretError"] = sns["UnscopedSecretError"]

    # ns_cli: print_truncated + real Colors constants
    ns_cli = {"__name__": "cli_output_extracted", "__builtins__": builtins.__dict__,
              "t": t_rec, "color": color_rec, "print": print_rec}
    ctree = ast.parse(open(COLORS_SRC, encoding="utf-8").read())
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name == "Colors":
            exec_node(node, ns_cli, COLORS_SRC)
    clitree = ast.parse(open(CLI_SRC, encoding="utf-8").read())
    for node in clitree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "print_truncated":
            exec_node(node, ns_cli, CLI_SRC)

    # ns_sp: _write
    ns_sp = {"__name__": "secret_prompt_extracted", "__builtins__": builtins.__dict__,
             "sys": types.SimpleNamespace(stdout=StdoutDouble())}
    sptree = ast.parse(open(SP_SRC, encoding="utf-8").read())
    for node in sptree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_write":
            exec_node(node, ns_sp, SP_SRC)

    # ns_layers: pure leaves + pack/overlay state flows
    ns_layers = {"__name__": "i18n_layers_extracted", "__builtins__": builtins.__dict__,
                 "re": re, "logging": logging, "threading": threading, "Path": PathDouble,
                 "Any": Any, "Mapping": Mapping, "dataclass": dataclass, "field": field,
                 "count": itertools.count, "logger": RecLogger(),
                 "parse_locale_file": parse_locale_file_rec, "_lock": SilentCM()}
    ltree = ast.parse(open(LAYERS_SRC, encoding="utf-8").read())
    seed_consts(ltree, ns_layers, LAYERS_SRC)
    for node in ltree.body:
        if isinstance(node, ast.ClassDef) and node.name == "PackEntry":
            # real (non-string) annotations so @dataclass needs no sys.modules lookup
            exec_node(node, ns_layers, LAYERS_SRC, flags=0)
    for node in ltree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LAYERS_TARGETS:
            exec_node(node, ns_layers, LAYERS_SRC)
    seed_consts(ltree, ns_layers, LAYERS_SRC)
    ns_layers["logger"] = RecLogger()
    ns_layers["_lock"] = SilentCM()
    LAYERS_DOUBLE.normalize_language_id = ns_layers["normalize_language_id"]

    # ns_i18n: catalog-resolution flows
    ns_i18n = {"__name__": "i18n_extracted", "__builtins__": builtins.__dict__,
               "os": OS_DOUBLE, "logging": logging, "threading": threading, "Path": PathDouble,
               "Any": Any, "lru_cache": functools.lru_cache,
               "i18n_layers": LAYERS_DOUBLE, "language_options": lambda: [],
               "logger": RecLogger(),
               "_normalize_lang": normalize_lang_rec,
               "_config_language_cached": config_language_cached_rec,
               "_locales_dir": locales_dir_rec,
               "_bundled_surfaces": bundled_surfaces_rec,
               "_load_catalog": mk("_load_catalog", lambda a, k: {"lang": a[0]}),
               "t": t_rec,
               "get_language": mk("get_language", lambda a, k: {}),
               "reset_language_cache": mk("reset_language_cache", lambda a, k: {}),
               "_flatten_into": mk("_flatten_into", lambda a, k: {}),
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
    if "print_truncated" not in ns_cli: missing.append("print_truncated")
    if "_write" not in ns_sp: missing.append("_write")
    missing += [n for n in I18N_TARGETS if n not in ns_i18n]
    missing += [n for n in LAYERS_TARGETS if n not in ns_layers]
    if "PackEntry" not in ns_layers: missing.append("PackEntry")
    if missing:
        print("ORACLE cli_output_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return {"cli": ns_cli, "sp": ns_sp, "i18n": ns_i18n, "layers": ns_layers}


# ── state conversion ──

def sup_cache_to_real(d):
    return {k: tuple(v) for k, v in (d or {}).items()}


def sup_cache_to_json(d):
    return {k: list(v) for k, v in d.items()}


def pack_cache_to_real(d):
    return {tuple(k.split("\x1f")): copy.deepcopy(v) for k, v in (d or {}).items()}


def pack_cache_to_json(d):
    return {"\x1f".join(k): v for k, v in d.items()}


def packs_to_real(ns, lst):
    return [ns["PackEntry"](**copy.deepcopy(e)) for e in (lst or [])]


def packs_to_json(lst):
    return [dataclasses.asdict(e) for e in lst]


# ── drive ──

def drive(NS, func, inp):
    CUR["echo"] = None
    cli, sp, i18n, layers = NS["cli"], NS["sp"], NS["i18n"], NS["layers"]

    if func == "print_truncated":
        return cli["print_truncated"](inp.get("more"), inp.get("hint", "")), None
    if func == "_write":
        return sp["_write"](inp.get("text", "")), None
    if func == "_current_home":
        return i18n["_current_home"](), None
    if func == "_config_language":
        return i18n["_config_language"](), None
    if func == "_resolve_language":
        return i18n["_resolve_language"](inp.get("home", "")), None
    if func == "supported_languages":
        real = sup_cache_to_real((inp.get("state") or {}).get("supported_cache"))
        i18n["_supported_cache"] = real
        CUR["echo"] = lambda: {"supported_cache": sup_cache_to_json(real)}
        ret = i18n["supported_languages"](inp.get("home"))
        return ret, CUR["echo"]()
    if func == "resolve_language_id":
        real = sup_cache_to_real((inp.get("state") or {}).get("supported_cache"))
        i18n["_supported_cache"] = real
        CUR["echo"] = lambda: {"supported_cache": sup_cache_to_json(real)}
        ret = i18n["resolve_language_id"](inp.get("value"), inp.get("home"))
        return ret, CUR["echo"]()
    if func == "_load_bundled":
        return i18n["_load_bundled"](inp.get("lang", "")), None
    if func == "surface_catalog":
        return i18n["surface_catalog"](inp.get("lang", ""), inp.get("surface", "core")), None
    if func == "language_surfaces":
        return i18n["language_surfaces"](inp.get("home")), None
    if func == "normalize_language_id":
        return layers["normalize_language_id"](inp.get("value")), None
    if func == "is_language_id":
        return layers["is_language_id"](inp.get("value")), None
    if func == "flatten":
        node = copy.deepcopy(inp.get("node"))
        out = copy.deepcopy(inp.get("out")) if isinstance(inp.get("out"), dict) else None
        ret = layers["flatten"](node, inp.get("prefix", ""), out)
        return ret, {"out": ret}
    if func == "non_text_leaves":
        return layers["non_text_leaves"](copy.deepcopy(inp.get("node")), inp.get("prefix", "")), None
    if func == "overlay_dir":
        return layers["overlay_dir"](inp.get("home", "")), None
    if func == "load_locale_source":
        return layers["load_locale_source"](copy.deepcopy(inp.get("source"))), None
    if func == "pack_layer":
        st = inp.get("state") or {}
        packs = packs_to_real(layers, st.get("packs"))
        cache = pack_cache_to_real(st.get("pack_cache"))
        layers["_packs"] = packs
        layers["_pack_cache"] = cache
        CUR["echo"] = lambda: {"packs": packs_to_json(packs), "pack_cache": pack_cache_to_json(cache)}
        ret = layers["pack_layer"](inp.get("lang", ""), inp.get("surface", "core"))
        return ret, CUR["echo"]()
    if func == "pack_info":
        st = inp.get("state") or {}
        packs = packs_to_real(layers, st.get("packs"))
        layers["_packs"] = packs
        CUR["echo"] = lambda: {"packs": packs_to_json(packs)}
        ret = layers["pack_info"](inp.get("lang", ""))
        return ret, CUR["echo"]()
    if func == "overlay_layer":
        st = inp.get("state") or {}
        cache = pack_cache_to_real(st.get("overlay_cache"))
        layers["_overlay_cache"] = cache
        CUR["echo"] = lambda: {"overlay_cache": pack_cache_to_json(cache)}
        ret = layers["overlay_layer"](inp.get("home", ""), inp.get("lang", ""), inp.get("surface", "core"))
        return ret, CUR["echo"]()
    if func == "scan_locale_dir":
        return layers["scan_locale_dir"](inp.get("directory", "")), None
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
        print("ORACLE cli_output_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE cli_output_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
