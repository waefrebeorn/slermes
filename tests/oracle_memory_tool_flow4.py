#!/usr/bin/env python3
"""Oracle for tools/memory_tool.py final UNCLAIMED-remainder FLOW core
(src/tools/memory_tool_flow4.c).

A CRLF-aware PoP scan over src/ leaves exactly nine tools/memory_tool.py defs
unclaimed (the rest are claimed by memory_tool.c / memory_tool_ports.c /
memory_tool_pure.c):

    _pin_matched_entries, _gate_or_stage, _batch_op_line,
    _validate_single_op, destructive_ops, _background_delete_gate,
    _applied, _invalid, _memory_tool

This oracle AST-extracts those LIVE defs into a seeded namespace, drives them
with SimpleNamespace-style doubles + a universal scripted seam (identical event
shape on the C side), and diffs three ways:

    shadow-mirror (literal transcription of the mt4_* C algorithm) == live
    live drive                                                     == C harness

The FULL builtin exception hierarchy (py3.14 builtins) is encoded on both sides
(T4_EXC_PARENTS / C MT4_EXC_PARENTS). Scripted {"__raise__": Kind} seam results
are resolved to real exception classes on the live side (so Python's own
isinstance decides which `except` clause catches) and to a parent-table walk on
the shadow/C side. Unknown names default to a fresh Exception subclass.

Summary: ORACLE memory_tool_flow4: N cases, M mismatches, shadow-diff K.
Fail-closed on 0 cases or any shadow/oracle mismatch.
"""
import ast
import builtins
import copy
import inspect
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

# Full builtin exception hierarchy (direct builtin base). Alias names
# (EnvironmentError/IOError/WindowsError) canonicalise to OSError on BOTH the
# live side (real Python aliases) and the shadow side (canon below).
T4_EXC_PARENTS = {
    "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
    "GeneratorExit": "BaseException", "CancelledError": "BaseException",
    "BaseExceptionGroup": "BaseException", "Exception": "BaseException",
    "ExceptionGroup": "BaseExceptionGroup", "TypeError": "Exception",
    "ValueError": "Exception", "AttributeError": "Exception", "NameError": "Exception",
    "LookupError": "Exception", "ArithmeticError": "Exception", "OSError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError",
    "StopIteration": "Exception", "StopAsyncIteration": "Exception",
    "AssertionError": "Exception", "SyntaxError": "Exception", "SystemError": "Exception",
    "ImportError": "Exception", "Warning": "Exception", "RuntimeError": "Exception",
    "MemoryError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "ReferenceError": "Exception", "_IncompleteInputError": "SyntaxError",
    "PythonFinalizationError": "RuntimeError", "MT4TestExc": "RuntimeError",
    "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
    "UnboundLocalError": "NameError", "UnicodeError": "ValueError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeTranslateError": "UnicodeError", "IndexError": "LookupError",
    "KeyError": "LookupError", "ZeroDivisionError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "FloatingPointError": "ArithmeticError",
    "FileNotFoundError": "OSError", "PermissionError": "OSError",
    "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "InterruptedError": "OSError", "TimeoutError": "OSError", "ConnectionError": "OSError",
    "ProcessLookupError": "OSError", "FileExistsError": "OSError",
    "ChildProcessError": "OSError", "BlockingIOError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "ModuleNotFoundError": "ImportError", "IndentationError": "SyntaxError",
    "TabError": "IndentationError", "UserWarning": "Warning",
    "DeprecationWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "SyntaxWarning": "Warning", "RuntimeWarning": "Warning", "FutureWarning": "Warning",
    "ImportWarning": "Warning", "UnicodeWarning": "Warning", "BytesWarning": "Warning",
    "ResourceWarning": "Warning", "EncodingWarning": "Warning",
}

_ALIASES = {"EnvironmentError": "OSError", "IOError": "OSError", "WindowsError": "OSError"}


def _canon_exc(name):
    return _ALIASES.get(name, name)


# ---- live exception resolution ----
_EXC_CACHE = {}


def resolve_exc(name):
    if name in _EXC_CACHE:
        return _EXC_CACHE[name]
    if name == "MT4TestExc":
        cls = type(name, (RuntimeError,), {})
    elif name == "CancelledError":
        cls = type(name, (BaseException,), {})
    else:
        obj = getattr(builtins, name, None)
        cls = obj if (inspect.isclass(obj) and issubclass(obj, BaseException)) \
            else type(name, (Exception,), {})
    _EXC_CACHE[name] = cls
    return cls


