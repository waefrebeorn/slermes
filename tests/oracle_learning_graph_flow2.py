#!/usr/bin/env python3
"""Oracle for the agent/learning_graph_render.py FLOW + leaf port
(src/cli/learning_graph_flow2.c).

Drives the LIVE learning_graph_render.py defs with SCRIPTED I/O: the one
foreign/non-portable sibling, format_date (claimed -> learning_graph_render_helpers.c),
is a recording stub fed the same scripts the C harness feeds its seam. The
sixteen unclaimed functions ported here run REAL inline. Compares the DECISION
SEQUENCE (ordered seam calls {"name","args"} in "trace"), the return ("ret")
and the echoed globals ("state").

Live defs are AST-extracted (module constants seeded, Assign AND AnnAssign;
the two nested row helpers and the ChartBucket.category method lifted out);
the heavy quarry import chain is never imported. Fail-closed: missing defs or
0 cases => exit 2. Catch BaseException (scripted BaseException-only raises
propagate through `except Exception` sites).

Representation contract (shared with the C port header):
- node: dict with id/kind/label/category/timestamp/useCount/pinned/memorySource.
- bucket: {"label","ts","rec","nodes"} -> a live _ChartBucket-style double.
- memory_lookup: id -> card map (body already materialized).
- _utc projects a datetime to [year,month,day,hour,minute,second].
- _fill_even_bins emits mutated buckets via out.state["buckets"].
- Integral floats normalize to ints; fixtures avoid integral-float-only
  literals where a str() distinction matters and non-finite floats. ASCII scope.
Summary: 'ORACLE learning_graph_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import sys
import textwrap
import types
from collections import Counter
from datetime import datetime, timezone, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "learning_graph_render.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "learning_graph_render.py")

_REPO = _HERE.parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

# The 16 ported funcs + the inline helpers the live bodies need.
PY = {
    "_lerp": "_lerp", "_is_memory": "_is_memory", "_node_id": "_node_id",
    "_utc": "_utc", "_lead_in": "_lead_in", "_visible_count": "_visible_count",
    "_node_raw_label": "_node_raw_label", "_node_ts": "_node_ts",
    "_node_card": "_node_card", "_skill_category_counts": "_skill_category_counts",
    "category": "category", "_period": "_period",
    "_fill_even_bins": "_fill_even_bins", "node_row": "node_row",
    "bucket_row": "bucket_row", "_bar_lengths": "_bar_lengths",
}
HELPERS = ["_clamp"]
WANT = sorted(set(PY.values()) | set(HELPERS))
UNCLAIMED = set(PY.values())

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag

MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]


def safe_strftime(dt, fmt):
    if fmt == "%b":
        return MONTHS[dt.month - 1]
    if fmt == "%b %Y":
        return "%s %d" % (MONTHS[dt.month - 1], dt.year)
    if fmt == "%Y":
        return "%04d" % dt.year
    return dt.strftime(fmt)


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return textwrap.dedent("\n".join(src.splitlines()[start - 1:node.end_lineno]))


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("agent.learning_graph_render")
    sys.modules["agent.learning_graph_render"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "agent.learning_graph_render",
        "math": math, "Counter": Counter,
        "datetime": datetime, "timezone": timezone, "UTC": UTC,
        "safe_strftime": safe_strftime,
        "Any": object, "Optional": object, "Iterable": object, "list": list,
        "tuple": tuple, "str": str, "int": int, "float": float,
        # claimed collaborator imported at module top; seamed per case below
        "memory_node_id": lambda card, idx: "",
    })
    tree = ast.parse(src)

    # module constants (Assign AND AnnAssign)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            flat = []
            for t in tgts:
                if isinstance(t, (ast.Tuple, ast.List)):
                    flat.extend(t.elts)
                else:
                    flat.append(t)
            if not all(isinstance(t, ast.Name) for t in flat):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass

    # collect the FunctionDefs we need, including nested/method ones
    found = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANT:
            found.setdefault(node.name, node)
    for name in WANT:
        if name not in found:
            print("FATAL oracle_learning_graph_flow2: no FunctionDef for %s" % name)
            sys.exit(2)
        try:
            exec_node(src, found[name], ns, SRC)
        except Exception as exc:
            print("FATAL oracle_learning_graph_flow2: exec %s: %s" % (name, exc))
            sys.exit(2)
    missing = [t for t in WANT if t not in ns]
    if missing:
        print("FATAL oracle_learning_graph_flow2: missing defs: %s" % missing)
        sys.exit(2)
    return ns


NS = extract()

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
            raise RuntimeError("no script for %s" % name)
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError("script exhausted for %s" % name)
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


def install_seams(ctx):
    NS["format_date"] = lambda ts: ctx.call("format_date", {"ts": ts})


class BucketDbl:
    """_ChartBucket-style double: the live methods read .nodes/.total/etc."""
    def __init__(self, d):
        d = d or {}
        self.label = d.get("label", "")
        self.ts = d.get("ts", 0.0)
        self.rec = d.get("rec", 1.0)
        self.nodes = [dict(n) for n in (d.get("nodes") or [])]

    @property
    def memories(self):
        return sum(1 for n in self.nodes if n.get("kind") == "memory")

    @property
    def skills(self):
        return len(self.nodes) - self.memories

    @property
    def total(self):
        return len(self.nodes)

    def category(self):
        return NS["category"](self)

    def as_dict(self):
        return {"label": self.label, "ts": self.ts, "rec": self.rec, "nodes": self.nodes}


def canon(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, bool):
        return v
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
    install_seams(ctx)
    state_extra = {}
    ret = None
    try:
        if func == "_lerp":
            ret = NS["_lerp"](inp.get("a", 0.0), inp.get("b", 0.0), inp.get("t", 0.0))
        elif func == "_is_memory":
            ret = NS["_is_memory"](inp.get("node") or {})
        elif func == "_node_id":
            ret = NS["_node_id"](inp.get("node") or {})
        elif func == "_utc":
            dt = NS["_utc"](inp.get("ts", 0.0))
            ret = [dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second]
        elif func == "_lead_in":
            ret = NS["_lead_in"](inp.get("ratio", 0.0))
        elif func == "_visible_count":
            ret = NS["_visible_count"](inp.get("reveal", 0.0), inp.get("n", 0))
        elif func == "_node_raw_label":
            ret = NS["_node_raw_label"](inp.get("node") or {})
        elif func == "_node_ts":
            ret = NS["_node_ts"](inp.get("node") or {})
        elif func == "_node_card":
            ret = NS["_node_card"](inp.get("node") or {})
        elif func == "_skill_category_counts":
            ret = NS["_skill_category_counts"](inp.get("nodes") or [])
        elif func == "category":
            ret = NS["category"](BucketDbl(inp.get("bucket")))
        elif func == "_period":
            ret = NS["_period"](inp.get("ts", 0.0), inp.get("granularity", "year"))
        elif func == "_fill_even_bins":
            buckets = [BucketDbl(b) for b in (inp.get("buckets") or [])]
            NS["_fill_even_bins"](buckets, inp.get("nodes") or [], inp.get("rec") or {})
            state_extra["buckets"] = [b.as_dict() for b in buckets]
        elif func == "node_row":
            NS["memory_lookup"] = inp.get("memory_lookup") or {}
            ret = NS["node_row"](inp.get("node") or {})
        elif func == "bucket_row":
            NS["memory_lookup"] = inp.get("memory_lookup") or {}
            NS["cmap"] = inp.get("cmap") or {}
            ret = NS["bucket_row"](inp.get("idx", 0), BucketDbl(inp.get("bucket")))
        elif func == "_bar_lengths":
            ret = NS["_bar_lengths"](BucketDbl(inp.get("bucket")),
                                     inp.get("max_total", 1), inp.get("bar_w", 0))
        else:
            return {"error": "unknown func"}
    except BaseException as exc:
        ret = proj_exc(exc)
    out = {"ret": ret, "trace": ctx.trace}
    state = copy.deepcopy(inp.get("state") or {})
    state.update(state_extra)
    out["state"] = state
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
            print("PYERR %s: unknown func" % func)
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:
            mism += 1
            print("PYERR %s: %s: %s" % (func, type(exc).__name__, exc))
            continue
        if c_out is None:
            mism += 1
            print("MISMATCH %s: missing C out" % func)
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print("MISMATCH %s keys=%s" % (func, bad))
            for key in bad:
                print("  C.%s=%s" % (key, json.dumps(canon(c_out.get(key)), default=str)[:800]))
                print("  PY.%s=%s" % (key, json.dumps(canon(py_out.get(key)), default=str)[:800]))
    if cases == 0:
        print("FATAL oracle_learning_graph_flow2: 0 cases")
        print("ORACLE learning_graph_flow2: 0 cases, 0 mismatches")
        return 2
    print("ORACLE learning_graph_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
