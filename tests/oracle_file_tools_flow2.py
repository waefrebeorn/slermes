#!/usr/bin/env python3
"""Oracle for file_tools_flow2.c — tools/file_tools.py UNCLAIMED remainder.

LIVE authority: C:/Users/eman5/hermes-agent/tools/file_tools.py (READ-ONLY quarry).
A CRLF-aware PoP grep of src/tools/file_tools*.c first: every def already claimed
by a PoP annotation (read_file_tool, write_file_tool, patch_tool, search_tool,
_truncate_to_char_budget, _get_file_ops, the _handle_* wrappers, ...) is SEAMED
here, never re-ported. The 11 defs below are UNCLAIMED and AST-extracted live.

Each case's "seams" object scripts the universal seam. The harness records one
{ev,args,ret} event per call in order; recording stubs drive the LIVE Python with
the same scripts and emit the identical shape. Compares ordered trace + ret with
insertion-order-sensitive canon().

Input (stdin JSONL from t_port_file_tools_flow2):
    {"func":..., "in":{..., "seams":{...}}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE file_tools_flow2: N cases, M mismatches.

Script semantics (identical on both sides, FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; an array is a per-call queue (last repeats, empty ->
None); a single value is a constant; a scripted null is None; {"__raise__":cls,
"msg":...} raises a real exception of that builtin class.

Modes:
    (default)      compare harness envelopes on stdin with the LIVE drive
    --selftest     shadow-mirror vs LIVE over every fixture (expect 0 diffs)
    --emit-mirror  emit shadow-mirror envelopes to stdout (proxy gate input)
"""
import ast
import builtins
import contextlib
import copy
import json
import os
import re
import sys
import types
from pathlib import Path as FsPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = FsPath(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "ft2_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "tools", "file_tools.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __import__("__future__").annotations.compiler_flag
FIXTURE = FsPath(__file__).parent / "fixtures" / "file_tools_flow2.jsonl"

FLOW_FUNCS = {
    "apply_char_budget": "_apply_char_budget",
    "resolved_match_path": "_resolved_match_path",
    "resolve_or_none": "_resolve_or_none",
    "write_precheck_error": "_write_precheck_error",
    "edit_warnings": "_edit_warnings",
    "note_edited": "_note_edited",
    "whole_file_rewrite_hint": "_whole_file_rewrite_hint",
    "collect_v4a_header_paths": "_collect_v4a_header_paths",
    "owned": "_owned",
    "read_device_base": "_read_device_base",
    "dedup_stub_or_block": "_dedup_stub_or_block",
}
TOP_DEFS = list(FLOW_FUNCS.values())
ALL_FUNCS = set(FLOW_FUNCS)

_READ_DEDUP_STATUS_MESSAGE = (
    "File unchanged since last read. The content from "
    "the earlier read_file result in this conversation is "
    "still current \u2014 refer to that instead of re-reading.")
GUARDRAIL_REFUSAL_KEY = "guardrail_refusal"
_V4A_SINGLE = re.compile(r'^(\*\*\*\s*(Update|Add|Delete)\s+File:\s*)(.+)$', re.MULTILINE)
_V4A_MOVE = re.compile(r'^(\*\*\*\s*Move\s+File:\s*)(.+?)\s*->\s*(.+)$', re.MULTILINE)

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "raised": []}


# ── FULL builtin exception hierarchy (Python 3.14, generated from builtins) ──
def _builtin_exc_table():
    t = {}
    for n in dir(builtins):
        o = getattr(builtins, n)
        if isinstance(o, type) and issubclass(o, BaseException):
            t[n] = o
    # CPython aliases: these ARE OSError, even though __mro__[1] is Exception.
    for a in ("IOError", "EnvironmentError", "WindowsError"):
        t[a] = OSError
    return t


BUILTIN_EXC = _builtin_exc_table()
# child -> parent name table (mirrors FT2_EXC_TREE in the C port).
FT2_PARENT = {}
for _n, _c in BUILTIN_EXC.items():
    try:
        _mro = _c.__mro__[1]
        FT2_PARENT[_n] = _mro.__name__
    except Exception:
        pass
for _a in ("IOError", "EnvironmentError", "WindowsError"):
    FT2_PARENT[_a] = "OSError"


