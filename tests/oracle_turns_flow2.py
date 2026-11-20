#!/usr/bin/env python3
"""Oracle for the unclaimed pure-helper FLOW port (src/session_rules/turns_flow2.c).

Defining module: session_rules/turns.py. A CRLF-aware PoP grep over src/+include/
shows 19 turns.py defs already claimed by chat_completion_helpers_flow.c,
store_rules_flow.c, store_rules_flow2.c and store_identity_flow.c. The 20 defs
driven here are the remaining UNCLAIMED helpers that are portable with libjson
alone (the regex/global-state/multi-pass-orchestrator helpers are out of scope
for this FLOW shape).

The live defs are AST-extracted (FunctionDef nodes) from turns.py into a
registered sys.modules namespace; module constants are seeded by executing the
top-level literal assignments (so f-strings resolve), and annotations stay lazy
under the future-annotations compiler flag. The heavy quarry import chain is
never imported.

The private C helper coalesce_tool_call_id is an un-claimed transcription (its
PoP owner is store_identity_flow.c); the oracle extracts the live def as a
private dependency but never lists it in KINDS, so no claim is made here.

Compares the DECISION SEQUENCE (scripted seams, empty for this cluster), the
final return ("ret") and the mutated mapping params ("state"). Fail-closed:
missing defs or 0 cases => exit 2. Per-case wrapper catches BaseException
(SystemExit included). Summary: 'ORACLE turns_flow2: N cases, M mismatches'.

--self-test drives every fixture through the LIVE defs.
--shadow-test runs a literal Python mirror of the C port over every fixture and
diffs it against the live drive (expect 0 shadow diffs).
"""
import ast
import json
import logging
import math
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections import Counter
from collections.abc import Mapping, MutableMapping
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
TURNS_SRC = os.path.join(Q, "session_rules", "turns.py")

_FLAGS = _fut.annotations.compiler_flag

# 20 UNCLAIMED helper defs (KINDS) + 1 private dependency (claimed elsewhere).
KINDS = [
    "_uid_occurrences", "merge_tool_call_uids", "_tc_field",
    "_expand_tool_id_variants", "tool_call_id_variants", "tool_result_id_variants",
    "per_occurrence_tool_call_uids", "index_tool_call_uids",
    "resolve_tool_call_uid", "retired_row_payload", "_is_multimodal_tool_result",
    "_is_text_part", "_multimodal_text_summary", "_is_ephemeral_scaffolding",
    "_image_marker", "_durable_content", "_content_has_payload", "_msg_has_payload",
    "_is_codex_interim", "_json_or",
]
PRIVATE_DEPS = ["coalesce_tool_call_id"]  # extracted, NOT claimed by this oracle
MUT_KINDS = {"index_tool_call_uids": ("index",)}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            if x.get("__float__") is True:
                f = float(x.get("v", 0))
                return int(f) if math.isfinite(f) and f.is_integer() else f
            if x.get("__tuple__") is True:
                return [conv(i) for i in x.get("v", [])]
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float):
            return int(x) if math.isfinite(x) and x.is_integer() else x
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection (FULL builtin hierarchy) ──
EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "TypeError": TypeError, "StopIteration": StopIteration,
    "StopAsyncIteration": StopAsyncIteration, "ArithmeticError": ArithmeticError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "SystemError": SystemError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning, "ExceptionGroup": ExceptionGroup,
    "BaseExceptionGroup": BaseExceptionGroup, "GeneratorExit": GeneratorExit,
    "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C tf2_call; no seams here) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


