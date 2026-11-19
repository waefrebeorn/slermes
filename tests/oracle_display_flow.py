#!/usr/bin/env python3
"""Oracle for the agent/display.py FLOW + leaf port (src/agent/display_flow.c).

Drives the LIVE agent/display.py presentation flows with SCRIPTED I/O: every
foreign sub-call (i18n t, skin engine, tool-label registry, safe_json_loads,
logger) and every CLAIMED sibling is a recording stub fed the same scripts the
C harness feeds its seams. Unclaimed siblings that are themselves ports here
are seamed when composed and driven REAL by their own direct cases. Compares
the DECISION SEQUENCE (ordered seam calls {"name","args"} in "trace"), the
return ("ret") and the echoed globals ("state").

Live defs are AST-extracted (module constants seeded, Assign AND AnnAssign);
the heavy quarry import chain is never imported. Fail-closed: missing defs or
0 cases => exit 2; a missing/exhausted seam script raises RuntimeError on both
sides. Catch BaseException (scripted SystemExit/KeyboardInterrupt/GeneratorExit/
CancelledError propagate through `except Exception` sites).

Representation contract (shared with the C port header):
- _CuteRow -> {"__cute_row__":true,"text","emoji","verb","detail"} (materialized
  to the real extracted _CuteRow class so isinstance checks behave).
- tool label double -> {"name","emoji","text","preview"} (materialized to a
  local ToolLabel dataclass so dataclasses.replace works; defaults "").
- _detect_tool_failure seam result -> 2-element array [is_failure, suffix].
- _memory_first_op seam result -> {"op","more"} (materialized to a tuple).
- get_tool_row_parts ret -> list of [emoji, verb, detail, failure].
- Integral floats normalize to ints; fixtures avoid integral-float literals and
  non-finite floats. ASCII scope for strip/lstrip/lower/len/slice; the emoji /
  box-drawing / arrow glyphs are verbatim UTF-8 constants from display.py.
Summary: 'ORACLE display_flow: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import re
import sys
import types
from dataclasses import dataclass, replace
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "display.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "display.py")

PY = {
    "_memory_first_op": "_memory_first_op",
    "tool_labels_for_call": "tool_labels_for_call",
    "bridge_tool_labels": "bridge_tool_labels",
    "tool_row_emoji": "tool_row_emoji",
    "_cute_row": "_cute_row",
    "_cute_fallback_row": "_cute_fallback_row",
    "_cute_web_extract": "_cute_web_extract",
    "_cute_todo_list": "_cute_todo_list",
    "_cute_memory": "_cute_memory",
    "_cute_skill_view": "_cute_skill_view",
    "_cute_cronjob": "_cute_cronjob",
    "_cute_execute_code": "_cute_execute_code",
    "_cute_browser_exec": "_cute_browser_exec",
    "_cute_delegate": "_cute_delegate",
    "_cute_process_manage": "_cute_process_manage",
    "_cute_scroll": "_cute_scroll",
    "_cute_bridge_rows": "_cute_bridge_rows",
    "_entry_failure_suffix": "_entry_failure_suffix",
    "_bridge_row_suffixes": "_bridge_row_suffixes",
    "get_tool_row_parts": "get_tool_row_parts",
    "_skin_spinner_list": "_skin_spinner_list",
    "_catalog_thinking_verbs": "_catalog_thinking_verbs",
    "_clear_line_blanks": "_clear_line_blanks",
}
METHODS = {"_skin_spinner_list", "_catalog_thinking_verbs", "_clear_line_blanks"}
REQUIRED_CONSTS = ["_CUTE_LINES", "_SCROLL_ARROWS", "_TOOL_VERBS_NO_PREVIEW",
                   "_TOOL_VERBS_FOR_CONNECTOR", "_BRIDGE_CALL_INDEX_RE"]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


@dataclass
class ToolLabel:
    name: str = ""
    emoji: str = ""
    text: str = ""
    preview: str = ""


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "agent.display",
        "re": re, "os": os, "sys": sys, "json": json, "math": math,
        "logging": __import__("logging"), "threading": __import__("threading"),
        "time": __import__("time"), "types": types,
        "dataclass": dataclass, "field": __import__("dataclasses").field,
        "replace": replace,
        "unified_diff": __import__("difflib").unified_diff,
        "Path": Path, "Any": object, "Iterator": object,
        "urlsplit": __import__("urllib.parse", fromlist=["urlsplit"]).urlsplit,
        "ToolLabel": ToolLabel,
        # imported collaborators (seamed per case; placeholders for constant exec)
        "safe_json_loads": lambda *a, **k: None,
        "t": lambda *a, **k: "",
        "redact_sensitive_text": lambda *a, **k: "",
        "sanitize_command_for_display": lambda *a, **k: "",
        "shape_failure_error": lambda *a, **k: None,
        "file_mutation_result_landed": lambda *a, **k: False,
        "has_failure_marker": lambda *a, **k: False,
        "is_guardrail_refusal": lambda *a, **k: False,
        "logger": _SilentLogger(),
    }
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
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ("_CuteRow", "KawaiiSpinner"):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    consts()  # re-seed lambda-valued constants (_CUTE_LINES / _PREVIEW_BUILDERS)
    missing = [t for t in PY.values() if t not in ns and t not in METHODS]
    for m in METHODS:
        if not hasattr(ns.get("KawaiiSpinner", None), m):
            missing.append("KawaiiSpinner." + m)
    for c in REQUIRED_CONSTS:
        if c not in ns:
            missing.append(c)
    if not hasattr(ns.get("KawaiiSpinner", None), "THINKING_VERBS"):
        missing.append("KawaiiSpinner.THINKING_VERBS")
    if "_CuteRow" not in ns:
        missing.append("_CuteRow")
    if missing:
        print(f"FATAL oracle_display_flow: missing defs/consts: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) if nm in NS}
REAL_CUTE_LINES = NS["_CUTE_LINES"]
REAL_CUTE_KEYS = list(REAL_CUTE_LINES.keys())
REAL_KAWAII = NS["KawaiiSpinner"]
CURRENT = {"ctx": None}


# ── per-case script backend (fail-closed presence) ─────────────────────────
EXC_CLASSES = {
    "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "FileNotFoundError": FileNotFoundError,
    "UnicodeDecodeError": UnicodeDecodeError, "IsADirectoryError": IsADirectoryError,
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


# ── result materialization (script dict -> live object double) ─────────────
def mat_skin(r):
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def mat_labels(r):
    if isinstance(r, list):
        return [ToolLabel(**d) if isinstance(d, dict) else d for d in r]
    return r


def mat_cute(r):
    if isinstance(r, dict) and r.get("__cute_row__"):
        return NS["_CuteRow"](r.get("text", ""), r.get("emoji", ""), r.get("verb", ""), r.get("detail", ""))
    return r


def mat_detect(r):
    if isinstance(r, list) and len(r) == 2:
        return (r[0], r[1])
    return r


def mat_mfo(r):
    if isinstance(r, dict) and "op" in r:
        return (r.get("op"), r.get("more", ""))
    return r


def label_dict(l):
    return {"name": l.name, "emoji": l.emoji, "text": l.text, "preview": l.preview}


def cute_dict(r):
    return {"__cute_row__": True, "text": str(r), "emoji": r.emoji, "verb": r.verb, "detail": r.detail}


def install_seams(ctx, skip):
    CURRENT["ctx"] = ctx
    ns = NS
    for nm, obj in REAL.items():
        ns[nm] = obj
    ns["_CUTE_LINES"] = REAL_CUTE_LINES
    ns["KawaiiSpinner"] = REAL_KAWAII
    ns["logger"] = types.SimpleNamespace(
        debug=lambda fmt, *a, **k: ctx.call("logger.debug", {
            "fmt": fmt,
            "args": [str(x) if isinstance(x, BaseException) else x for x in a],
            **k}),
        info=_SilentLogger().info, warning=_SilentLogger().warning, error=_SilentLogger().error,
    )

    def raw(name, proj):
        def f(*a, **k):
            return ctx.call(name, proj(*a, **k))
        return f

    seams = {
        "t": lambda key, **kw: ctx.call("t", {"key": key, **kw}),
        "safe_json_loads": raw("safe_json_loads", lambda s: {"s": s}),
        "_get_skin": lambda: mat_skin(ctx.call("_get_skin", {})),
        "labels_for_call": lambda tool_name, args: mat_labels(ctx.call("labels_for_call", {"tool_name": tool_name, "args": args})),
        "tool_labels_for_call": lambda tool_name, args: mat_labels(ctx.call("tool_labels_for_call", {"tool_name": tool_name, "args": args})),
        "bridge_tool_labels": lambda tool_name, args: mat_labels(ctx.call("bridge_tool_labels", {"tool_name": tool_name, "args": args})),
        "get_tool_emoji": raw("get_tool_emoji", lambda tool_name, default="\u26a1": {"tool_name": tool_name, "default": default}),
        "build_tool_preview": raw("build_tool_preview", lambda tool_name, args, max_len=None: {"tool_name": tool_name, "args": args, "max_len": max_len}),
        "_cute_trunc": raw("_cute_trunc", lambda s: {"s": s}),
        "_oneline": raw("_oneline", lambda text: {"text": text}),
        "_domain": raw("_domain", lambda url: {"url": url}),
        "_display_url": raw("_display_url", lambda value: {"value": value}),
        "_tail_trunc": raw("_tail_trunc", lambda text, limit: {"text": text, "limit": limit}),
        "_delegate_action_preview": raw("_delegate_action_preview", lambda args: {"args": args}),
        "_delegate_task_goals": lambda tasks, *, per_goal_len: ctx.call("_delegate_task_goals", {"tasks": tasks, "per_goal_len": per_goal_len}),
        "_detect_tool_failure": lambda tool_name, result: mat_detect(ctx.call("_detect_tool_failure", {"tool_name": tool_name, "result": result})),
        "_trim_error": raw("_trim_error", lambda msg: {"msg": msg}),
        "_memory_first_op": lambda args: mat_mfo(ctx.call("_memory_first_op", {"args": args})),
        "_browser_exec_step_label": raw("_browser_exec_step_label", lambda args, max_chars=80: {"args": args, "max_chars": max_chars}),
        "_entry_failure_suffix": raw("_entry_failure_suffix", lambda entry: {"entry": entry}),
        "_bridge_row_suffixes": raw("_bridge_row_suffixes", lambda rows, result, call_suffix: {"rows": rows, "result": result, "call_suffix": call_suffix}),
        "_cute_fallback_row": lambda tool_name, args: mat_cute(ctx.call("_cute_fallback_row", {"tool_name": tool_name, "args": args})),
        "_cute_row": lambda emoji, verb, detail="": mat_cute(ctx.call("_cute_row", {"emoji": emoji, "verb": verb, "detail": detail})),
        "redact_tool_args_for_display": raw("redact_tool_args_for_display", lambda tool_name, args: {"tool_name": tool_name, "args": args}),
    }
    for nm, fn in seams.items():
        if nm == skip:
            continue
        ns[nm] = fn
    # tool_labels_for_call does `from tools.tool_labels import labels_for_call`
    # INSIDE its body — that resolves sys.modules, not NS. Stub the module so the
    # import binds the per-case recorder (fail-closed when unscripted).
    tools = sys.modules.get("tools")
    if not isinstance(tools, types.ModuleType):
        tools = types.ModuleType("tools")
        tools.__path__ = []
        sys.modules["tools"] = tools
    tl = types.ModuleType("tools.tool_labels")
    tl.labels_for_call = ns["labels_for_call"]
    tools.tool_labels = tl
    sys.modules["tools.tool_labels"] = tl
    # rebuild the renderer dispatch table for get_tool_row_parts
    if skip == "get_tool_row_parts":
        def mk(tool):
            def render(a, r):
                return mat_cute(ctx.call("cute_render", {"tool_name": tool, "args": a, "result": r}))
            return render
        ns["_CUTE_LINES"] = {k: mk(k) for k in REAL_CUTE_KEYS}


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
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx, PY[func])
    ns = NS
    ns["_friendly_tool_labels"] = (inp.get("state") or {}).get("_friendly_tool_labels", True)
    ns["_tool_preview_max_len"] = (inp.get("state") or {}).get("_tool_preview_max_len", 0)
    out = {}
    ret = None
    args = inp.get("args")
    result = inp.get("result")
    try:
        if func == "_memory_first_op":
            op, more = ns["_memory_first_op"](args)
            ret = {"op": op, "more": more}
        elif func == "tool_labels_for_call":
            ret = [label_dict(l) for l in ns["tool_labels_for_call"](inp.get("tool_name", ""), args)]
        elif func == "bridge_tool_labels":
            ret = [label_dict(l) for l in ns["bridge_tool_labels"](inp.get("tool_name", ""), args)]
        elif func == "tool_row_emoji":
            ret = ns["tool_row_emoji"](inp.get("tool_name", ""), args, inp.get("default", "\u26a1"))
        elif func == "_cute_row":
            ret = cute_dict(ns["_cute_row"](inp.get("emoji", ""), inp.get("verb", ""), inp.get("detail", "")))
        elif func == "_cute_fallback_row":
            ret = cute_dict(ns["_cute_fallback_row"](inp.get("tool_name", ""), args))
        elif func in ("_cute_web_extract", "_cute_todo_list", "_cute_memory", "_cute_skill_view",
                      "_cute_cronjob", "_cute_execute_code", "_cute_browser_exec", "_cute_delegate",
                      "_cute_process_manage", "_cute_scroll"):
            ret = cute_dict(ns[func](args, result))
        elif func == "_cute_bridge_rows":
            ret = ns["_cute_bridge_rows"](inp.get("tool_name", ""), args)
        elif func == "_entry_failure_suffix":
            ret = ns["_entry_failure_suffix"](inp.get("entry"))
        elif func == "_bridge_row_suffixes":
            ret = ns["_bridge_row_suffixes"](inp.get("rows", 0), result, inp.get("call_suffix", ""))
        elif func == "get_tool_row_parts":
            rows = ns["get_tool_row_parts"](inp.get("tool_name", ""), args, result, is_error=inp.get("is_error", False))
            ret = [list(r) for r in rows]
        elif func == "_skin_spinner_list":
            ret = ns["KawaiiSpinner"]._skin_spinner_list(inp.get("key", ""), inp.get("fallback"))
        elif func == "_catalog_thinking_verbs":
            ret = ns["KawaiiSpinner"]._catalog_thinking_verbs()
        elif func == "_clear_line_blanks":
            slf = types.SimpleNamespace(last_line_len=(inp.get("state") or {}).get("last_line_len", 0))
            ret = ns["KawaiiSpinner"]._clear_line_blanks(slf)
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
        print("FATAL oracle_display_flow: 0 cases")
        print("ORACLE display_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE display_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


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
    print(f"SELF-TEST display_flow: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


def seam_names():
    return ["t", "safe_json_loads", "_get_skin", "labels_for_call", "tool_labels_for_call",
            "bridge_tool_labels", "get_tool_emoji", "build_tool_preview", "_cute_trunc",
            "_oneline", "_domain", "_display_url", "_tail_trunc", "_delegate_action_preview",
            "_delegate_task_goals", "_detect_tool_failure", "_trim_error", "_memory_first_op",
            "_browser_exec_step_label", "_entry_failure_suffix", "_bridge_row_suffixes",
            "_cute_fallback_row", "_cute_row", "redact_tool_args_for_display", "cute_render",
            "logger.debug"]


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "display_flow.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())