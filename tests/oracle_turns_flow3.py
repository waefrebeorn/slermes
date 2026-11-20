#!/usr/bin/env python3
"""Oracle for the remaining-unclaimed turns.py FLOW port (turns_flow3.c).

Defining module: session_rules/turns.py. A CRLF-aware PoP grep over src/+include/
shows 39 turns.py defs already claimed (turns_flow2 + four other flow ports).
The 17 defs driven here are the remaining UNCLAIMED string/dict helpers of the
turn-policy cluster: summary detection, user-turn classification, handoff
unwrapping, display projection, rewind and user-index scanning. Regex/global-
registry/sentinel-codec/multi-pass-orchestrator leaves stay out of scope.

The live defs are AST-extracted from turns.py into a registered sys.modules
namespace; module constants are seeded by executing the top-level literal
assignments (so f-strings resolve); annotations stay lazy. The heavy quarry
import chain is never imported. Claimed helpers the port needs are extracted as
private dependencies but are NOT in KINDS, so no claim is made here.

Compares the decision SEQUENCE (scripted seams, empty for this cluster), the
final return ("ret") and mutated params ("state", always {} here). Fail-closed:
missing defs or 0 cases => exit 2. Per-case wrapper catches BaseException.
Summary: 'ORACLE turns_flow3: N cases, M mismatches'.

--self-test drives every fixture through the LIVE defs.
--shadow-test runs a literal Python mirror of the C port over every fixture and
diffs it against the live drive (expect 0 shadow diffs).
"""
import ast
import copy
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

# 17 UNCLAIMED defs (KINDS) + 7 claimed private deps (extracted, NOT claimed).
KINDS = [
    "_starts_with_summary_prefix", "classify_summary_content",
    "_is_context_summary_content", "_has_compressed_summary_metadata",
    "_is_context_summary_message", "_is_synthetic_compression_user_turn",
    "_is_blank_user_turn", "_is_actionable_user_turn", "_is_real_user_turn",
    "is_compaction_summary_message", "_strip_context_summary_handoff_message",
    "_handoff_only_content", "split_user_originated_turn",
    "user_originated_turn_view", "history_before_user_originated_turn",
    "project_compaction_message_for_display", "user_indices",
]
PRIVATE_DEPS = [
    "_content_text_for_contains", "_part_text", "_with_part_text",
    "typed_prompt_content", "withheld_prompt_text", "_display_metadata_str",
    "drop_stale_api_content",
]
MUT_KINDS = {}


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


