#!/usr/bin/env python3
"""Oracle for the agent/learning_graph.py FLOW + leaf port
(src/agent/learning_graph_flow.c).

Drives the LIVE agent/learning_graph.py functions with SCRIPTED I/O: every
foreign sub-call (project_root_str, get_hermes_home) and every CLAIMED sibling
(build_skill_nodes, build_edges, _memory_cards, _memory_skill_edges,
density_stats, _related, _tokenize, _to_int_ts, _load_usage) is a recording stub
fed the same scripts the C harness feeds its seams. The UNCLAIMED leaves ported
here (_fm_field, memory_fingerprint, memory_node_id, _has_learning_signal) run
REAL inline. Claimed siblings ported here as helpers are seamed when composed
and driven REAL by their own direct cases. Compares the DECISION SEQUENCE
(ordered seam calls {"name","args"} in "trace"), the return ("ret") and the
echoed globals ("state").

Live defs are AST-extracted (module constants seeded, Assign AND AnnAssign;
SkillNode dataclass exec'd); the heavy quarry import chain is never imported.
Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
raises RuntimeError on both sides. Catch BaseException (scripted SystemExit /
KeyboardInterrupt / GeneratorExit propagate through `except Exception` sites).

Representation contract (shared with the C port header):
- SkillNode double -> nine-field dict, materialized to the real SkillNode.
- memory card double -> {source,timestamp,title,body,fingerprint} (a dict live).
- _tokenize ret / seam result -> token array (live set; both sides sort / set()).
- build_edges / _memory_skill_edges ret -> array of [a,b] (live list of tuples).
- density_stats top_categories -> array of [category,count].
- Integral floats normalize to ints; fixtures avoid integral-float literals and
  non-finite floats. ASCII scope. _to_int_ts domain: None/int/float/numeric
  string/ISO (naive, Z, +HH:MM); no underscores/hex/inf/nan, no >2^53.
Summary: 'ORACLE learning_graph_flow: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import re
import sys
import types
from collections import Counter
from dataclasses import dataclass, field
from datetime import datetime, timezone, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "learning_graph.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "learning_graph.py")

# Scrubbed-env safety: derive home from __file__ (repo is parent.parent).
_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "lg_home"))

PY = {
    "_fm_field": "_fm_field",
    "memory_fingerprint": "memory_fingerprint",
    "memory_node_id": "memory_node_id",
    "_has_learning_signal": "_has_learning_signal",
    "_related": "_related",
    "_tokenize": "_tokenize",
    "build_edges": "build_edges",
    "density_stats": "density_stats",
    "_to_int_ts": "_to_int_ts",
    "_memory_skill_edges": "_memory_skill_edges",
    "build_learning_graph": "build_learning_graph",
}
UNCLAIMED = {"_fm_field", "memory_fingerprint", "memory_node_id", "_has_learning_signal"}
FIELDS = ["name", "category", "source", "timestamp", "use_count",
          "state", "created_by", "pinned", "related"]
REQUIRED_CONSTS = ["_USAGE_TS_KEYS"]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    # Register a stub module so the SkillNode dataclass's cls.__module__ resolves
    # (dataclasses._is_type reads sys.modules[cls.__module__].__dict__). Exec into
    # the module's own __dict__ so annotation lookups see the seeded names.
    mod = types.ModuleType("agent.learning_graph")
    sys.modules["agent.learning_graph"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "agent.learning_graph",
        "hashlib": __import__("hashlib"), "json": json, "re": re,
        "Counter": Counter, "dataclass": dataclass, "field": field,
        "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "Path": Path, "Any": object, "Optional": object,
        # module-top imported collaborators (placeholders; seamed per case)
        "EXCLUDED_SKILL_DIRS": frozenset((
            ".git", ".github", ".hub", ".archive", ".curator_backups", ".locks",
            ".hermes-dist", ".venv", "venv", "node_modules", "site-packages",
            "__pycache__", ".tox", ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache")),
        "get_hermes_home": lambda: Path("/h"),
    })
    tree = ast.parse(src)

    def consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    consts()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SkillNode":
            exec_node(src, node, ns, SRC)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    missing = [t for t in PY.values() if t not in ns]
    for c in REQUIRED_CONSTS:
        if c not in ns:
            missing.append(c)
    if "SkillNode" not in ns:
        missing.append("SkillNode")
    if missing:
        print(f"FATAL oracle_learning_graph_flow: missing defs/consts: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) if nm in NS}
SKILLNODE = NS["SkillNode"]
CURRENT = {"ctx": None}

# ── per-case script backend (fail-closed presence) ─────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "FileNotFoundError": FileNotFoundError,
    "UnicodeDecodeError": UnicodeDecodeError, "IsADirectoryError": IsADirectoryError,
    "IndexError": IndexError, "OverflowError": OverflowError, "ImportError": ImportError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── materialization (script dict -> live object double) ────────────────────
def mat_node(d):
    if isinstance(d, dict):
        return SKILLNODE(**d)
    return d


def mat_nodes(r):
    if isinstance(r, dict):
        return {k: mat_node(v) for k, v in r.items()}
    return r


def mat_edges(r):
    if isinstance(r, list):
        return [tuple(e) if isinstance(e, list) else e for e in r]
    return r


def mat_tokens(r):
    if isinstance(r, list):
        return set(r)
    return r


def node_dict(n):
    return {f: getattr(n, f) for f in FIELDS}


def install_seams(ctx, skip):
    CURRENT["ctx"] = ctx
    ns = NS
    for nm, obj in REAL.items():
        ns[nm] = obj
    ns["get_hermes_home"] = lambda: Path(ctx.call("get_hermes_home", {}))

    def raw(name, proj):
        def f(*a, **k):
            return ctx.call(name, proj(*a, **k))
        return f

    seams = {
        "_tokenize": lambda text: mat_tokens(ctx.call("_tokenize", {"text": text})),
        "_related": raw("_related", lambda fm: {"fm": fm}),
        "_to_int_ts": raw("_to_int_ts", lambda value: {"value": value}),
        "build_edges": lambda nodes: mat_edges(ctx.call("build_edges", {"nodes": {k: node_dict(v) for k, v in nodes.items()}})),
        "density_stats": lambda nodes, edges: ctx.call("density_stats", {"nodes": {k: node_dict(v) for k, v in nodes.items()}, "edges": [list(e) for e in edges]}),
        "_memory_skill_edges": lambda memory_cards, skills: mat_edges(ctx.call("_memory_skill_edges", {"memory_cards": memory_cards, "skills": [node_dict(s) for s in skills]})),
        "build_skill_nodes": lambda roots: mat_nodes(ctx.call("build_skill_nodes", {"roots": [{"source": s, "root": p.as_posix()} for s, p in roots]})),
        "_memory_cards": lambda: ctx.call("_memory_cards", {}),
        "_load_usage": lambda: ctx.call("_load_usage", {}),
    }
    for nm, fn in seams.items():
        if nm == skip:
            continue
        ns[nm] = fn
    # build_learning_graph does `from hermes_cli._startup_fast import
    # project_root_str` INSIDE its body — resolves sys.modules, not NS. Stub the
    # package chain so the import binds the per-case recorder (fail-closed).
    hc = sys.modules.get("hermes_cli")
    if not isinstance(hc, types.ModuleType):
        hc = types.ModuleType("hermes_cli")
        hc.__path__ = []
        sys.modules["hermes_cli"] = hc
    sf = types.ModuleType("hermes_cli._startup_fast")
    sf.project_root_str = lambda: ctx.call("project_root_str", {})
    hc._startup_fast = sf
    sys.modules["hermes_cli._startup_fast"] = sf


def canon(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, (set, frozenset)):
        return sorted(canon(x) for x in v)
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx, PY[func])
    ns = NS
    out = {}
    ret = None
    try:
        if func == "_fm_field":
            ret = ns["_fm_field"](inp.get("fm") or {}, inp.get("key", ""))
        elif func == "memory_fingerprint":
            ret = ns["memory_fingerprint"](inp.get("entry", ""))
        elif func == "memory_node_id":
            ret = ns["memory_node_id"](inp.get("card") or {}, inp.get("index", 0))
        elif func == "_has_learning_signal":
            ret = ns["_has_learning_signal"](mat_node(inp.get("node") or {}))
        elif func == "_related":
            ret = ns["_related"](inp.get("fm") or {})
        elif func == "_tokenize":
            ret = ns["_tokenize"](inp.get("text", ""))
        elif func == "build_edges":
            nodes = {k: mat_node(v) for k, v in (inp.get("nodes") or {}).items()}
            ret = ns["build_edges"](nodes)
        elif func == "density_stats":
            nodes = {k: mat_node(v) for k, v in (inp.get("nodes") or {}).items()}
            edges = [tuple(e) for e in (inp.get("edges") or [])]
            ret = ns["density_stats"](nodes, edges)
        elif func == "_to_int_ts":
            ret = ns["_to_int_ts"](inp.get("value"))
        elif func == "_memory_skill_edges":
            cards = inp.get("memory_cards") or []
            skills = [mat_node(v) for v in (inp.get("skills") or [])]
            ret = ns["_memory_skill_edges"](cards, skills)
        elif func == "build_learning_graph":
            ret = ns["build_learning_graph"]()
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["state"] = copy.deepcopy(inp.get("state") or {})
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
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:  # noqa: BLE001
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
        print("FATAL oracle_learning_graph_flow: 0 cases")
        print("ORACLE learning_graph_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE learning_graph_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


def seam_names():
    return ["_tokenize", "_related", "_to_int_ts", "build_edges", "density_stats",
            "_memory_skill_edges", "build_skill_nodes", "_memory_cards", "_load_usage",
            "get_hermes_home", "project_root_str"]


def self_test(fixpath):
    """Drive every fixture through run_case; flag drive failures and suspicious
    uniform AttributeError families naming a ported method/seam (binding bugs)."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    names = set(PY.values()) | set(seam_names())
    seen_funcs = set()
    problems = []
    n = 0
    for line in open(fixpath, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        func = rec["func"]
        seen_funcs.add(func)
        n += 1
        try:
            out = run_case(func, rec.get("in") or {})
        except BaseException as exc:  # noqa: BLE001
            problems.append(f"DRIVE-FAIL {func}: {type(exc).__name__}: {exc}")
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            for nm in names:
                if f"'{nm}'" in msg:
                    problems.append(f"SUSPECT-ATTR {func}: {msg}")
                    break
    missing = set(PY) - seen_funcs
    if missing:
        problems.append(f"FIXTURE missing funcs: {sorted(missing)}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELF-TEST learning_graph_flow: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "learning_graph_flow.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())