# ---- AST extraction of the LIVE remainder defs ----
WANTED = {
    "_pin_matched_entries", "_gate_or_stage", "_batch_op_line",
    "_validate_single_op", "destructive_ops", "_background_delete_gate",
    "_applied", "_invalid", "_memory_tool",
}


def _future_flags(src):
    import __future__
    flags = 0
    for node in ast.parse(src).body:
        if isinstance(node, ast.ImportFrom) and node.module == "__future__":
            for a in node.names:
                f = getattr(__future__, a.name, None)
                if f is not None:
                    flags |= f.compiler_flag
    return flags


def extract():
    path = os.path.join(QUARRY, "tools", "memory_tool.py")
    src = open(path, encoding="utf-8").read()
    from typing import Any, Dict, List, Optional, Tuple
    ns = {
        "__name__": "mt4_extracted", "__file__": path,
        "json": json, "copy": copy,
        "Any": Any, "Dict": Dict, "List": List, "Optional": Optional, "Tuple": Tuple,
        "MemoryStore": type("MemoryStore", (), {}),
        "Path": Path,
    }
    flags = _future_flags(src)
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)
            found.add(node.name)
    missing = WANTED - found
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    # Module constants the defs read (seeded; do not exec the heavy Assign nodes).
    ns["_BG_DELETE_ACTIONS"] = ("replace", "remove", "patch")
    ns["_REPLACE_REMOVED"] = (
        "'replace' is not available; use 'patch'. old_text is the exact span to change and "
        "content is its new text; the rest of the entry is kept. To rewrite a whole entry, "
        "set old_text to the whole entry. Nothing was written.")
    ns["PATCH_DROP_HINT"] = (
        "To drop text inside an entry, patch a longer span that contains it; 'remove' deletes the whole entry.")
    return ns


NS = extract()

# ---- per-case seam recorder ----
CUR = {"trace": [], "idx": {}, "scripts": {}}


def emit(name, args, ret=None):
    CUR["trace"].append({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(ret)})


class _Decision:
    def __init__(self, d):
        d = d or {}
        self.allow = d.get("allow", False)
        self.blocked = d.get("blocked", False)
        self.message = d.get("message", "")


def _script_result(name):
    sc = CUR["scripts"].get(name)
    if sc is None:
        return None
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"] or []
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        return seq[min(i, len(seq) - 1)] if seq else None
    return sc


def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    r = _script_result(name)
    emit(name, args, copy.deepcopy(r))
    if isinstance(r, dict) and "__raise__" in r:
        raise resolve_exc(r["__raise__"])(r.get("msg", ""))
    return r


class _Obj:
    def __init__(self, **kw):
        self.__dict__.update(kw)


def make_store():
    return _Obj(
        _entries_for=lambda target: seam("store._entries_for", {"target": target}),
        _usage=lambda target: seam("store._usage", {"target": target}),
        resolve_batch_entries=lambda target, ops: seam(
            "resolve_batch_entries", {"target": target, "operations": copy.deepcopy(ops)}),
        resolve_entry=lambda target, old, action: seam(
            "resolve_entry", {"target": target, "old_text": old, "action": action}),
        apply_batch=lambda target, ops: seam(
            "apply_batch", {"target": target, "operations": copy.deepcopy(ops)}),
        add=lambda target, content: seam("store.add", {"target": target, "content": content}),
        replace=lambda target, old, content, entry=None: seam(
            "store.replace", {"target": target, "content": content, "old_text": old, "entry": entry}),
        remove=lambda target, old, entry=None: seam(
            "store.remove", {"target": target, "old_text": old, "entry": entry}),
        patch=lambda target, old, content, entry=None: seam(
            "store.patch", {"target": target, "content": content, "old_text": old, "entry": entry}),
        target_enabled=lambda target: seam("target_enabled", {"target": target}),
    )


def _tool_error(message, **extra):
    return json.dumps({"error": str(message), **extra}, ensure_ascii=False)


class _FC:
    def set(self, v):
        seam("failure_class.set", {"value": v})
        return object()

    def get(self):
        return "other"

    def reset(self, tok):
        pass


