#!/usr/bin/env python3
"""Oracle for the runtime-helper FLOW-core port (src/cli/port_agent_runtime_helpers_flow.c).

Drives the LIVE agent/agent_runtime_helpers.py helper defs with SCRIPTED I/O:
every external call (scratchpad conversion, logger, clock, retry-after parse,
absolute-timestamp parse, entitlement probe) is a recording stub fed the same
scripts the C harness feeds its seams. Compares the DECISION SEQUENCE
(ordered seam calls {"name","args"} in "trace") plus the final return and —
for the reset flows — the mutated context state.

The live defs are AST-extracted (module constants seeded, Assign AND
AnnAssign, plain-Name targets only) so the heavy quarry import chain is never
imported. Fail-closed: missing defs or 0 cases => exit 2.
Summary line: 'ORACLE agent_runtime_helpers_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- _UNMERGEABLE sentinel -> {"__unmergeable__": true}.
- Non-finite floats -> {"__float__": "inf"|"-inf"|"nan"} on both sides.
- Integral floats normalize to ints on both sides before comparison.
- str()/repr() of CONTAINERS is out of fixture scope (flatten dict branch,
  entitlement haystack): fixtures carry scalars there.
- strip()/lower()/\\d are ASCII-scoped in the port; fixtures stay ASCII
  (Python's are unicode-wide). Embedded json.dumps number literals avoid
  integral-float forms ("1.0") and leading zeros (libjson double parse).
"""
import ast
import contextlib
import json
import math
import os
import re
import sys
import types
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "agent", "agent_runtime_helpers.py")

TARGETS = [
    "_with_think_block", "_flatten_content_text", "_merge_user_content",
    "_trajectory_gpt_prefix", "_trajectory_tool_call_turn",
    "_trajectory_tool_responses", "_duration_string_seconds",
    "_set_reset_from_retry_after", "_set_reset_from_vendor_headers",
    "_is_entitlement_403",
]
STATEFUL = {"_set_reset_from_retry_after", "_set_reset_from_vendor_headers"}

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
    ns = {
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "contextlib": contextlib, "math": math, "threading": __import__("threading"),
        "Any": Any, "Optional": Optional,
        "SimpleNamespace": types.SimpleNamespace,
    }
    tree = ast.parse(src)
    # module constants (Assign/AnnAssign, plain Name targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # all module-level defs (targets + real siblings); recorders overwrite
    # the I/O names per case afterwards.
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # re-seed constants after defs (lambda-valued constants need the defs)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    missing = [t for t in TARGETS if t not in ns]
    for cname in ("_UNMERGEABLE", "_DURATION_COMPONENT_RE", "_DURATION_UNIT_SECONDS",
                  "_VENDOR_RESET_HEADERS", "convert_scratchpad_to_think"):
        # convert_scratchpad_to_think is an imported name; seeded per case.
        if cname != "convert_scratchpad_to_think" and cname not in ns:
            missing.append(cname)
    if missing:
        print(f"FATAL oracle_agent_runtime_helpers_flow: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


# ── per-case script backend ───────────────────────────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "BlockingIOError": BlockingIOError, "ValueError": ValueError,
    "TypeError": TypeError, "RuntimeError": RuntimeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "Exception": Exception,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    errno = spec.get("errno")
    if errno is not None and issubclass(cls, OSError):
        return cls(errno, msg)
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
        # record BEFORE resolving the script (the C dispatcher records first)
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def canon(v):
    """Integral floats -> int; non-finite floats -> tagged; recurse."""
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


def run_case(func, inp):
    ctx = Ctx(inp)
    # Extracted defs share NS as their __globals__: install the per-case
    # recorders INTO NS (cases run sequentially; no copy would be visible).
    ns = NS
    ns["logger"] = types.SimpleNamespace(
        warning=lambda fmt, *a: ctx.call("logger.warning", {"fmt": fmt, "args": list(a)}),
        debug=_SilentLogger().debug, info=_SilentLogger().info,
        error=_SilentLogger().error,
    )
    ns["time"] = types.SimpleNamespace(time=lambda: ctx.call("time.time", {}))
    ns["convert_scratchpad_to_think"] = lambda content: ctx.call(
        "convert_scratchpad_to_think", {"content": content})
    ns["parse_retry_after_seconds"] = lambda value: ctx.call(
        "parse_retry_after_seconds", {"value": value})
    ns["_parse_absolute_timestamp"] = lambda value: ctx.call(
        "_parse_absolute_timestamp", {"value": value})

    out = {}
    ret = None
    try:
        if func == "_with_think_block":
            ret = ns["_with_think_block"](inp.get("content", ""))
        elif func == "_flatten_content_text":
            ret = ns["_flatten_content_text"](inp.get("content"))
        elif func == "_merge_user_content":
            r = ns["_merge_user_content"](inp.get("prev"), inp.get("cur"))
            ret = {"__unmergeable__": True} if r is ns["_UNMERGEABLE"] else r
        elif func == "_trajectory_gpt_prefix":
            ret = ns["_trajectory_gpt_prefix"](inp["msg"])
        elif func == "_trajectory_tool_call_turn":
            ret = ns["_trajectory_tool_call_turn"](inp["msg"])
        elif func == "_trajectory_tool_responses":
            blocks, j = ns["_trajectory_tool_responses"](inp["msg"], inp["messages"], inp["start"])
            ret = {"blocks": blocks, "next": j}
        elif func == "_duration_string_seconds":
            ret = ns["_duration_string_seconds"](inp.get("text", ""))
        elif func == "_set_reset_from_retry_after":
            context = dict(inp.get("context") or {})
            ns["_set_reset_from_retry_after"](context, inp.get("retry_after"))
            out["state"] = {"context": context}
        elif func == "_set_reset_from_vendor_headers":
            context = dict(inp.get("context") or {})
            ns["_set_reset_from_vendor_headers"](context, inp.get("headers"))
            out["state"] = {"context": context}
        elif func == "_is_entitlement_403":
            aspec = dict(inp.get("agent") or {})
            agent = types.SimpleNamespace(**aspec)
            agent._is_entitlement_failure = lambda error_context, status_code: ctx.call(
                "agent._is_entitlement_failure",
                {"error_context": error_context, "status_code": status_code})
            ret = ns["_is_entitlement_403"](agent, inp.get("status_code"), inp.get("error_context"))
        else:
            out["error"] = "unknown func"
            return out
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    out["ret"] = ret
    out["trace"] = ctx.trace
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
        print("FATAL oracle_agent_runtime_helpers_flow: 0 cases")
        print("ORACLE agent_runtime_helpers_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE agent_runtime_helpers_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