# ── value materialization / transport ──
def deep_mat(v, ctx=None):
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(deep_mat(i, ctx) for i in v.get("v", []))
        if v.get("__dt__") is True:
            return types.SimpleNamespace(tzinfo=v.get("tzinfo"),
                                          microsecond=v.get("microsecond", 0))
        return {k: deep_mat(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [deep_mat(i, ctx) for i in v]
    return v


def transport(v):
    if isinstance(v, float):
        return {"__float__": True, "v": v}
    if isinstance(v, tuple):
        return {"__tuple__": True, "v": [transport(i) for i in v]}
    if isinstance(v, dict):
        return {k: transport(x) for k, x in v.items()}
    if isinstance(v, list):
        return [transport(i) for i in v]
    return v


# ── seam registry (empty for this pure cluster; carried for contract) ──
SEAM_NAMES = []


# ── turns.py namespace (live defs + seeded constants) ──
def extract_turns():
    mod = new_module("tf2_oracle_turns")
    ns = mod.__dict__
    ns.update({
        "Any": Any, "Optional": Optional, "Mapping": Mapping,
        "MutableMapping": MutableMapping, "Counter": Counter,
        "re": re, "json": json, "logging": logging,
        "CONTENT_JSON_PREFIX": "\x00json:",
        "CONTENT_REFUSED_DISPLAY_KIND": "refused",
    })
    src = open(TURNS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Seed module constants by executing top-level literal assignments in order
    # (f-strings such as SUMMARY_PREFIX resolve after HISTORICAL_TASK_HEADING).
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]),
                             "<turns-const>", "exec", flags=_FLAGS, dont_inherit=True), ns)
            except Exception:
                pass
    # Silence _json_or's logger.warning so gate output stays clean.
    quiet = logging.getLogger("tf2_oracle_quiet")
    quiet.addHandler(logging.NullHandler())
    quiet.propagate = False
    ns["logger"] = quiet
    # Exec the live function defs we drive plus the private dependency.
    want = set(KINDS) | set(PRIVATE_DEPS)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec_node(src, node, ns, TURNS_SRC)
    missing = [k for k in want if k not in ns]
    if missing:
        print(f"FATAL oracle_turns_flow2: turns defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_turns()


# ── per-case drive ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    mut = {}

    def mat(k, default=None):
        return deep_mat(args.get(k, default), ctx)

    try:
        if func == "_uid_occurrences":
            ret = NS["_uid_occurrences"](mat("value"))
        elif func == "merge_tool_call_uids":
            ret = NS["merge_tool_call_uids"](mat("into"), mat("extra"))
        elif func == "_tc_field":
            ret = NS["_tc_field"](mat("tc"), args.get("key", ""))
        elif func == "_expand_tool_id_variants":
            ret = NS["_expand_tool_id_variants"](mat("values"))
        elif func == "tool_call_id_variants":
            ret = NS["tool_call_id_variants"](mat("tc"))
        elif func == "tool_result_id_variants":
            ret = NS["tool_result_id_variants"](mat("tool_call_id"))
        elif func == "per_occurrence_tool_call_uids":
            ret = NS["per_occurrence_tool_call_uids"](mat("uids"), mat("tool_calls"))
        elif func == "index_tool_call_uids":
            mut["index"] = mat("index")
            ret = NS["index_tool_call_uids"](mut["index"], mat("assistant"))
        elif func == "resolve_tool_call_uid":
            ret = NS["resolve_tool_call_uid"](mat("index"), mat("tool_call_id"))
        elif func == "retired_row_payload":
            ret = NS["retired_row_payload"](mat("message"))
        elif func == "_is_multimodal_tool_result":
            ret = NS["_is_multimodal_tool_result"](mat("value"))
        elif func == "_is_text_part":
            ret = NS["_is_text_part"](mat("p"))
        elif func == "_multimodal_text_summary":
            ret = NS["_multimodal_text_summary"](mat("value"))
        elif func == "_is_ephemeral_scaffolding":
            ret = NS["_is_ephemeral_scaffolding"](mat("msg"))
        elif func == "_image_marker":
            ret = NS["_image_marker"](mat("part"))
        elif func == "_durable_content":
            ret = NS["_durable_content"](mat("content"))
        elif func == "_content_has_payload":
            ret = NS["_content_has_payload"](mat("content"))
        elif func == "_msg_has_payload":
            ret = NS["_msg_has_payload"](mat("msg"))
        elif func == "_is_codex_interim":
            ret = NS["_is_codex_interim"](mat("m"))
        elif func == "_json_or":
            ret = NS["_json_or"](mat("raw"), mat("fallback"), args.get("warning", ""))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {k: transport(v) for k, v in mut.items()}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: a literal transcription of the C port tf2_* functions.
# Operates on transported args and emits transported values; diffed vs live
# drive() over every fixture. Zero diffs proves the C transcription faithful.
# ─────────────────────────────────────────────────────────────────────────────
def _sdict(v):
    return isinstance(v, dict)


def _snull(v):
    return v is None


def _sbool(v):
    return isinstance(v, bool)


def _struthy(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return v != ""
    if isinstance(v, (list, dict, tuple)):
        return len(v) > 0
    return False


def _sstrip(v):
    return v.strip() if isinstance(v, str) else None


def _attr_err(v, attr):
    t = "NoneType" if v is None else ("bool" if isinstance(v, bool) else
         "int" if isinstance(v, int) else "str" if isinstance(v, str) else
         "list" if isinstance(v, list) else "dict" if isinstance(v, dict) else "object")
    return {"__raise__": "AttributeError", "message": f"'{t}' object has no attribute '{attr}'"}


def _seq_sorted_strings(items):
    return sorted(set(x for x in items if isinstance(x, str)))


def _field_str(tc, key):
    if not _sdict(tc):
        return None
    v = tc.get(key)
    return _sstrip(v)


def _coalesce(tc):
    for k in ("call_id", "id"):
        value = _field_str(tc, k)
        if value:
            head = value.split("|", 1)[0].strip()
            return head if head else value
    return ""


def _expand(values):
    out = set()
    for raw in values:
        value = _sstrip(raw)
        if not value:
            continue
        out.add(value)
        for part in value.split("|"):
            p = part.strip()
            if p:
                out.add(p)
    return sorted(out)


def _py_str(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        return json.dumps(v)
    return json.dumps(v, ensure_ascii=False, separators=(", ", ": "))


def _dumps(v):
    return json.dumps(v, ensure_ascii=False, default=str)


def _is_mm(value):
    return _sdict(value) and value.get("_multimodal") is True and isinstance(value.get("content"), list)


def _image_marker(part):
    if not _sdict(part):
        return _attr_err(part, "get")
    url = part.get("image_url")
    detail = part.get("detail") if _sdict(part) else None
    if not detail and isinstance(url, dict):
        detail = url.get("detail")
    if detail in ("low", "high", "auto"):
        return f"[screenshot detail={detail}]"
    return "[screenshot]"


def _durable(content):
    if _is_mm(content):
        return _mm_summary(content)
    if not isinstance(content, list):
        return content
    txt = []
    for p in content:
        if not _sdict(p):
            continue
        ty = p.get("type")
        if ty == "text":
            txt.append(_py_str(p.get("text", "")))
        elif ty in ("image", "image_url", "input_image"):
            txt.append(_image_marker(p))
    return "\n".join(txt) if txt else None


def _mm_summary(value):
    if isinstance(value, str):
        return value
    if _is_mm(value):
        if value.get("text_summary"):
            return _py_str(value["text_summary"])
        parts = [_py_str(p.get("text", "")) for p in (value.get("content") or []) if _sdict(p) and p.get("type") == "text"]
        return "\n".join(parts) if parts else "[multimodal tool result]"
    return _dumps(value)


def shadow(func, args):
    g = args.get

    if func == "_uid_occurrences":
        v = g("value")
        return list(v) if isinstance(v, list) else [v]
    if func == "_tc_field":
        tc = g("tc")
        return tc.get(g("key", "")) if _sdict(tc) else None
    if func == "_expand_tool_id_variants":
        return _expand(g("values"))
    if func == "tool_call_id_variants":
        tc = g("tc")
        return _expand([tc.get(k) if _sdict(tc) else None for k in ("call_id", "id", "response_item_id")])
    if func == "tool_result_id_variants":
        return _expand([g("tool_call_id")])
    if func == "merge_tool_call_uids":
        into, extra = g("into"), g("extra")
        if not isinstance(into, dict):
            return {"__raise__": "TypeError", "message": "'NoneType' object is not iterable"}
        if not isinstance(extra, dict):
            return _attr_err(extra, "items")
        merged = dict(into)
        for cid, uid in (g("extra") or {}).items():
            if cid in merged:
                a = merged[cid] if isinstance(merged[cid], list) else [merged[cid]]
                b = uid if isinstance(uid, list) else [uid]
                merged[cid] = a + b
            else:
                merged[cid] = uid
        return merged
    if func == "per_occurrence_tool_call_uids":
        counts = Counter(_coalesce(tc) for tc in (g("tool_calls") or []) if _coalesce(tc))
        expanded = dict(g("uids") or {})
        for cid, count in counts.items():
            uid = expanded.get(cid)
            if count > 1 and isinstance(uid, str):
                expanded[cid] = [uid] * count
        return expanded
    if func == "index_tool_call_uids":
        index = g("index")
        assistant = g("assistant") or {}
        calls = [(tc, _expand([tc.get(k) for k in ("call_id", "id", "response_item_id")]))
                 for tc in (assistant.get("tool_calls") or [])]
        named = _seq_sorted_strings([v for _, vs in calls for v in vs])
        for v in named:
            index.pop(v, None)
        uids = assistant.get("_tool_call_uids")
        if not isinstance(uids, dict) or not uids:
            return named
        occurrence = {}
        for tc, variants in calls:
            cid = _coalesce(tc)
            uid = uids.get(cid)
            if isinstance(uid, list):
                nth = occurrence.get(cid, -1) + 1
                occurrence[cid] = nth
                uid = uid[nth] if nth < len(uid) else None
            if isinstance(uid, str) and uid:
                for v in variants:
                    index[v] = uid
        return named
    if func == "resolve_tool_call_uid":
        index = g("index")
        tcid = g("tool_call_id")
        if not index or not isinstance(tcid, str) or not tcid:
            return None
        for v in _expand([tcid]):
            if index.get(v):
                return index[v]
        return None
    if func == "retired_row_payload":
        m = g("message")
        if not _sdict(m):
            return _attr_err(m, "get")
        payload = {"role": m.get("role"), "content": m.get("content")}
        if m.get("tool_call_id"):
            payload["tool_call_id"] = m["tool_call_id"]
        calls = [c.get("id") for c in (m.get("tool_calls") or []) if _sdict(c)]
        if calls:
            payload["tool_call_ids"] = calls
        return payload
    if func == "_is_multimodal_tool_result":
        return _is_mm(g("value"))
    if func == "_is_text_part":
        p = g("p")
        return _sdict(p) and p.get("type") == "text"
    if func == "_multimodal_text_summary":
        return _mm_summary(g("value"))
    if func == "_is_ephemeral_scaffolding":
        m = g("msg")
        flags = ("_empty_recovery_synthetic", "_empty_terminal_sentinel", "_thinking_prefill",
                 "_verification_stop_synthetic", "_pre_verify_synthetic", "_kanban_stop_synthetic",
                 "_dropped_toolcall_nudge")
        return _sdict(m) and any(m.get(fl) for fl in flags)
    if func == "_image_marker":
        return _image_marker(g("part"))
    if func == "_durable_content":
        return _durable(g("content"))
    if func == "_content_has_payload":
        c = g("content")
        if isinstance(c, str):
            return bool(c.strip())
        if not isinstance(c, list):
            return c is not None
        for b in c:
            if _sdict(b):
                if b.get("type") != "text":
                    return True
                t = b.get("text")
                if isinstance(t, str) and t.strip():
                    return True
            elif b:
                return True
        return False
    if func == "_msg_has_payload":
        m = g("msg") or {}
        if _content_has_payload_shadow(m.get("content")):
            return True
        return bool(m.get("tool_calls")
                    or (isinstance(m.get("reasoning_content"), str) and m["reasoning_content"].strip())
                    or m.get("reasoning") or m.get("reasoning_details")
                    or m.get("codex_message_items") or m.get("codex_reasoning_items"))
    if func == "_is_codex_interim":
        m = g("m")
        if not _sdict(m):
            return _attr_err(m, "get")
        return bool(m.get("codex_reasoning_items")
                    or m.get("codex_message_items")
                    or m.get("finish_reason") == "incomplete")
    if func == "_json_or":
        raw = g("raw")
        if isinstance(raw, str):
            try:
                return json.loads(raw)
            except (json.JSONDecodeError, TypeError):
                return g("fallback")
        return g("fallback")
    raise RuntimeError(f"unknown shadow func {func}")


def _content_has_payload_shadow(c):
    if isinstance(c, str):
        return bool(c.strip())
    if not isinstance(c, list):
        return c is not None
    for b in c:
        if _sdict(b):
            if b.get("type") != "text":
                return True
            t = b.get("text")
            if isinstance(t, str) and t.strip():
                return True
        elif b:
            return True
    return False


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "turns_flow2.jsonl"


def each_fixture():
    p = fixpath()
    if not p.exists():
        return []
    out = []
    for line in open(p, encoding="utf-8", errors="surrogatepass"):
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def cross_check():
    csrc = open(_REPO / "src" / "session_rules" / "turns_flow2.c",
                encoding="utf-8").read()
    pop_missing = [k for k in KINDS
                   if f"tf2_{k.lstrip('_')}" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_turns_flow2: PoP funcs absent from C port: {pop_missing}")
        sys.exit(2)
    fix = fixpath()
    if fix.exists():
        counts = {}
        bad_io = set()
        for case in each_fixture():
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
            bad_io |= set((case.get("in") or {}).get("io") or {})
        if bad_io - set(SEAM_NAMES):
            print(f"FATAL oracle_turns_flow2: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_turns_flow2: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_turns_flow2: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_turns_flow2: funcs with <5 fixture cases: {thin}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


def main():
    cross_check()
    n = m = shown = 0
    data = sys.stdin.buffer.read().decode("utf-8", errors="surrogatepass")
    for line in data.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE turns_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    n = bad = 0
    counts = {}
    for case in each_fixture():
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    print(f"SELFTEST turns_flow2: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


def shadow_test():
    n = bad = 0
    for case in each_fixture():
        n += 1
        func = case["func"]
        args = (case.get("in") or {}).get("args") or {}
        want = drive(func, case.get("in") or {})
        import copy
        shadow_args = copy.deepcopy(args)
        raw = shadow(func, shadow_args)
        got = {"ret": canon(raw), "trace": [], "state": {}}
        if MUT_KINDS.get(func):
            got["state"] = canon({k: shadow_args.get(k) for k in MUT_KINDS[func]})
        g = canon(got)
        if g != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:400]}")
            print(f"  shad={json.dumps(g, ensure_ascii=False)[:400]}")
    print(f"SHADOW turns_flow2: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