class _Logger:
    def warning(self, fmt, *args, **kw):
        seam("logger.warning", {"fmt": fmt, "args": [str(a) for a in args],
                                "exc_info": bool(kw.get("exc_info", False))})

    def debug(self, *args, **kw):
        seam("logger.debug", {"args": [str(a) for a in args]})


def setup_case(inp):
    CUR["trace"] = []
    CUR["idx"] = {}
    CUR["scripts"] = inp.get("script") or {}
    NS["FAILURE_CLASS"] = _FC()
    NS["tool_error"] = _tool_error
    NS["logger"] = _Logger()

    def _store_action(action):
        return lambda store, target, content, old, entry=None: getattr(store, action)(
            target, *( (old, content, entry) if action in ("replace", "patch")
                       else (old, entry) if action == "remove" else (content,)))

    NS["_STORE_ACTIONS"] = {
        "add": (_store_action("add"), lambda label, content, old: (f"add to {label}", content or "")),
        "replace": (_store_action("replace"), lambda label, content, old: (
            f"replace in {label}", f"entry matching: {old}\nwhole entry becomes: {content}")),
        "remove": (_store_action("remove"), lambda label, content, old: (
            f"remove from {label}", old or "")),
        "patch": (_store_action("patch"), lambda label, content, old: (
            f"patch in {label}", f"entry matching: {old}\nspan '{old}' becomes: {content} (rest of the entry is kept)")),
    }
    # Claimed siblings -> stubs (seam-driven); unclaimed siblings stay LIVE.
    NS["_memory_target_error"] = lambda store, target, operations=None: seam(
        "_memory_target_error", {"target": target, "operations": copy.deepcopy(operations)})
    NS["_apply_write_gate"] = lambda store, action, target, content, old_text, operations=None: seam(
        "_apply_write_gate", {"action": action, "target": target, "content": content,
                              "old_text": old_text, "operations": copy.deepcopy(operations)})

    wa = _Obj(
        MEMORY="memory",
        evaluate_gate=lambda kind, inline_summary, inline_detail: _Decision(
            seam("evaluate_gate", {"kind": kind, "inline_summary": inline_summary,
                                   "inline_detail": inline_detail})),
        stage_write=lambda kind, payload, **kw: seam(
            "stage_write", {"kind": kind, "payload": copy.deepcopy(payload), **kw}),
        current_origin=lambda: seam("current_origin", {}),
    )
    sp = _Obj(is_unattended_review=lambda: seam("is_unattended_review", {}))
    real_import = builtins.__import__

    def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
        if name == "tools":
            seam("import write_approval", {})
            return _Obj(write_approval=wa)
        if name == "tools.skill_provenance":
            seam("import skill_provenance", {})
            return _Obj(is_unattended_review=sp.is_unattended_review)
        return real_import(name, globals, locals, fromlist, level)

    builtins.__import__ = fake_import
    return real_import


def _pj(v):
    """Project a JSON-string return (json.dumps/tool_error) to a parsed value."""
    if isinstance(v, str):
        try:
            return json.loads(v)
        except (ValueError, TypeError):
            return v
    return v


def _call_live(func, a, store):
    if func == "batch_op_line":
        return NS["_batch_op_line"](a.get("op") or {}), None
    if func == "destructive_ops":
        return NS["destructive_ops"](a.get("payload") or {}), None
    if func == "applied":
        return NS["_applied"](a.get("result") or {}), None
    if func == "invalid":
        return NS["_invalid"](a.get("message", "")), None
    if func == "validate_single_op":
        return NS["_validate_single_op"](store, a.get("action", ""), a.get("target", "memory"),
                                         a.get("content"), a.get("old_text")), None
    if func == "pin_matched_entries":
        payload = a.get("payload") or {}
        CUR["payload"] = payload
        return NS["_pin_matched_entries"](store, payload), payload
    if func == "gate_or_stage":
        payload = a.get("payload") or {}
        CUR["payload"] = payload
        return NS["_gate_or_stage"](store, a.get("summary", ""), a.get("detail", ""), payload), payload
    if func == "background_delete_gate":
        payload = {"action": a.get("action", "add"), "target": a.get("target", "memory"),
                   "content": a.get("content"), "old_text": a.get("old_text")}
        CUR["payload"] = payload
        ops = a.get("operations")
        if ops is not None:
            payload["operations"] = ops
        return NS["_background_delete_gate"](store, a.get("action", "add"), ops,
                                             a.get("target", "memory"), a.get("content"),
                                             a.get("old_text")), payload
    if func == "memory_tool":
        return NS["_memory_tool"](a.get("action"), a.get("target", "memory"), a.get("content"),
                                  a.get("old_text"), a.get("new_text"), a.get("operations"), store), None
    raise ValueError("unknown func: %s" % func)