# ── per-case script backend (mirrors C tf3_call; no seams here) ──
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
    mod = new_module("tf3_oracle_turns")
    ns = mod.__dict__
    ns.update({
        "Any": Any, "Optional": Optional, "Mapping": Mapping,
        "MutableMapping": MutableMapping, "Counter": Counter,
        "re": re, "json": json, "logging": logging, "copy": copy,
        "CONTENT_JSON_PREFIX": "\x00json:",
        "CONTENT_REFUSED_DISPLAY_KIND": "refused",
    })
    src = open(TURNS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # Seed module constants by executing top-level literal assignments in order.
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]),
                             "<turns-const>", "exec", flags=_FLAGS, dont_inherit=True), ns)
            except Exception:
                pass
    quiet = logging.getLogger("tf3_oracle_quiet")
    quiet.addHandler(logging.NullHandler())
    quiet.propagate = False
    ns["logger"] = quiet
    want = set(KINDS) | set(PRIVATE_DEPS)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec_node(src, node, ns, TURNS_SRC)
    missing = [k for k in want if k not in ns]
    if missing:
        print(f"FATAL oracle_turns_flow3: turns defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_turns()


# ── per-case drive (LIVE reference) ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    mut = {}

    def mat(k, default=None):
        return deep_mat(args.get(k, default), ctx)

    try:
        if func == "_starts_with_summary_prefix":
            ret = NS["_starts_with_summary_prefix"](mat("text"))
        elif func == "classify_summary_content":
            ret = NS["classify_summary_content"](mat("content"))
        elif func == "_is_context_summary_content":
            ret = NS["_is_context_summary_content"](mat("content"))
        elif func == "_has_compressed_summary_metadata":
            ret = NS["_has_compressed_summary_metadata"](mat("message"))
        elif func == "_is_context_summary_message":
            ret = NS["_is_context_summary_message"](mat("message"))
        elif func == "_is_synthetic_compression_user_turn":
            ret = NS["_is_synthetic_compression_user_turn"](mat("message"))
        elif func == "_is_blank_user_turn":
            ret = NS["_is_blank_user_turn"](mat("message"))
        elif func == "_is_actionable_user_turn":
            ret = NS["_is_actionable_user_turn"](mat("message"))
        elif func == "_is_real_user_turn":
            ret = NS["_is_real_user_turn"](mat("message"))
        elif func == "is_compaction_summary_message":
            ret = NS["is_compaction_summary_message"](mat("message"))
        elif func == "_strip_context_summary_handoff_message":
            ret = NS["_strip_context_summary_handoff_message"](mat("message"))
        elif func == "_handoff_only_content":
            ret = NS["_handoff_only_content"](mat("content"))
        elif func == "split_user_originated_turn":
            ret = NS["split_user_originated_turn"](mat("message"))
        elif func == "user_originated_turn_view":
            ret = NS["user_originated_turn_view"](mat("message"))
        elif func == "history_before_user_originated_turn":
            ret = NS["history_before_user_originated_turn"](mat("messages"), mat("index"))
        elif func == "project_compaction_message_for_display":
            ret = NS["project_compaction_message_for_display"](mat("message"))
        elif func == "user_indices":
            ret = NS["user_indices"](mat("messages"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {k: transport(v) for k, v in mut.items()}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: a literal transcription of the C tf3_* helpers and entry
# points. Operates on transported args, emits transported values; diffed vs
# the live drive() over every fixture. Zero diffs proves the C transcription.
# ─────────────────────────────────────────────────────────────────────────────
def _sd(v): return isinstance(v, dict)
def _ss(v): return isinstance(v, str)
def _sl(v): return isinstance(v, list)


def _strip(s): return s.strip() if isinstance(s, str) else None
def _lstrip(s): return s.lstrip() if isinstance(s, str) else None
def _contains(s, p): return isinstance(s, str) and p in s
def _startswith(s, p): return isinstance(s, str) and s.startswith(p)
def _find(s, p):
    if not isinstance(s, str): return -1
    return s.find(p)
def _rfind(s, p):
    if not isinstance(s, str): return -1
    return s.rfind(p)


def _split2(s, sep):
    if not isinstance(s, str) or sep not in s: return (None, None, False)
    b, a = s.split(sep, 1)
    return (b, a, True)


def _replace_all(s, old, rep):
    if not isinstance(s, str): return None
    return s.replace(old, rep)


def _py_str(v):
    if v is None: return "None"
    if isinstance(v, bool): return "True" if v else "False"
    if isinstance(v, str): return v
    if isinstance(v, (int, float)):
        if isinstance(v, float) and v.is_integer() and abs(v) < 1e16:
            return str(int(v))
        if isinstance(v, float):
            return "%.17g" % v
        return str(v)
    return json.dumps(v, ensure_ascii=False)


# --- constants mirrored from the C (loaded from turns.py const snapshot) ---
_SRC_CONST = {}
def _load_consts():
    src = open(TURNS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {"Any": Any, "Optional": Optional, "Mapping": Mapping,
          "MutableMapping": MutableMapping, "CONTENT_JSON_PREFIX": "\x00json:",
          "CONTENT_REFUSED_DISPLAY_KIND": "refused"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), "<c>", "exec",
                             flags=_FLAGS, dont_inherit=True), ns)
            except Exception:
                pass
    return ns
_C = _load_consts()
SUMMARY_PREFIX = _C["SUMMARY_PREFIX"]
LEGACY_SUMMARY_PREFIX = _C["LEGACY_SUMMARY_PREFIX"]
HISTORICAL_PREFIXES = tuple(_C["_HISTORICAL_SUMMARY_PREFIXES"])
MERGED_DELIM = _C["_MERGED_SUMMARY_DELIMITER"]
END_MARKER = _C["_SUMMARY_END_MARKER"]
PRIOR_HEADER = _C["_MERGED_PRIOR_CONTEXT_HEADER"]
STEER = _C["STEER_DISPLAY_KIND"]
COMPRESSED_KEY = _C["COMPRESSED_SUMMARY_METADATA_KEY"]
HAS_USER_KEY = _C["COMPRESSED_SUMMARY_HAS_USER_TURN_KEY"]
MICRO_KEY = _C["MICRO_COMPACT_MARKER_KEY"]
CARRIER_KEYS = tuple(_C["SUMMARY_CARRIER_DURABLE_DISPLAY_METADATA_KEYS"])
REFUSED = _C["CONTENT_REFUSED_DISPLAY_KIND"]
WITHHELD_PROMPT = _C["WITHHELD_PROMPT"]
PRE_LLM_BLOCK = _C["PRE_LLM_BLOCK_WITHHELD_PROMPT"]
DB_PERSISTED = _C["_DB_PERSISTED_MARKER"]
COMPACTION_FIELDS = tuple(_C["_COMPACTION_INTERNAL_FIELDS"])
SYNTH_CONTENTS = tuple(_C[n] for n in (
    "COMPRESSION_CONTINUATION_USER_CONTENT", "_LEGACY_COMPRESSION_CONTINUATION_USER_CONTENT",
    "MAX_ITERATIONS_SUMMARY_REQUEST", "_CODEX_INCOMPLETE_NUDGE", "_CODEX_ACK_CONTINUATION_NUDGE",
    "_DEGENERATE_FINAL_NUDGE", "_DROPPED_TOOLCALL_NUDGE_CONTENT", "_EMPTY_TOOL_RESPONSE_NUDGE",
    "_LENGTH_CONTINUATION_NETWORK_STUB", "_LEGACY_LENGTH_CONTINUATION_NETWORK_STUB",
    "_LENGTH_CONTINUATION_OUTPUT_LIMIT"))
SYNTH_PREFIXES = (_C["_BACKGROUND_PROCESS_NOTIFICATION_PREFIX"],
                  _C["TODO_INJECTION_HEADER"] + "\n",
                  _C["_LENGTH_CONTINUATION_DROPPED_TOOLS_PREFIX"])


def _truthy(v):
    if v is None: return False
    if isinstance(v, bool): return v
    if isinstance(v, (int, float)): return v != 0
    if isinstance(v, str): return v != ""
    if isinstance(v, (list, dict, tuple)): return len(v) > 0
    return False


def _eq_str(v, lit): return isinstance(v, str) and v == lit
def _get(o, k): return o.get(k) if _sd(o) else None


def _startswith_any(text):
    if not isinstance(text, str): return False
    if text.startswith(SUMMARY_PREFIX): return True
    if text.startswith(LEGACY_SUMMARY_PREFIX): return True
    for p in HISTORICAL_PREFIXES:
        if text.startswith(p): return True
    return False


# C: tf3_part_text / tf3_with_part_text / tf3_content_text
def _part_text(item):
    if isinstance(item, str): return item
    if isinstance(item, dict): return item.get("text")
    return None


def _with_part_text(item, text):
    if isinstance(item, dict):
        d = dict(item); d["text"] = text; return d
    return text


def _content_text(content):
    if isinstance(content, list):
        return "\n".join(t for t in (_part_text(x) for x in content) if isinstance(t, str) and t)
    if content is None: return ""
    if isinstance(content, str): return content
    return _py_str(content)


def _classify_core(content):
    text = _lstrip(_content_text(content))
    if _contains(text, MERGED_DELIM):
        _, after, _ = _split2(text, MERGED_DELIM)
        al = _lstrip(after)
        return "merged" if _startswith_any(al) else None
    return "standalone" if _startswith_any(text) else None


def _is_ctx_content(content): return _classify_core(content) is not None
def _has_compressed(m): return _sd(m) and _truthy(_get(m, COMPRESSED_KEY))


def _is_ctx_message(m):
    if not _sd(m): return False
    if _has_compressed(m): return True
    return _is_ctx_content(_get(m, "content"))


def _is_synth(m):
    if not _sd(m) or not _eq_str(_get(m, "role"), "user"): return False
    if _is_ctx_message(m): return True
    text = _strip(_content_text(_get(m, "content")))
    if text in SYNTH_CONTENTS: return True
    for p in SYNTH_PREFIXES:
        if isinstance(text, str) and text.startswith(p): return True
    return False


def _blank_part(part):
    if isinstance(part, str):
        return _strip(part) == ""
    if isinstance(part, dict) and _get(part, "type") in ("text", "input_text"):
        t = _get(part, "text")
        return isinstance(t, str) and _strip(t) == ""
    return False


def _is_blank(m):
    if not _sd(m) or not _eq_str(_get(m, "role"), "user"): return False
    if _is_ctx_message(m): return False
    content = _get(m, "content")
    if content is None: return True
    if isinstance(content, str): return _strip(content) == ""
    if not isinstance(content, list): return False
    for p in content:
        if not _blank_part(p): return False
    return True


def _is_actionable(m):
    if not _sd(m) or not _eq_str(_get(m, "role"), "user"): return False
    dk = _get(m, "display_kind")
    if (_truthy(dk) and not _eq_str(dk, STEER)) or _is_ctx_message(m): return False
    return not _is_blank(m)


def _is_real_user(m): return _is_actionable(m) and not _is_synth(m)


# C: tf3_unwrapped / tf3_through_end_marker
def _unwrapped(m, new_content):
    d = dict(m); d["content"] = new_content; d.pop(COMPRESSED_KEY, None); return d


def _through_end_marker(text):
    idx = _find(text, END_MARKER)
    if idx < 0: return text
    return text[:idx + len(END_MARKER)]


# C: tf3_handoff_only
def _handoff_only(content):
    if isinstance(content, str):
        if _contains(content, MERGED_DELIM):
            _, after, _ = _split2(content, MERGED_DELIM)
            return _through_end_marker(_lstrip(after))
        return _through_end_marker(content)
    if not isinstance(content, list): return content
    for item in content:
        tx = _part_text(item)
        if isinstance(tx, str) and _contains(tx, MERGED_DELIM):
            _, after, _ = _split2(tx, MERGED_DELIM)
            suf = _through_end_marker(_lstrip(after))
            return [_with_part_text(item, suf)] if suf else []
    projected = []
    for item in content:
        tx = _part_text(item)
        if not isinstance(tx, str): continue
        if _contains(tx, END_MARKER):
            projected.append(_with_part_text(item, tx.split(END_MARKER, 1)[0] + END_MARKER))
            return projected
        projected.append(dict(item) if isinstance(item, dict) else item)
    return projected


# C: tf3_strip_handoff
def _strip_handoff(m):
    if not _sd(m): return dict(m) if isinstance(m, dict) else m
    if not _is_ctx_message(m): return dict(m)
    content = _get(m, "content")
    if isinstance(content, str):
        if _contains(content, MERGED_DELIM):
            before, _, _ = _split2(content, MERGED_DELIM)
            prior = _strip(before)
            if prior.startswith(PRIOR_HEADER):
                prior = _lstrip(prior[len(PRIOR_HEADER):])
        elif _contains(content, END_MARKER):
            _, after, _ = _split2(content, END_MARKER)
            prior = _lstrip(after)
        else:
            prior = ""
        return _unwrapped(m, prior) if prior else None
    if isinstance(content, list):
        prior = []
        found = False
        for item in content:
            tx = _part_text(item)
            if isinstance(tx, str) and _contains(tx, MERGED_DELIM):
                before, _, _ = _split2(tx, MERGED_DELIM)
                bs = _strip(before)
                if bs: prior.append(_with_part_text(item, bs))
                found = True; break
            prior.append(dict(item) if isinstance(item, dict) else item)
        if not found:
            for i, item in enumerate(content):
                tx = _part_text(item)
                if isinstance(tx, str) and _contains(tx, END_MARKER):
                    _, after, _ = _split2(tx, END_MARKER)
                    rem = _lstrip(after)
                    legacy = [_with_part_text(item, rem)] if rem else []
                    legacy += [dict(x) if isinstance(x, dict) else x for x in content[i + 1:]]
                    return _unwrapped(m, legacy) if legacy else None
            return None
        for i, item in enumerate(prior):
            tx = _part_text(item)
            if isinstance(tx, str):
                ls = _lstrip(tx)
                if ls.startswith(PRIOR_HEADER):
                    lead = _lstrip(ls[len(PRIOR_HEADER):])
                    if lead: prior[i] = _with_part_text(item, lead)
                    else: prior.pop(i)
                    break
        return _unwrapped(m, prior) if prior else None
    return None


# C: tf3_typed_prompt_content / tf3_display_metadata_str
def _display_metadata_str(m, key):
    md = _get(m, "display_metadata")
    v = _get(md, key)
    return v if isinstance(v, str) else None


def _typed_prompt_content(content, m):
    typed = _display_metadata_str(m, WITHHELD_PROMPT)
    if typed is None: return content
    if isinstance(content, str): return _replace_all(content, PRE_LLM_BLOCK, typed)
    if isinstance(content, list):
        return [({**part, "text": part["text"].replace(PRE_LLM_BLOCK, typed)}
                 if isinstance(part, dict) and isinstance(part.get("text"), str) else part)
                for part in content]
    return content


def _carrier_key(k): return k in CARRIER_KEYS


# C: tf3_split_user -> (handoff, candidate)
def _split_user(m):
    handoff = None; candidate = None
    if not _sd(m) or not _eq_str(_get(m, "role"), "user"): return handoff, candidate
    is_summary = _is_ctx_message(m)
    if is_summary:
        handoff = {"role": "user", "content": _handoff_only(_get(m, "content")),
                   COMPRESSED_KEY: True, "display_kind": "hidden"}
        if _get(m, HAS_USER_KEY) is not None:
            handoff[HAS_USER_KEY] = _truthy(_get(m, HAS_USER_KEY))
        if _truthy(_get(m, MICRO_KEY)):
            handoff[MICRO_KEY] = True
        ts = _get(m, "timestamp")
        if ts is not None:
            handoff["timestamp"] = ts
        handoff.pop("api_content", None)
        dk = _get(m, "display_kind")
        if _truthy(dk) and not _eq_str(dk, "hidden"):
            candidate = None
        else:
            candidate = _strip_handoff(m)
        if candidate is None: return handoff, None
        pops = [COMPRESSED_KEY, HAS_USER_KEY, MICRO_KEY, DB_PERSISTED, "_row_id", "display_kind", "display_metadata"]
        for k in pops: candidate.pop(k, None)
        cm = _get(m, "display_metadata")
        if _sd(cm):
            durable = {k: copy.deepcopy(cm[k]) for k in cm if _carrier_key(k)}
            if durable: candidate["display_metadata"] = durable
        candidate.pop("api_content", None)
        if not _is_real_user(candidate): return handoff, None
        return handoff, candidate
    dk = _get(m, "display_kind")
    if _truthy(dk) and not _eq_str(dk, STEER): return None, None
    candidate = dict(m)
    pops = [COMPRESSED_KEY, HAS_USER_KEY, MICRO_KEY, DB_PERSISTED, "display_kind", "display_metadata"]
    for k in pops: candidate.pop(k, None)
    cm = _get(m, "display_metadata")
    if _sd(cm):
        durable = {k: copy.deepcopy(cm[k]) for k in cm if _carrier_key(k)}
        if durable: candidate["display_metadata"] = durable
    candidate.pop("api_content", None)
    if not _is_real_user(candidate): return None, None
    return None, candidate


def shadow(func, args):
    g = args.get
    if func == "_starts_with_summary_prefix":
        return _startswith_any(g("text"))
    if func == "classify_summary_content":
        return _classify_core(g("content"))
    if func == "_is_context_summary_content":
        return _is_ctx_content(g("content"))
    if func == "_has_compressed_summary_metadata":
        return _has_compressed(g("message"))
    if func == "_is_context_summary_message":
        return _is_ctx_message(g("message"))
    if func == "_is_synthetic_compression_user_turn":
        return _is_synth(g("message"))
    if func == "_is_blank_user_turn":
        return _is_blank(g("message"))
    if func == "_is_actionable_user_turn":
        return _is_actionable(g("message"))
    if func == "_is_real_user_turn":
        return _is_real_user(g("message"))
    if func == "is_compaction_summary_message":
        m = g("message")
        return _is_ctx_message(m) if _sd(m) else _is_ctx_content(m)
    if func == "_strip_context_summary_handoff_message":
        r = _strip_handoff(g("message"))
        return r if r is not None else None
    if func == "_handoff_only_content":
        return _handoff_only(g("content"))
    if func == "split_user_originated_turn":
        h, c = _split_user(g("message"))
        return [h, c]
    if func == "user_originated_turn_view":
        return _split_user(g("message"))[1]
    if func == "history_before_user_originated_turn":
        msgs = g("messages"); idx = g("index")
        if not isinstance(idx, int) or isinstance(idx, bool) or not isinstance(msgs, list) or idx < 0 or idx >= len(msgs):
            return {"__raise__": "IndexError", "message": "user turn index is outside the transcript"}
        sel = msgs[idx]
        h, c = _split_user(sel)
        if c is None:
            return {"__raise__": "ValueError", "message": "selected row is not a user-originated turn"}
        c["content"] = _typed_prompt_content(c.get("content"), sel)
        prefix = [dict(m) for m in msgs[:idx]]
        if h is not None: prefix.append(h)
        return [prefix, c]
    if func == "project_compaction_message_for_display":
        m = g("message")
        if not _sd(m): return None
        if not _is_ctx_message(m): return dict(m)
        p = _strip_handoff(m)
        if p is None: return None
        p = dict(p)
        for k in COMPACTION_FIELDS: p.pop(k, None)
        if p.get("display_kind") != REFUSED: p.pop("display_kind", None)
        return p
    if func == "user_indices":
        msgs = g("messages")
        out = []
        if isinstance(msgs, list):
            for i, m in enumerate(msgs):
                if _split_user(m)[1] is not None: out.append(i)
        return out
    raise RuntimeError(f"unknown shadow func {func}")


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "turns_flow3.jsonl"


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
    csrc = open(_REPO / "src" / "session_rules" / "turns_flow3.c",
                encoding="utf-8").read()
    pop_missing = [k for k in KINDS if f"tf3_{k.lstrip('_')}" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_turns_flow3: PoP funcs absent from C port: {pop_missing}")
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
            print(f"FATAL oracle_turns_flow3: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_turns_flow3: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_turns_flow3: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_turns_flow3: funcs with <5 fixture cases: {thin}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        gg = gt[i] if i < len(gt) else None
        if w != gg:
            return i, w, gg
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, gg = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(gg, ensure_ascii=False)[:400]}")
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
        gg = canon(got)
        if gg != want:
            m += 1
            report_mismatch(func, want, gg, shown)
            shown += 1
    print(f"ORACLE turns_flow3: {n} cases, {m} mismatches")
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
    print(f"SELFTEST turns_flow3: {n} cases, {bad} drive failures")
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
        raw = shadow(func, copy.deepcopy(args))
        got = canon({"ret": raw, "trace": [], "state": {}})
        if got != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:400]}")
            print(f"  shad={json.dumps(got, ensure_ascii=False)[:400]}")
    print(f"SHADOW turns_flow3: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