def cls_is(c, ancestor):
    """isinstance-style fold over the FULL builtin hierarchy (C ft2_cls_is)."""
    while c:
        if c == ancestor:
            return True
        c = FT2_PARENT.get(c)
    return False


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    C = BUILTIN_EXC.get(cls)
    if C is not None:
        return C(msg)
    return type(cls, (Exception,), {})(msg)


# ── live seam (raises real exceptions so live try/except runs) ──
def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:  # FAIL-CLOSED: presence test, not .get() is None
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = copy.deepcopy(r)
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, PathDouble):
        return o.s
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


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
    return ("str", o)


# ── doubles ──
class PathDouble:
    def __init__(self, p):
        self.s = str(p)

    def expanduser(self):
        r = seam("path_expanduser", {"path": self.s})
        return PathDouble(r if isinstance(r, str) else "")

    def is_absolute(self):
        return self.s.startswith("/")

    def __str__(self):
        return self.s


class _FileOpsDouble:
    def read_file_raw(self, resolved):
        r = seam("read_file_raw", {"task_id": CUR["in"].get("task_id", "default"),
                                   "resolved": str(resolved)})
        if not isinstance(r, dict):
            r = {}
        return types.SimpleNamespace(**r)


@contextlib.contextmanager
def _bind_owner(owner):
    seam("bind_owner_enter", {"owner": owner})
    try:
        yield
    except BaseException as e:
        seam("bind_owner_exit", {"owner": owner, "exc": type(e).__name__})
        raise
    else:
        seam("bind_owner_exit", {"owner": owner, "exc": None})


def _module(name, parent=None, **attrs):
    m = types.ModuleType(name)
    m.__dict__.update(attrs)
    sys.modules[name] = m
    if parent:
        parent_attr = name.rsplit(".", 1)[1]
        setattr(sys.modules[parent], parent_attr, m)
    return m