def _project(func, ret, payload):
    if func in ("applied", "invalid", "memory_tool"):
        value = {"outcome": ret[0], "result": _pj(ret[1])}
    elif func in ("pin_matched_entries", "gate_or_stage", "background_delete_gate", "validate_single_op"):
        value = _pj(ret)
    else:
        value = ret
    return {"value": value, "payload": payload}


def live_drive(func, inp):
    if func == "except_dispatch":
        a = inp.get("args") or {}
        raised = a.get("raised", "RuntimeError")
        e = resolve_exc(raised)(raised)
        trace = []
        caught = None
        for name in (a.get("clauses") or []):
            matched = bool(isinstance(e, resolve_exc(name)))
            trace.append({"ev": "isinstance", "args": {"name": name}, "ret": matched})
            if matched and caught is None:
                caught = name
        return {"value": {"caught_by": caught}, "payload": None, "trace": trace}
    CUR["payload"] = None
    real_import = setup_case(inp)
    try:
        ret, payload = _call_live(func, inp.get("args") or {}, make_store())
        proj = _project(func, ret, payload)
    except BaseException as ex:
        proj = {"value": {"__uncaught__": type(ex).__name__}, "payload": CUR.get("payload")}
    finally:
        builtins.__import__ = real_import
    return {"value": proj["value"], "payload": proj["payload"], "trace": CUR["trace"]}


# ---- SHADOW mirror (literal transcription of the mt4_* C algorithm) ----
def s_parent(kind):
    k = _canon_exc(kind)
    if k in T4_EXC_PARENTS:
        return T4_EXC_PARENTS[k]
    if k == "BaseException":
        return None
    return "Exception"


def s_exc_is(kind, base):
    k = kind
    for _ in range(32):
        if k is None:
            return False
        if _canon_exc(k) == _canon_exc(base):
            return True
        k = s_parent(k)
    return False


def s_seam(name, args, raise_ok=True):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    r = _script_result(name)
    emit(name, args, copy.deepcopy(r))
    if isinstance(r, dict) and "__raise__" in r:
        if raise_ok:
            return ("__raise__", r["__raise__"])
        return r
    return r


def _is_raise(v):
    return isinstance(v, tuple) and len(v) == 2 and v[0] == "__raise__"


def s_batch_op_line(a):
    op = a.get("op") or {}
    act = op.get("action", "?")
    content = op.get("content") or op.get("new_text") or ""
    old = op.get("old_text", "")
    if act == "remove":
        return "- remove: %s" % old
    if act == "patch":
        return "- patch span '%s' -> '%s' (rest of the entry is kept)" % (old, content)
    return "- %s: %s" % (act, content)


def s_destructive_ops(a):
    payload = a.get("payload") or {}
    ops = (payload.get("operations") or []) if payload.get("action") == "batch" else [payload]
    return [op for op in ops if (op or {}).get("action") in ("replace", "remove", "patch")]


def s_applied(a):
    r = a.get("result") or {}
    return {"outcome": "success" if r.get("success") else "failed",
            "result": _pj(json.dumps(r, ensure_ascii=False))}


def s_invalid(a):
    s_seam("failure_class.set", {"value": "invalid_args"})
    msg = a.get("message", "")
    return {"outcome": "rejected",
            "result": _pj(json.dumps({"error": msg, "success": False}, ensure_ascii=False))}


def s_validate_single_op(a):
    action = a.get("action", "")
    content = a.get("content")
    old = a.get("old_text")
    if action == "add" and not content:
        s_seam("failure_class.set", {"value": "missing_content"})
        return _pj(_tool_error("Content is required for 'add' action.", success=False))
    if action in ("remove", "patch") and not old:
        s_seam("failure_class.set", {"value": "missing_old_text"})
        msg = ("'%s' needs old_text -- a short unique substring of the entry to %s. "
               "None was provided. Reissue the %s with old_text set to part of one of "
               "the current_entries below." % (action, action, action))
        return _pj(json.dumps({
            "success": False, "error": msg,
            "current_entries": s_seam("store._entries_for", {"target": a.get("target", "memory")}),
            "usage": s_seam("store._usage", {"target": a.get("target", "memory")}),
        }, ensure_ascii=False))
    if action == "patch" and not content:
        s_seam("failure_class.set", {"value": "missing_content"})
        return _pj(_tool_error(
            "content (the new text for the old_text span) is required for 'patch' action. "
            + T4_CONST["PATCH_DROP_HINT"], success=False))
    return None


