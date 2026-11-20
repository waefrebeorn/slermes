#!/usr/bin/env python3
"""Oracle for src/acp_adapter/acp_tools_flow2.c.

Reads harness JSONL on stdin ({"func","in","ret","trace","st"}), AST-extracts
the LIVE defs of acp_adapter/tools.py into a namespace with stubbed external
collaborators, drives each case under the SAME universal seams the C harness
uses, and diffs the ordered trace, the return (or raise spec) and the state
window. Prints 'ORACLE acp_tools_flow2: N cases, M mismatches'.

The 6 flow1-claimed defs (get_tool_kind, make_tool_call_id, build_tool_title,
_truncate_text, _fenced_text, _tool_result_failed) are re-derived LIVE here for
the standalone differential check; their older claims stay in force.

Seams: uuid.uuid4, build_tool_preview, json.loads, json.dumps,
JSONDecoder.raw_decode, acp.text_block, acp.tool_content,
acp.tool_diff_content, extract_edit_diff, acp.start_tool_call,
acp.update_tool_call, ToolCallLocation, logger.debug.
Scripts are constants or per-call queues (last repeats); an absent/exhausted
script yields a RuntimeError raise spec recorded before propagation. Full
builtin exception hierarchy is used for `except` matching (runtime builtins).
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
import __future__
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
_QUARRY = _REPO.parent / "hermes-agent"
if not (_QUARRY / "acp_adapter" / "tools.py").is_file():
    _QUARRY = Path.home() / "hermes-agent"
SRC = _QUARRY / "acp_adapter" / "tools.py"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
ANN = __future__.annotations.compiler_flag

# every top-level def in the module (the port covers all 47)
FUNCS = [
    "get_tool_kind", "make_tool_call_id", "_text", "_arg", "_first", "_fmt",
    "_nonempty", "_plural", "_failure", "coerce_tool_args", "_args_json",
    "_json_loads_maybe", "_truncate_text", "_fenced_text", "_tool_result_failed",
    "build_tool_title", "_format_todo_result", "_format_read_file_result",
    "_format_search_files_result", "_format_execute_code_result",
    "_format_skill_view_result", "_format_skill_manage_result",
    "_format_web_search_result", "_format_web_extract_result",
    "_format_process_result", "_format_delegate_result",
    "_format_session_search_result", "_format_memory_result", "_format_edit_result",
    "_format_browser_result", "_format_media_or_cron_result",
    "_format_structured_value", "_format_generic_structured_result",
    "_parse_unified_diff_content", "_build_tool_complete_content", "_more",
    "_start_todo", "_start_skill_manage", "_start_execute_code", "_start_delegate",
    "_preview", "build_tool_start", "_build_tool_start", "build_tool_complete",
    "build_tool_abandoned", "extract_locations",
]

# ---------------- exception table (full builtin hierarchy) ----------------
EXC = {}
for _n in dir(builtins):
    _o = getattr(builtins, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        EXC[_n] = _o
try:
    import asyncio
    EXC["CancelledError"] = asyncio.CancelledError
except Exception:
    pass


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    try:
        return cls(msg)
    except Exception:
        e = cls.__new__(cls)
        try:
            Exception.__init__(e, msg)
        except Exception:
            pass
        return e


def is_raise(x):
    return isinstance(x, dict) and "__raise__" in x


def norm(x, _d=0):
    if _d > 24:
        return str(x)
    if x is None:
        return None
    if isinstance(x, SimpleNamespace):
        return {k: norm(v, _d + 1) for k, v in vars(x).items()}
    if isinstance(x, BaseException):
        return {"__raise__": type(x).__name__, "message": str(x)}
    if isinstance(x, dict):
        return {k: norm(v, _d + 1) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v, _d + 1) for v in x]
    if isinstance(x, bool) or isinstance(x, int) or isinstance(x, str):
        return x
    if isinstance(x, float):
        return x
    return str(x)


# ---------------- universal seam recorder ----------------
CURRENT = {"ctx": None}


class Ctx:
    def __init__(self, seams):
        self.seams = seams or {}
        self.cursors = {}
        self.trace = []

    def seam(self, name, args):
        args = norm(args)
        sc = self.seams.get(name)
        ret = None
        have = False
        if sc is not None:
            if isinstance(sc, list):
                i = self.cursors.get(name, 0)
                if i < len(sc):
                    ret, have = sc[i], True
                    self.cursors[name] = i + 1
            else:
                ret, have = sc, True
        if not have:
            ret = {"__raise__": "RuntimeError", "message": "no script for " + name}
        self.trace.append({"ev": name, "args": args, "ret": norm(ret)})
        if is_raise(ret):
            raise build_exc(ret)
        return ret


def _ctx():
    return CURRENT["ctx"]


NS = {}


def _install_stubs():
    """Stub external collaborators. They consult CURRENT['ctx'] per call."""
    jm = types.ModuleType("json")

    def j_loads(s, **kw):
        return _ctx().seam("json.loads", {"s": s})

    def j_dumps(obj, **kw):
        d = kw.get("default")
        dv = getattr(d, "__name__", str(d)) if d is not None else None
        return _ctx().seam("json.dumps", {
            "obj": obj, "indent": kw.get("indent"),
            "ensure_ascii": kw.get("ensure_ascii", True), "default": dv,
        })

    class JSONDecoder:
        def raw_decode(self, s, **kw):
            v = _ctx().seam("JSONDecoder.raw_decode", {"s": s})
            return (v, len(s))

    jm.loads, jm.dumps, jm.JSONDecoder = j_loads, j_dumps, JSONDecoder
    NS["json"] = jm

    um = types.ModuleType("uuid")

    class U:
        @property
        def hex(self):
            return getattr(self, "_hex", "")

    def uuid4():
        v = _ctx().seam("uuid.uuid4", {})
        o = U()
        o._hex = v
        return o
    um.uuid4 = uuid4
    NS["uuid"] = um

    acp = types.ModuleType("acp")

    def text_block(content):
        return _ctx().seam("acp.text_block", {"content": content})

    def tool_content(block):
        return _ctx().seam("acp.tool_content", {"block": block})

    def tool_diff_content(*, path, old_text=None, new_text=None):
        return _ctx().seam("acp.tool_diff_content", {
            "path": path, "old_text": old_text, "new_text": new_text})

    def start_tc(id, title=None, *, kind=None, content=None, locations=None,
                 raw_input=None, raw_output=None):
        return _ctx().seam("acp.start_tool_call", {
            "tool_call_id": id, "title": title, "kind": kind, "content": content,
            "locations": locations, "raw_input": raw_input})

    def update_tc(id, *, kind=None, status=None, content=None, raw_output=None):
        return _ctx().seam("acp.update_tool_call", {
            "tool_call_id": id, "kind": kind, "status": status, "content": content,
            "raw_output": raw_output})

    acp.tool_content = tool_content
    acp.text_block = text_block
    acp.tool_diff_content = tool_diff_content
    acp.start_tool_call = start_tc
    acp.update_tool_call = update_tc
    NS["acp"] = acp

    def ToolCallLocation(*, path=None, line=None):
        return _ctx().seam("ToolCallLocation", {"path": path, "line": line})
    NS["ToolCallLocation"] = ToolCallLocation

    def build_tool_preview(tool_name, args, max_len=80):
        return _ctx().seam("build_tool_preview", {
            "tool_name": tool_name, "args": args, "max_len": max_len})
    NS["build_tool_preview"] = build_tool_preview

    def extract_edit_diff(tool_name, result, *, function_args=None, snapshot=None):
        return _ctx().seam("extract_edit_diff", {
            "tool_name": tool_name, "result": result,
            "function_args": function_args, "snapshot": snapshot})
    ag = types.ModuleType("agent")
    ad = types.ModuleType("agent.display")
    ad.extract_edit_diff = extract_edit_diff
    ad.build_tool_preview = build_tool_preview
    ag.display = ad
    sys.modules.setdefault("agent", ag)
    sys.modules["agent.display"] = ad

    class RecLogger:
        def debug(self, msg, *args):
            return _ctx().seam("logger.debug", {
                "msg": msg, "tool_name": args[0] if len(args) > 0 else None,
                "exc": args[1] if len(args) > 1 else None})

    lm = types.ModuleType("logging")
    lm.getLogger = lambda *a, **k: RecLogger()
    NS["logging"] = lm
    sys.modules["logging"] = lm

    # typing names used in module-level annotations
    NS.update({"Any": Any, "Callable": Callable, "Dict": Dict, "List": List,
               "Optional": Optional})


def load_ns():
    """AST-extract all top-level defs + constants from the live module."""
    NS.clear()
    _install_stubs()
    text = SRC.read_text(encoding="utf-8")
    tree = ast.parse(text)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            nd = copy.deepcopy(node)
            nd.decorator_list = []
            exec(compile(ast.Module([nd], []), str(SRC), "exec", flags=ANN), NS)
            if node.decorator_list:
                fn = NS[node.name]
                for dec in reversed(node.decorator_list):
                    fac = eval(compile(ast.Expression(dec), str(SRC), "eval", flags=ANN), NS)
                    fn = fac(fn)
                NS[node.name] = fn
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module([node], []), str(SRC), "exec", flags=ANN), NS)
            except Exception:
                pass
    NS["logger"] = RecLoggerStub()
    return NS


class RecLoggerStub:
    def debug(self, msg, *args):
        return _ctx().seam("logger.debug", {
            "msg": msg, "tool_name": args[0] if len(args) > 0 else None,
            "exc": args[1] if len(args) > 1 else None})


def drive(func, argv, kw, seams):
    """Drive the LIVE function under the given seam scripts."""
    ctx = Ctx(seams)
    CURRENT["ctx"] = ctx
    try:
        fn = NS[func]
        r = fn(*argv, **kw)
        ret = norm(r)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    finally:
        CURRENT["ctx"] = None
    return {"ret": ret, "trace": ctx.trace, "st": {}}


def canon(x):
    if isinstance(x, bool):
        return x
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, list):
        return [canon(v) for v in x]
    return x


# ---------------- main: compare harness output to live ----------------
def main():
    raw = sys.stdin.read()
    cases = []
    for line in raw.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    load_ns()
    mismatches = 0
    shown = 0
    for rec in cases:
        func = rec.get("func", "")
        ini = rec.get("in", {}) or {}
        argv = ini.get("argv", []) or []
        kw = ini.get("kw", {}) or {}
        seams = ini.get("seams", {}) or {}
        try:
            exp = drive(func, argv, kw, seams)
        except BaseException as e:
            exp = {"ret": {"__raise__": type(e).__name__, "message": str(e)},
                   "trace": [], "st": {}}
        got = {"ret": rec.get("ret"), "trace": rec.get("trace", []),
               "st": rec.get("st", {})}
        ok = True
        for k in ("ret", "trace", "st"):
            if canon(got.get(k)) != canon(exp.get(k)):
                ok = False
                break
        if not ok:
            mismatches += 1
            if shown < 100:
                shown += 1
                print("MISMATCH func=%s argv=%r" % (func, argv), file=sys.stderr)
                for k in ("ret", "trace", "st"):
                    if canon(got.get(k)) != canon(exp.get(k)):
                        print("  %s\n    got=%r\n    exp=%r" % (k, got.get(k), exp.get(k)), file=sys.stderr)
    print("ORACLE acp_tools_flow2: %d cases, %d mismatches" % (len(cases), mismatches))
    return 1 if mismatches else 0


def selftest():
    """Python-only self-test over fixture cases: flag drive failures and
    AttributeError/NameError/TypeError families naming ported methods/seams."""
    fx = _HERE.parent / "fixtures" / "acp_tools_flow2.jsonl"
    if not fx.is_file():
        print("SELFTEST: no fixture file")
        return 2
    load_ns()
    bad = []
    names = set(FUNCS)
    seam_names = {"uuid.uuid4", "build_tool_preview", "json.loads", "json.dumps",
                  "JSONDecoder.raw_decode", "acp.text_block", "acp.tool_content",
                  "acp.tool_diff_content", "extract_edit_diff", "acp.start_tool_call",
                  "acp.update_tool_call", "ToolCallLocation", "logger.debug"}
    total = 0
    for line in fx.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        case = json.loads(line)
        total += 1
        func = case.get("func")
        if func not in names:
            bad.append("%s: func not in extraction map" % func)
            continue
        ini = case.get("in", {})
        r = drive(func, ini.get("argv", []), ini.get("kw", {}), ini.get("seams", {}))
        ret = r["ret"]
        if is_raise(ret) and ret.get("__raise__") in ("AttributeError", "NameError", "TypeError"):
            msg = ret.get("message", "")
            for n in list(names) + list(seam_names):
                if "'%s'" % n in msg or n in msg:
                    bad.append("%s: %s (%s)" % (func, ret["__raise__"], msg[:100]))
                    break
    print("SELFTEST: %d cases, %d suspicious" % (total, len(bad)))
    for b in bad[:20]:
        print("  " + b)
    return 1 if bad else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    sys.exit(main())
