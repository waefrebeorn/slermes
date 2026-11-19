#!/usr/bin/env python3
"""Oracle for the status-phrases FLOW port (src/gateway/status_phrases_flow.c).

Drives the LIVE gateway/status_phrases.py defs with SCRIPTED I/O: every external
call (hermes_yaml.safe_load, the filesystem-touching pathlib.Path methods,
hermes_constants.get_hermes_home, random.choice) is a recording stub fed the same
scripts the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered
seam calls {"name","args"} in "trace") plus the final return and — for the
catalog/recent mutating flows — the mutated state.

The live defs are AST-extracted (module constants seeded, Assign AND AnnAssign,
plain-Name targets only) so the heavy quarry import chain is never imported. The
module-level _DEFAULT_PHRASES merge Expr is not an Assign and is skipped; the
global is injected per case from in.default_phrases. Fail-closed: missing defs or
0 cases => exit 2. Summary: 'ORACLE status_phrases_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Path is modeled as pathlib.PurePosixPath (posix semantics, deterministic
  cross-platform); its filesystem methods are seamed. Pure path algebra stays
  real and matches the C transcription (probe-verified py3.14.9).
- Paths reach traces/results as posix strings (as_posix).
- str()-projected positions (item/kind/mode/raw_path) carry no float values;
  strip()/lower() are ASCII-scoped; fixtures stay ASCII.
- Integral floats normalize to ints on both sides before comparison.
- Scripted raises project VERBATIM ({type,message}); unscripted live raises
  reconstruct {type, str(exc)}. Real builtin exception classes are built so the
  live `except Exception` swallows Exception subclasses and lets the
  BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) propagate.
"""
import ast
import builtins
import copy
import json
import math
import os
import sys
import types
from collections.abc import Mapping, MutableSequence
from pathlib import PurePosixPath
from typing import Any

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "gateway", "status_phrases.py")

TARGETS = [
    "_clean_phrase_list", "_merge_phrase_mapping", "_merge_phrase_file",
    "_iter_phrase_files", "_merge_phrase_paths", "_copy_catalog",
    "_merge_phrase_config", "resolve_status_phrase_catalog",
    "classify_status_context", "choose_status_phrase",
]
CATALOG_STATE = {"_merge_phrase_mapping", "_merge_phrase_file",
                 "_merge_phrase_paths", "_merge_phrase_config"}

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


# ── seamed Path double (posix semantics; fs methods record + return script) ──
_CTX = None


class SeamPath(PurePosixPath):
    def expanduser(self):
        return SeamPath(_CTX.call("Path.expanduser", {"path": self.as_posix()}))

    def resolve(self, *a, **k):
        return SeamPath(_CTX.call("Path.resolve", {"path": self.as_posix()}))

    def is_file(self):
        return _CTX.call("Path.is_file", {"path": self.as_posix()})

    def is_dir(self):
        return _CTX.call("Path.is_dir", {"path": self.as_posix()})

    def iterdir(self):
        return [SeamPath(p) for p in _CTX.call("Path.iterdir", {"path": self.as_posix()})]

    def read_text(self, encoding=None, errors=None):
        return _CTX.call("Path.read_text", {"path": self.as_posix(), "encoding": encoding})


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "Mapping": Mapping, "MutableSequence": MutableSequence, "Any": Any,
        "Path": SeamPath, "types": types, "json": json, "math": math,
        "SimpleNamespace": types.SimpleNamespace,
        # external collaborators (recorders installed per case)
        "yaml": types.SimpleNamespace(safe_load=lambda *a, **k: None),
        "_random": types.SimpleNamespace(choice=lambda *a, **k: None),
        "get_hermes_home": lambda: SeamPath("."),
        "__file__": SRC,
    }
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    seed_constants()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    seed_constants()
    missing = [t for t in TARGETS if t not in ns]
    for cname in ("_FALLBACK_PHRASES", "_DEFAULT_PHRASES", "_STATUS_SURFACES",
                  "_CONVENTIONAL_RELATIVE_PATHS", "_CONFIG_KEYS", "_YAML_SUFFIXES"):
        if cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_status_phrases_flow: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL_DEFAULT = NS["_DEFAULT_PHRASES"]


# ── per-case script backend (mirrors the C dispatcher) ──────────────────────
def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(msg)


class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []
        self.scripted = {}

    def next_entry(self, name):
        if name not in self.io:
            return {"__raise__": "RuntimeError", "message": f"no script for {name}"}
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return {"__raise__": "RuntimeError", "message": f"script exhausted for {name}"}
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        # record BEFORE resolving the script (the C dispatcher records first)
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                exc = make_exc(entry["raise"])
                self.scripted[id(exc)] = entry["raise"]
                raise exc
            return entry.get("ret")
        if isinstance(entry, dict) and "__raise__" in entry:
            spec = {"type": entry["__raise__"], "message": entry.get("message", "")}
            exc = make_exc(spec)
            self.scripted[id(exc)] = spec
            raise exc
        return entry