def s_pin_matched_entries(a):
    payload = a.get("payload") or {}
    target = payload.get("target", "memory")
    action = payload.get("action")
    if action == "batch":
        r = s_seam("resolve_batch_entries", {"target": target,
                                            "operations": copy.deepcopy(payload.get("operations"))})
        if _is_raise(r):
            return r
        if isinstance(r, dict) and r.get("success"):
            ops = payload.get("operations") or []
            entries = r.get("matched_entries") or []
            newops = []
            for i in range(min(len(ops), len(entries))):
                op = ops[i]
                ent = entries[i]
                if ent is None:
                    newops.append(op)
                else:
                    d = dict(op)
                    d["matched_entry"] = ent
                    newops.append(d)
            payload["operations"] = newops
    elif action in ("replace", "remove", "patch"):
        r = s_seam("resolve_entry", {"target": target, "old_text": payload.get("old_text") or "",
                                     "action": payload.get("action")})
        if _is_raise(r):
            return r
        if isinstance(r, dict) and r.get("success"):
            payload["matched_entry"] = r.get("matched_entry")
    else:
        return None
    return None if (isinstance(r, dict) and r.get("success")) else _pj(json.dumps(r, ensure_ascii=False))


def s_gate_or_stage(a):
    payload = a.get("payload") or {}
    imp = s_seam("import write_approval", {})
    if _is_raise(imp):
        return None if s_exc_is(imp[1], "Exception") else imp
    summary = a.get("summary", "")
    detail = a.get("detail", "")
    dec = s_seam("evaluate_gate", {"kind": "memory", "inline_summary": summary,
                                   "inline_detail": detail})
    if _is_raise(dec):
        return dec
    if dec and dec.get("allow"):
        return None
    if dec and dec.get("blocked"):
        s_seam("failure_class.set", {"value": "gate_refused"})
        return _pj(_tool_error(dec.get("message", ""), success=False))
    unmatched = s_pin_matched_entries({"payload": payload})
    if _is_raise(unmatched):
        return unmatched
    if unmatched is not None:
        return unmatched
    s_seam("failure_class.set", {"value": "staged"})
    st = s_seam("stage_write", {"kind": "memory", "payload": payload,
                                "summary": "%s: %s" % (summary, detail[:120]),
                                "origin": s_seam("current_origin", {})})
    if _is_raise(st):
        return st
    return _pj(json.dumps({"success": True, "staged": True, "pending_id": (st or {}).get("id"),
                           "message": dec.get("message", "")}, ensure_ascii=False))


def _gate_text(action, label, content, old):
    if action == "add":
        return ("add to %s" % label, content or "")
    if action == "replace":
        return ("replace in %s" % label, "entry matching: %s\nwhole entry becomes: %s" % (old, content))
    if action == "remove":
        return ("remove from %s" % label, old or "")
    if action == "patch":
        return ("patch in %s" % label,
                "entry matching: %s\nspan '%s' becomes: %s (rest of the entry is kept)" % (old, old, content))
    return ("", "")