# ── extraction (no heavy quarry import) ──
def extract():
    ns = {"__name__": "file_tools_flow2_extracted", "__builtins__": builtins.__dict__}
    ns.update({
        "json": json, "os": os, "sys": sys, "re": re,
        "threading": types.SimpleNamespace(Lock=lambda: contextlib.nullcontext()),
        "Path": PathDouble, "PurePosixPath": PathDouble,
        "SimpleNamespace": types.SimpleNamespace,
        "_READ_DEDUP_STATUS_MESSAGE": _READ_DEDUP_STATUS_MESSAGE,
        "GUARDRAIL_REFUSAL_KEY": GUARDRAIL_REFUSAL_KEY,
        "_V4A_SINGLE_HEADER_RE": _V4A_SINGLE, "_V4A_MOVE_HEADER_RE": _V4A_MOVE,
    })
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    # Module constants (incl. _REWRITE_HINT_* and the V4A regexes).
    for node in tree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try:
                exec_node(node)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec_node(node)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_DEFS:
            exec_node(node)
    missing = [n for n in TOP_DEFS if n not in ns]
    if missing:
        print("ORACLE file_tools_flow2: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    install_stubs(ns)
    return ns


def install_stubs(ns):
    # CLAIMED siblings / collaborators -> recording seams.
    def _trunc(content, max_chars):
        r = seam("truncate_to_char_budget", {"content": content, "max_chars": max_chars})
        if isinstance(r, list):
            if len(r) < 3:
                r = r + [None] * (3 - len(r))
            return (r[0], r[1], r[2])  # live unpacks (trimmed, lines_kept, _)
        return r

    ns["_truncate_to_char_budget"] = _trunc
    ns["_resolve_base_dir"] = lambda tid: seam("resolve_base_dir", {"task_id": tid})
    ns["_resolve_path_for_task"] = lambda p, tid: seam(
        "resolve_path_for_task", {"path": p, "task_id": tid})
    ns["_resolve_entry_for_task"] = lambda p, tid: seam(
        "resolve_entry_for_task", {"path": p, "task_id": tid})
    for n in ("_check_sensitive_path", "_check_cross_profile_path",
              "_check_binary_document_write", "_check_protected_instruction_write",
              "_check_approval_required_write", "_check_file_staleness"):
        ns[n] = _guard(n)
    ns["_path_resolution_warning"] = lambda p, r, tid: seam(
        "path_resolution_warning", {"path": p, "resolved": str(r), "task_id": tid})
    ns["_mark_verification_stale"] = lambda tid, paths, session_id=None: seam(
        "mark_verification_stale", {"task_id": tid, "paths": paths, "session_id": session_id})
    ns["_update_read_timestamp"] = lambda p, tid: seam(
        "update_read_timestamp", {"path": p, "task_id": tid})
    ns["_cap_read_tracker_data"] = lambda td: seam("cap_read_tracker_data", {"task_data": td})
    ns["_read_tracker_lock"] = contextlib.nullcontext()
    ns["_get_file_ops"] = lambda tid="default": _FileOpsDouble()

    def tool_error(message, **extra):
        return json.dumps({"error": str(message), **extra}, ensure_ascii=False)

    ns["tool_error"] = tool_error
    fs = types.SimpleNamespace(
        check_stale=lambda tid, r: seam("file_state.check_stale", {"task_id": tid, "path": str(r)}),
        note_write=lambda tid, p: seam("file_state.note_write", {"task_id": tid, "path": str(p)}),
    )
    ns["file_state"] = fs

    # In-function imports -> sys.modules stubs (every intermediate package).
    tools = _module("tools")
    tools.__path__ = []
    envs = _module("tools.environments", "tools")
    envs.__path__ = []
    _module("tools.environments.sandbox_scope", "tools.environments",
            hosted_scope=lambda: seam("hosted_scope", {}))
    _module("tools.path_security", "tools",
            has_traversal_component=lambda p: seam("has_traversal_component", {"path": p}))
    agent = sys.modules.get("agent") or _module("agent")
    agent.__path__ = []
    lsp = _module("agent.lsp", "agent")
    lsp.__path__ = []
    _module("agent.lsp.deferred", "agent.lsp", bind_owner=_bind_owner)


def _guard(name):
    def f(*a, **k):
        args = {}
        if name in ("_check_protected_instruction_write", "_check_approval_required_write"):
            args["paths"] = a[0] if a else k.get("paths")
            args["task_id"] = a[1] if len(a) > 1 else k.get("task_id")
        else:
            args["path"] = a[0] if a else k.get("path")
            args["task_id"] = a[1] if len(a) > 1 else k.get("task_id")
        return seam(name.lstrip("_"), args)
    return f


# ── LIVE drive ──
def drive(ns, func, inp):
    try:
        ret = _drive_inner(ns, func, inp)
    except BaseException as e:
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def _drive_inner(ns, func, inp):
    if func == "apply_char_budget":
        rd = copy.deepcopy(inp.get("result") or {})
        trimmed = ns["_apply_char_budget"](
            rd, inp.get("content", ""), inp.get("offset", 1),
            inp.get("total_lines", "unknown"), inp.get("max_chars", 100000))
        return {"trimmed": trimmed, "result": rd}
    if func == "resolved_match_path":
        return ns["_resolved_match_path"](inp.get("path", ""), inp.get("task_id", "default"))
    if func == "resolve_or_none":
        return ns["_resolve_or_none"](inp.get("filepath", ""), inp.get("task_id", "default"),
                                      entry=bool(inp.get("entry", False)))
    if func == "write_precheck_error":
        return ns["_write_precheck_error"](inp.get("paths") or [], inp.get("content_paths") or [],
                                           inp.get("task_id", "default"),
                                           bool(inp.get("cross_profile", False)))
    if func == "edit_warnings":
        return ns["_edit_warnings"](inp.get("paths") or [], inp.get("path_to_resolved") or {},
                                    inp.get("task_id", "default"))
    if func == "note_edited":
        return ns["_note_edited"](inp.get("task_id", "default"), inp.get("paths") or [],
                                  inp.get("path_to_resolved") or {}, inp.get("session_id"))
    if func == "whole_file_rewrite_hint":
        return ns["_whole_file_rewrite_hint"](inp.get("task_id", "default"),
                                              inp.get("resolved"), inp.get("new_content", ""))
    if func == "collect_v4a_header_paths":
        r = ns["_collect_v4a_header_paths"](inp.get("patch", ""))
        if isinstance(r, str):
            return {"__te__": json.loads(r)}
        return {"paths": r[0], "content_paths": r[1], "entry_paths": r[2]}
    if func == "owned":
        owner = inp.get("owner", "default")
        kw = copy.deepcopy(inp.get("call_kwargs") or {})

        def fn(**k):
            return seam("owned_call", {"owner": owner, **k})

        return ns["_owned"](owner, fn, **kw)
    if func == "read_device_base":
        base, unanchored = ns["_read_device_base"](inp.get("path", ""), inp.get("task_id", "default"))
        return {"base": norm(base), "unanchored": unanchored}
    if func == "dedup_stub_or_block":
        td = copy.deepcopy(inp.get("task_data") or {})
        k = inp.get("dedup_key") or []
        key = "%s|%s|%s" % (k[0] if len(k) > 0 else "", k[1] if len(k) > 1 else 0,
                            k[2] if len(k) > 2 else 0)
        r = ns["_dedup_stub_or_block"](td, key, inp.get("path", ""))
        return {"__te__": json.loads(r)}
    raise ValueError("unknown func: %s" % func)


# ═══════════════════ shadow mirror (literal transcription of the C) ═══════════════════
def _mc(M, name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = M["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = M["idx"].get(name, 0)
            M["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = copy.deepcopy(r)
    M["trace"].append({"ev": name, "args": args, "ret": r})
    return r


def _isr(v):
    return isinstance(v, dict) and "__raise__" in v


def _pystr(v):
    if v is None:
        return "None"
    if v is True:
        return "True"
    if v is False:
        return "False"
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return str(int(v)) if float(v).is_integer() else str(v)
    return str(v)


def _m_dispatch(M, func, inp):
    scripts = inp.get("seams") or {}
    M["scripts"] = scripts
    if func == "apply_char_budget":
        r = _mc(M, "truncate_to_char_budget",
                {"content": inp.get("content", ""), "max_chars": inp.get("max_chars", 100000)})
        if _isr(r):
            return r
        trimmed = r[0] if isinstance(r, list) and len(r) >= 1 else ""
        lines_kept = int(r[1]) if isinstance(r, list) and len(r) >= 2 else 0
        offset = int(inp.get("offset", 1))
        max_chars = int(inp.get("max_chars", 100000))
        next_offset = offset + lines_kept
        rd = copy.deepcopy(inp.get("result") or {})
        rd["content"] = trimmed
        rd["truncated"] = True
        rd["truncated_by"] = "bytes"
        rd["next_offset"] = next_offset
        hint = ("Output truncated at the %s-char read budget after %d line(s) "
                "(showing lines %d-%d of %s). Use offset=%d to continue." % (
                    format(max_chars, ","), lines_kept, offset, next_offset - 1,
                    _pystr(inp.get("total_lines", "unknown")), next_offset))
        rd["hint"] = hint
        if len(trimmed.split("\n", 1)[0]) >= max_chars:
            rd["truncated_lines"] = True
            rd["hint"] += (" Note: the first line alone exceeded the budget and was "
                           "clamped mid-line; its remainder is not retrievable via offset.")
        return {"trimmed": trimmed, "result": rd}

    if func == "resolved_match_path":
        r = _mc(M, "resolve_path_for_task",
                {"path": inp.get("path", ""), "task_id": inp.get("task_id", "default")})
        if _isr(r):
            c = r["__raise__"]
            if cls_is(c, "OSError") or cls_is(c, "ValueError") or cls_is(c, "RuntimeError"):
                return inp.get("path", "")
            return r
        return r if isinstance(r, str) else _pystr(r)

    if func == "resolve_or_none":
        name = "resolve_entry_for_task" if inp.get("entry") else "resolve_path_for_task"
        r = _mc(M, name, {"path": inp.get("filepath", ""), "task_id": inp.get("task_id", "default")})
        if _isr(r):
            return None if cls_is(r["__raise__"], "Exception") else r
        return r if isinstance(r, str) else _pystr(r)

    if func == "write_precheck_error":
        for p in inp.get("paths") or []:
            err = _mc(M, "check_sensitive_path", {"path": p, "task_id": inp.get("task_id", "default")})
            if _isr(err) or bool(err):
                return err
            if not inp.get("cross_profile", False):
                err = _mc(M, "check_cross_profile_path", {"path": p, "task_id": inp.get("task_id", "default")})
                if _isr(err) or bool(err):
                    return err
        for p in inp.get("content_paths") or []:
            err = _mc(M, "check_binary_document_write", {"path": p, "task_id": inp.get("task_id", "default")})
            if _isr(err) or bool(err):
                return err
        err = _mc(M, "check_protected_instruction_write",
                  {"paths": inp.get("paths") or [], "task_id": inp.get("task_id", "default")})
        if _isr(err) or bool(err):
            return err
        return _mc(M, "check_approval_required_write",
                   {"paths": inp.get("paths") or [], "task_id": inp.get("task_id", "default")})

    if func == "edit_warnings":
        out = []
        for p in inp.get("paths") or []:
            rv = (inp.get("path_to_resolved") or {}).get(p)
            w = None
            if rv:
                w = _mc(M, "file_state.check_stale", {"task_id": inp.get("task_id", "default"), "path": rv})
                if _isr(w):
                    return w
            if not w:
                w = _mc(M, "check_file_staleness", {"path": p, "task_id": inp.get("task_id", "default")})
                if _isr(w):
                    return w
            if not w and rv:
                w = _mc(M, "path_resolution_warning",
                        {"path": p, "resolved": rv, "task_id": inp.get("task_id", "default")})
                if _isr(w):
                    return w
            if w:
                out.append(w)
        return out

    if func == "note_edited":
        rp = [(inp.get("path_to_resolved") or {}).get(p) or p for p in inp.get("paths") or []]
        r = _mc(M, "mark_verification_stale",
                {"task_id": inp.get("task_id", "default"), "paths": rp, "session_id": inp.get("session_id")})
        if _isr(r):
            return r
        for p in inp.get("paths") or []:
            r = _mc(M, "update_read_timestamp", {"path": p, "task_id": inp.get("task_id", "default")})
            if _isr(r):
                return r
            rv = (inp.get("path_to_resolved") or {}).get(p)
            if rv:
                r = _mc(M, "file_state.note_write", {"task_id": inp.get("task_id", "default"), "path": rv})
                if _isr(r):
                    return r
        return None

    if func == "whole_file_rewrite_hint":
        resolved, nc_s = inp.get("resolved"), inp.get("new_content", "")
        if not resolved or not (20000 <= len(nc_s) <= 400000):
            return None
        r = _mc(M, "read_file_raw", {"task_id": inp.get("task_id", "default"), "resolved": resolved})
        if _isr(r):
            # live wraps the read in `try/except Exception: return None`.
            return None if cls_is(r["__raise__"], "Exception") else r
        old = r.get("content") if isinstance(r, dict) else None
        if (isinstance(r, dict) and r.get("error")) or not isinstance(old, str):
            return None
        if not (20000 <= len(old) <= 400000):
            return None
        old_lines, new_lines = old.splitlines(), nc_s.splitlines()
        if not old_lines:
            return None
        from collections import Counter
        unchanged = sum((Counter(old_lines) & Counter(new_lines)).values())
        ratio = unchanged / max(len(old_lines), len(new_lines))
        if ratio < 0.80:
            return None
        changed = max(len(old_lines), len(new_lines)) - unchanged
        return ("%s of %s lines were already on disk (%s unchanged); ~%s "
                "line(s) actually changed. Re-sending a %s-char file costs output "
                "tokens for every unchanged line; for edits like this use patch "
                "(old_string/new_string), which sends only the changed region." % (
                    format(unchanged, ","), format(len(new_lines), ","),
                    format(ratio, ".0%"), format(changed, ","), format(len(nc_s), ",")))

    if func == "collect_v4a_header_paths":
        patch = inp.get("patch", "")
        single = [(m.group(3), m.group(2) in ("Update", "Add"))
                  for m in _V4A_SINGLE.finditer(patch)]
        single += [(g, False) for m in _V4A_MOVE.finditer(patch)
                   for g in (m.group(2), m.group(3))]
        paths, content_paths, entry_paths = [], [], []
        for raw, writes_text in single:
            v4a = raw.strip()
            trav = _mc(M, "has_traversal_component", {"path": v4a})
            if _isr(trav):
                return trav
            if trav:
                return {"__te__": {"error": (
                    "V4A patch header contains '..' traversal: %r. "
                    "Use the agent's cwd-relative path (no '..') or an absolute "
                    "path in '*** Update File:' / '*** Add File:' / "
                    "'*** Delete File:' / '*** Move File:' headers." % v4a)}}
            paths.append(v4a)
            (content_paths if writes_text else entry_paths).append(v4a)
        return {"paths": paths, "content_paths": content_paths, "entry_paths": entry_paths}

    if func == "owned":
        owner = inp.get("owner", "default")
        r0 = _mc(M, "bind_owner_enter", {"owner": owner})
        if _isr(r0):
            return r0
        called = {"owner": owner, **(inp.get("call_kwargs") or {})}
        r = _mc(M, "owned_call", called)
        _mc(M, "bind_owner_exit", {"owner": owner,
                                   "exc": r["__raise__"] if _isr(r) else None})
        return r

    if func == "read_device_base":
        e = _mc(M, "path_expanduser", {"path": inp.get("path", "")})
        if _isr(e):
            if cls_is(e["__raise__"], "ValueError"):
                h = _mc(M, "hosted_scope", {})
                if _isr(h):
                    return h
                if not h:
                    return e
                return {"base": None, "unanchored": True}
            return e
        expanded = e if isinstance(e, str) else ""
        if expanded.startswith("/"):
            return {"base": None, "unanchored": False}
        b = _mc(M, "resolve_base_dir", {"task_id": inp.get("task_id", "default")})
        if _isr(b):
            return b
        return {"base": b if isinstance(b, str) else str(b), "unanchored": False}

    if func == "dedup_stub_or_block":
        k = inp.get("dedup_key") or []
        key = "%s|%s|%s" % (k[0] if len(k) > 0 else "",
                            int(k[1]) if len(k) > 1 else 0,
                            int(k[2]) if len(k) > 2 else 0)
        td = copy.deepcopy(inp.get("task_data") or {})
        dh = td.setdefault("dedup_hits", {})
        hits = int(dh.get(key, 0)) + 1
        dh[key] = hits
        r = _mc(M, "cap_read_tracker_data", {"task_data": td})
        if _isr(r):
            return r
        if hits >= 2:
            return {"__te__": {
                "error": ("BLOCKED: You have called read_file on this exact region %d times and "
                          "the file has NOT changed. STOP calling read_file for this path \u2014 "
                          "the content from your earlier read_file result in this conversation is "
                          "still current. Proceed with your task using the information you already have."
                          % (hits + 1)),
                "path": inp.get("path", ""), "already_read": hits + 1,
                "guardrail_refusal": True}}
        return {"__te__": {
            "status": "unchanged", "message": _READ_DEDUP_STATUS_MESSAGE,
            "path": inp.get("path", ""), "dedup": True, "content_returned": False}}

    return {"__raise__": "ValueError", "msg": "unknown func: %s" % func}


def mirror(func, inp):
    M = {"scripts": {}, "idx": {}, "trace": []}
    return {"ret": _m_dispatch(M, func, inp), "trace": M["trace"]}


def load_fixtures():
    out = []
    with open(FIXTURE, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("{"):
                out.append(json.loads(line))
    return out


def selftest(ns):
    cases = fixtures = mismatch = 0
    per_func = {}
    for case in load_fixtures():
        func, inp = case.get("func"), case.get("in") or {}
        if func not in ALL_FUNCS:
            print("SELFTEST unknown func: %s" % func)
            mismatch += 1
            continue
        fixtures += 1
        per_func[func] = per_func.get(func, 0) + 1
        # LIVE
        CUR.update({"in": inp, "trace": [], "scripts": inp.get("seams", {}),
                    "idx": {}, "raised": []})
        live_ret = drive(ns, func, inp)
        live = {"ret": norm(live_ret), "trace": CUR["trace"]}
        # MIRROR
        mir = mirror(func, inp)
        cases += 1
        if canon(live) != canon(mirror(func, inp)):
            mismatch += 1
            print("SHADOW DIFF %s" % func)
            print("  MIRROR: %s" % json.dumps(mir, ensure_ascii=False, default=str))
            print("  LIVE  : %s" % json.dumps(live, ensure_ascii=False, default=str))
    print("ORACLE file_tools_flow2: selftest %d cases, %d mismatches (%d funcs)" % (
        cases, mismatch, len(per_func)))
    return 1 if mismatch or not cases else 0


def emit_mirror():
    for case in load_fixtures():
        func, inp = case.get("func"), case.get("in") or {}
        env = {"func": func, "in": inp, "out": mirror(func, inp)}
        print(json.dumps(env, ensure_ascii=False))


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "--emit-mirror":
        emit_mirror()
        return 0
    ns = extract()
    if mode == "--selftest":
        return selftest(ns)
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func, inp = rec.get("func"), rec.get("in") or {}
        got = rec.get("out") or {}
        cases += 1
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        CUR.update({"in": inp, "trace": [], "scripts": inp.get("seams", {}),
                    "idx": {}, "raised": []})
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE file_tools_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE file_tools_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    sys.exit(main())