def proj_exc(exc, ctx):
    spec = ctx.scripted.get(id(exc))
    if spec is not None:
        return {"__raise__": spec.get("type", "RuntimeError"), "message": spec.get("message", "")}
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def _install(ctx, inp):
    """Install per-case recorders + injected globals into the shared NS."""
    global _CTX
    _CTX = ctx
    NS["yaml"] = types.SimpleNamespace(
        safe_load=lambda text: ctx.call("yaml.safe_load", {"text": text}))
    NS["_random"] = types.SimpleNamespace(
        choice=lambda candidates: ctx.call("random.choice", {"candidates": list(candidates)}))
    NS["get_hermes_home"] = lambda: SeamPath(ctx.call("get_hermes_home", {}))
    NS["Path"] = SeamPath
    dp = inp.get("default_phrases")
    NS["_DEFAULT_PHRASES"] = copy.deepcopy(dp) if isinstance(dp, dict) else copy.deepcopy(REAL_DEFAULT)


def run_case(func, inp):
    ctx = Ctx(inp)
    _install(ctx, inp)
    out = {}
    ret = None
    catalog = None
    recent = None
    try:
        if func == "_clean_phrase_list":
            ret = NS["_clean_phrase_list"](inp.get("value"))
        elif func == "_copy_catalog":
            ret = NS["_copy_catalog"](inp.get("catalog") or {})
        elif func == "_merge_phrase_mapping":
            catalog = copy.deepcopy(inp.get("catalog") or {})
            NS["_merge_phrase_mapping"](catalog, inp.get("section"),
                                        inherited_mode=inp.get("inherited_mode"))
        elif func == "_merge_phrase_file":
            catalog = copy.deepcopy(inp.get("catalog") or {})
            NS["_merge_phrase_file"](catalog, SeamPath(inp.get("path", "")),
                                     inherited_mode=inp.get("inherited_mode"))
        elif func == "_iter_phrase_files":
            paths = NS["_iter_phrase_files"](SeamPath(inp.get("base_dir", "")), inp.get("raw_path"))
            ret = [p.as_posix() for p in paths]
        elif func == "_merge_phrase_paths":
            catalog = copy.deepcopy(inp.get("catalog") or {})
            NS["_merge_phrase_paths"](catalog, inp.get("paths"),
                                      base_dir=SeamPath(inp.get("base_dir", "")),
                                      inherited_mode=inp.get("inherited_mode"))
        elif func == "_merge_phrase_config":
            catalog = copy.deepcopy(inp.get("catalog") or {})
            NS["_merge_phrase_config"](catalog, inp.get("section"),
                                       base_dir=SeamPath(inp.get("base_dir", "")))
        elif func == "resolve_status_phrase_catalog":
            ret = NS["resolve_status_phrase_catalog"](inp.get("user_config"), inp.get("platform_key"))
        elif func == "classify_status_context":
            ret = NS["classify_status_context"](inp.get("kind"), tool_name=inp.get("tool_name"),
                                                preview=inp.get("preview"), args=inp.get("args"))
        elif func == "choose_status_phrase":
            recent = copy.deepcopy(inp["recent"]) if inp.get("recent") is not None else None
            rng = None
            if inp.get("rng"):
                rng = types.SimpleNamespace(
                    choice=lambda c: ctx.call("random.choice", {"candidates": list(c)}))
            ret = NS["choose_status_phrase"](
                inp.get("kind"), tool_name=inp.get("tool_name"), preview=inp.get("preview"),
                args=inp.get("args"), recent=recent, rng=rng, catalog=inp.get("catalog"))
        elif func == "_load_default_phrases":
            catalog = NS["_copy_catalog"](NS["_FALLBACK_PHRASES"])
            p = SeamPath(inp.get("module_file", "")).resolve().parent / "assets" / "status_phrases.yaml"
            NS["_merge_phrase_file"](catalog, p, inherited_mode="replace")
            ret = catalog
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc, ctx)
    out["ret"] = ret
    out["trace"] = ctx.trace
    if func in CATALOG_STATE:
        out["state"] = {"catalog": catalog}
    elif func == "choose_status_phrase":
        out["state"] = {"recent": recent}
    return out


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        try:
            py_out = run_case(func, inp)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(py_out.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_status_phrases_flow: 0 cases")
        print("ORACLE status_phrases_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE status_phrases_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())