def s_background_delete_gate(a):
    imp = s_seam("import skill_provenance", {})
    if _is_raise(imp):
        return imp
    if not s_seam("is_unattended_review", {}):
        return None
    action = a.get("action", "add")
    target = a.get("target", "memory")
    content = a.get("content")
    old = a.get("old_text")
    ops = a.get("operations")
    payload = {"action": "batch", "target": target, "operations": ops} if ops is not None \
        else {"action": action, "target": target, "content": content, "old_text": old}
    dest = s_destructive_ops({"payload": payload})
    if not dest:
        return None
    if ops is not None:
        detail = "; ".join(s_batch_op_line({"op": op}) for op in ops)
        kind = "batch"
    else:
        detail = s_batch_op_line({"op": {"action": action, "content": content, "old_text": old}})
        kind = action
    # try:
    unmatched = s_pin_matched_entries({"payload": payload})
    if _is_raise(unmatched):
        return _bg_except(unmatched[1]) if s_exc_is(unmatched[1], "Exception") else unmatched
    if unmatched is not None:
        return unmatched
    imp2 = s_seam("import write_approval", {})
    if _is_raise(imp2):
        return _bg_except(imp2[1]) if s_exc_is(imp2[1], "Exception") else imp2
    s_seam("failure_class.set", {"value": "staged"})
    summary = ("background review consolidation (%s on %s): %s" % (kind, target, detail))[:200]
    st = s_seam("stage_write", {"kind": "memory", "payload": payload, "summary": summary,
                                "origin": s_seam("current_origin", {})})
    if _is_raise(st):
        return _bg_except(st[1]) if s_exc_is(st[1], "Exception") else st
    msg = ("Background review may not delete memory entries unattended. The proposed "
           "%s was staged for your approval — review it with /memory pending "
           "(approve to apply, discard to drop)." % kind)
    return _pj(json.dumps({"success": True, "staged": True, "proposal_staged": True,
                           "pending_id": (st or {}).get("id"), "message": msg}, ensure_ascii=False))


def _bg_except(kind):
    s_seam("logger.warning", {"fmt": "Failed to stage background-review consolidation; denying",
                              "args": [], "exc_info": True})
    s_seam("failure_class.set", {"value": "gate_refused"})
    return _pj(_tool_error(
        "Background review may not delete memory entries ('patch'/'remove', including in a "
        "batch); 'add' is still available.", success=False))


def s_memory_tool(a):
    action = a.get("action")
    target = a.get("target", "memory")
    content = a.get("content")
    old = a.get("old_text")
    new_text = a.get("new_text")
    operations = a.get("operations")
    if not content and new_text:
        content = new_text
    terr = s_seam("_memory_target_error", {"target": target, "operations": operations})
    if _is_raise(terr):
        return ("__uncaught__", terr[1])
    if terr is not None:
        return {"outcome": "rejected", "result": _pj(json.dumps(terr, ensure_ascii=False))}
    if operations:
        if not isinstance(operations, list):
            return s_invalid({"message": "operations must be a list of {action, content?, old_text?} objects."})
        if any(isinstance(op, dict) and op.get("action") == "replace" for op in operations):
            return s_invalid({"message": T4_CONST["_REPLACE_REMOVED"]})
        denied = s_background_delete_gate({"action": action, "target": target,
                                           "operations": operations})
        if _is_raise(denied):
            return ("__uncaught__", denied[1])
        if denied is not None:
            return {"outcome": "rejected", "result": _pj(denied)}
        gate = s_seam("_apply_write_gate", {"action": "batch", "target": target,
                                           "operations": operations})
        if _is_raise(gate):
            return ("__uncaught__", gate[1])
        if gate is not None:
            return {"outcome": "rejected", "result": _pj(gate)}
        r = s_seam("apply_batch", {"target": target, "operations": operations})
        if _is_raise(r):
            return ("__uncaught__", r[1])
        return s_applied({"result": r})
    if not action and not operations:
        return s_invalid({"message": (
            "Missing required parameter: 'operations', a list of {action: add/patch/remove, "
            "content?, old_text?} changes; a single change is a one-item list.")})
    if action == "replace":
        return s_invalid({"message": T4_CONST["_REPLACE_REMOVED"]})
    if action not in ("add", "replace", "remove", "patch"):
        return s_invalid({"message": "Unknown action '%s'. Use: add, patch, remove" % action})
    inv = s_validate_single_op({"action": action, "target": target, "content": content, "old_text": old})
    if _is_raise(inv):
        return ("__uncaught__", inv[1])
    if inv is None:
        inv = s_background_delete_gate({"action": action, "target": target, "content": content, "old_text": old})
        if _is_raise(inv):
            return ("__uncaught__", inv[1])
    if inv is None:
        inv = s_seam("_apply_write_gate", {"action": action, "target": target,
                                           "content": content, "old_text": old})
        if _is_raise(inv):
            return ("__uncaught__", inv[1])
    if inv is not None:
        return {"outcome": "rejected", "result": _pj(inv) if isinstance(inv, str) else inv}
    r = s_seam("store." + action, {"target": target, "content": content, "old_text": old})
    if _is_raise(r):
        return ("__uncaught__", r[1])
    return s_applied({"result": r})


T4_CONST = {
    "PATCH_DROP_HINT": ("To drop text inside an entry, patch a longer span that contains it; "
                        "'remove' deletes the whole entry."),
    "_REPLACE_REMOVED": (
        "'replace' is not available; use 'patch'. old_text is the exact span to change and "
        "content is its new text; the rest of the entry is kept. To rewrite a whole entry, "
        "set old_text to the whole entry. Nothing was written."),
}


def _mt_proj(r):
    if _is_uncaught(r):
        return {"__uncaught__": r[1]}
    return r


def _is_uncaught(v):
    return isinstance(v, tuple) and len(v) == 2 and v[0] == "__uncaught__"


def shadow_drive(func, inp):
    CUR["trace"] = []
    CUR["idx"] = {}
    CUR["scripts"] = inp.get("script") or {}
    a = inp.get("args") or {}
    if func == "except_dispatch":
        raised = a.get("raised", "RuntimeError")
        trace = []
        caught = None
        for name in (a.get("clauses") or []):
            matched = bool(s_exc_is(raised, name))
            trace.append({"ev": "isinstance", "args": {"name": name}, "ret": matched})
            if matched and caught is None:
                caught = name
        return {"value": {"caught_by": caught}, "payload": None, "trace": trace}
    table = {
        "batch_op_line": lambda: {"value": s_batch_op_line(a), "payload": None},
        "destructive_ops": lambda: {"value": s_destructive_ops(a), "payload": None},
        "applied": lambda: {"value": s_applied(a), "payload": None},
        "invalid": lambda: {"value": s_invalid(a), "payload": None},
        "validate_single_op": lambda: {"value": s_validate_single_op(a), "payload": None},
        "pin_matched_entries": lambda: _pin_proj(a),
        "gate_or_stage": lambda: _gate_proj(a),
        "background_delete_gate": lambda: _bg_proj(a),
        "memory_tool": lambda: {"value": _mt_proj(s_memory_tool(a)), "payload": None},
    }
    if func not in table:
        raise ValueError("unknown func: %s" % func)
    return dict(table[func](), trace=CUR["trace"])


def _pin_proj(a):
    payload = a.get("payload") or {}
    r = s_pin_matched_entries({"payload": payload})
    if _is_raise(r):
        return {"value": {"__uncaught__": r[1]}, "payload": payload}
    return {"value": r, "payload": payload}


def _gate_proj(a):
    payload = a.get("payload") or {}
    r = s_gate_or_stage(a)
    if _is_raise(r):
        return {"value": {"__uncaught__": r[1]}, "payload": payload}
    return {"value": r, "payload": payload}


def _bg_proj(a):
    # The LIVE `_background_delete_gate` builds its OWN internal payload; the
    # payload we echo is the untouched projection from the harness args, so the
    # shadow must not share the object that pinning mutates.
    payload = {"action": a.get("action", "add"), "target": a.get("target", "memory"),
               "content": a.get("content"), "old_text": a.get("old_text")}
    if a.get("operations") is not None:
        payload["operations"] = a.get("operations")
    r = s_background_delete_gate(a)
    if _is_raise(r):
        return {"value": {"__uncaught__": r[1]}, "payload": payload}
    return {"value": r, "payload": payload}


def canon(x):
    if isinstance(x, dict):
        return ("obj", tuple(sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0])))
    if isinstance(x, (list, tuple)):
        return ("arr", tuple(canon(v) for v in x))
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", str(x))


def main():
    cases = 0
    mism = 0
    shadowdiff = 0
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
        got_cmp = {"value": got.get("value"), "payload": got.get("payload"),
                   "trace": got.get("trace") or []}
        cases += 1
        try:
            want = live_drive(func, copy.deepcopy(inp))
            shadow = shadow_drive(func, copy.deepcopy(inp))
        except BaseException as e:
            print("DRIVE-ERROR %s: %s: %s" % (func, type(e).__name__, e))
            mism += 1
            continue
        if canon(shadow) != canon(want):
            shadowdiff += 1
            print("SHADOW-DIFF %s" % func)
            print("  shadow: %s" % json.dumps(shadow, ensure_ascii=False))
            print("  live  : %s" % json.dumps(want, ensure_ascii=False))
        if canon(want) != canon(got_cmp):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got_cmp, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    print("ORACLE memory_tool_flow4: %d cases, %d mismatches, shadow-diff %d"
          % (cases, mism, shadowdiff))
    return 0 if (mism == 0 and shadowdiff == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
