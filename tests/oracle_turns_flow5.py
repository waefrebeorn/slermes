#!/usr/bin/env python3
"""Oracle for the turns.py REMAINDER beyond turns_flow4 (turns_flow5.c).

Defining module: session_rules/turns.py. A CRLF-aware PoP grep over src/+include/
shows 64 turns.py defs already claimed (turns_flow2/flow3/flow4 + sr/sr2/si/flow
ports). This module claims the last self-contained slice: the regex context
scrubber / view projection (sanitize_context, _loaded_view_content,
comparison_content), the payload predicates, the absorbed-row bookkeeping, the
4 sequence-repair passes and the repair_message_sequence orchestrator.

All 17 defs in KINDS are UNCLAIMED at authoring time. Claimed collaborators
this cluster needs (tool-id variant codec, _durable_content, _json_or,
split_user_originated_turn) are extracted as private dependencies but are NOT
in KINDS, so no claim is made here.

The live defs are AST-extracted from turns.py into a registered sys.modules
namespace; module constants are seeded by executing top-level assignments. The
heavy quarry import chain is never imported. Compares the mutated args
("state.args") and the final return ("ret"); the decision trace is empty for
this pure cluster. Fail-closed: missing defs, PoP drift, exception drift, or 0
cases => exit 2. Per-case wrapper catches BaseException. Full builtin exception
hierarchy is carried (EXC_CLASSES) and audited. Summary:
'ORACLE turns_flow5: N cases, M mismatches'.

--self-test drives every fixture through the LIVE defs.
--shadow-test runs a literal Python mirror of the C port over every fixture and
diffs it against the live drive (expect 0 shadow diffs).
"""
import ast
import copy
import json
import logging
import os
import re
import sys
import textwrap
import threading
import types
from collections import Counter, OrderedDict

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
TURNS_SRC = os.path.join(Q, "session_rules", "turns.py")


# 17 UNCLAIMED defs (KINDS) + private deps (extracted, NOT claimed).
KINDS = [
    "sanitize_context", "_loaded_view_content", "comparison_content",
    "_content_has_payload", "_msg_has_payload", "_merge_assistant_into",
    "_remember_absorbed_row", "_count_unnamed_row", "_remember_own_row",
    "_retire_dropped_row", "_retire_leading_drops",
    "_merge_consecutive_assistants", "_drop_stray_tool_results",
    "_prune_unanswered_tool_calls", "_merge_consecutive_users",
    "_normalize_sentinel_encoded_content", "repair_message_sequence",
]
PRIVATE_DEPS = [
    "_json_or", "decode_content", "_uid_occurrences", "merge_tool_call_uids",
    "per_occurrence_tool_call_uids", "_tc_field", "_expand_tool_id_variants",
    "tool_call_id_variants", "tool_result_id_variants", "coalesce_tool_call_id",
    "retired_row_payload", "_is_multimodal_tool_result", "_is_text_part",
    "_multimodal_text_summary", "_image_marker", "_durable_content",
    "uid_list", "message_uid_or_none", "_is_empty_content", "drop_typed_ask",
    "record_absorbed_message", "_is_codex_interim", "drop_stale_api_content",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    import __future__ as _fut
    exec(compile(text, path, "exec", flags=_fut.annotations.compiler_flag,
                 dont_inherit=True), ns)


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
                return int(f) if f.is_integer() else f
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
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
_CATCH_ALIASES = {
    "IOError": OSError, "EnvironmentError": OSError, "WindowsError": OSError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or _CATCH_ALIASES.get(kind) or type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    try:
        if en is not None and issubclass(cls, OSError):
            return cls(en, msg)
        return cls(msg)
    except TypeError:
        try:
            return cls()
        except Exception:
            return Exception(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


def caught(spec, catch_name):
    base = EXC_CLASSES.get(catch_name) or _CATCH_ALIASES.get(catch_name)
    if base is None:
        return False
    return isinstance(make_exc(spec), base)


def audit_exceptions():
    bad = []
    for kn in EXC_CLASSES:
        spec = {"type": kn, "message": "x"}
        for cn in list(EXC_CLASSES) + list(_CATCH_ALIASES):
            real = EXC_CLASSES.get(cn) or _CATCH_ALIASES.get(cn)
            want = isinstance(make_exc(spec), real)
            if caught(spec, cn) != want:
                bad.append((kn, cn))
    return bad


# ── per-case script backend (carried for contract; no seams here) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            self.trace.append({"name": name, "args": args,
                               "ret": proj_exc(RuntimeError(f"no script for {name}"))})
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            entry = entry[idx]
        ret = entry.get("ret") if isinstance(entry, dict) and entry.get("__script__") else entry
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


SEAM_NAMES = []


class _NullLock:
    def __enter__(self): return self
    def __exit__(self, *a): return False


# ── turns.py namespace (live defs + seeded constants) ──
def extract_turns():
    mod = new_module("tf5_oracle_turns")
    ns = mod.__dict__
    ns.update({
        "__name__": "tf5_oracle_turns",
        "Any": object, "Optional": object, "Mapping": dict,
        "MutableMapping": dict, "re": re, "json": json, "logging": logging,
        "copy": copy, "threading": threading, "OrderedDict": OrderedDict,
        "Counter": Counter,
        "CONTENT_JSON_PREFIX": "\x00json:",
        "CONTENT_REFUSED_DISPLAY_KIND": "content_refused",
    })
    src = open(TURNS_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    import __future__ as _fut
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]),
                             "<turns-const>", "exec", flags=_fut.annotations.compiler_flag,
                             dont_inherit=True), ns)
            except Exception:
                pass
    quiet = logging.getLogger("tf5_oracle_quiet")
    quiet.addHandler(logging.NullHandler())
    quiet.propagate = False
    ns["logger"] = quiet
    # split_user_originated_turn needs a large claimed dependency chain from
    # flow3; for _merge_consecutive_users only slot 0 is used and the fixture
    # domain keeps user rows plain. Mirror the C carrier check with a stub.
    def split_stub(message):
        if isinstance(message, dict) and message.get("compressed_summary_metadata"):
            return ({"handoff": True}, None)
        return (None, None)
    ns["split_user_originated_turn"] = split_stub
    want = set(KINDS) | set(PRIVATE_DEPS)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want:
            exec_node(src, node, ns, TURNS_SRC)
    ns["_decode_content"] = ns["decode_content"]
    ns["_SEQUENCE_REPAIR_PASSES"] = (
        ns["_merge_consecutive_assistants"], ns["_drop_stray_tool_results"],
        ns["_prune_unanswered_tool_calls"], ns["_merge_consecutive_users"],
    )
    missing = [k for k in want if k not in ns]
    if missing:
        print(f"FATAL oracle_turns_flow5: turns defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_turns()


def drive(func, inp, ctx=None):
    """Run the LIVE defs on a deep copy of args; return the transported result."""
    args = copy.deepcopy(inp.get("args") or {})
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    g = args.get
    try:
        if func == "sanitize_context":
            ret = NS["sanitize_context"](g("text", ""))
        elif func == "_loaded_view_content":
            ret = NS["_loaded_view_content"](g("role"), g("content"))
        elif func == "comparison_content":
            ret = NS["comparison_content"](g("message") or {})
        elif func == "_content_has_payload":
            ret = NS["_content_has_payload"](g("content"))
        elif func == "_msg_has_payload":
            ret = NS["_msg_has_payload"](g("msg") or {})
        elif func == "_merge_assistant_into":
            ret = NS["_merge_assistant_into"](g("prev"), g("msg"))
        elif func == "_remember_absorbed_row":
            ret = NS["_remember_absorbed_row"](g("survivor"), g("dropped"), folded=g("folded", False))
        elif func == "_count_unnamed_row":
            ret = NS["_count_unnamed_row"](g("survivor"), g("retired"))
        elif func == "_remember_own_row":
            ret = NS["_remember_own_row"](g("survivor"))
        elif func == "_retire_dropped_row":
            ret = NS["_retire_dropped_row"](g("kept"), g("dropped"), g("leading"))
        elif func == "_retire_leading_drops":
            ret = NS["_retire_leading_drops"](g("kept"), g("leading"))
        elif func in ("_merge_consecutive_assistants", "_drop_stray_tool_results",
                      "_prune_unanswered_tool_calls", "_merge_consecutive_users"):
            out_list, made = NS[func](g("messages") or [])
            ret = [out_list, made]
        elif func == "_normalize_sentinel_encoded_content":
            ret = NS["_normalize_sentinel_encoded_content"](g("messages") or [])
        elif func == "repair_message_sequence":
            ret = NS["repair_message_sequence"](None, g("messages") or [])
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {"args": canon(args)}}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: literal Python transcription of the C tf5_* helpers / entries.
# Operates on transported values; diffed vs the live drive over every fixture.
# Zero diffs proves the C transcription (not just that live agrees with itself).
# ─────────────────────────────────────────────────────────────────────────────
PFX = "\x00json:"
DBP = "_db_persisted"
STEER = "steer"
MERGED_PREFIX = "_merged_turn_prefix"
MUID = "message_uid"
ABS_UIDS = "_absorbed_message_uids"
TCU = "_tool_call_uids"
MDUR = "_merged_durable_rows"
UDUR = "_unnamed_durable_rows"
RDUR = "_retired_durable_rows"
OWN = "_own_row"
TYPED_ASK = "typed_ask"


def S_truthy(v):
    return bool(v)


def S_int(v):
    return int(v) if v not in (None, False, "") else (1 if v is None or v is False or v == "" else int(v))


def S_uids(v):
    if not isinstance(v, list):
        return []
    return list(dict.fromkeys(u for u in v if isinstance(u, str) and u))


def S_drop_typed(m):
    md = m.get("display_metadata")
    if isinstance(md, dict) and TYPED_ASK in md:
        m["display_metadata"] = {k: v for k, v in md.items() if k != TYPED_ASK}


def S_empty_content(c):
    return c is None or c == [] or (isinstance(c, str) and not c.strip())


def S_muid(m):
    u = m.get(MUID) if isinstance(m, dict) else None
    return u if isinstance(u, str) and u else None


def S_record_absorbed(surv, drop, dropped_leads=False):
    if not S_empty_content(drop.get("content")):
        S_drop_typed(surv)
    sv = S_muid(surv)
    dv = S_muid(drop)
    own = S_uids(surv.get(ABS_UIDS))
    theirs = S_uids(drop.get(ABS_UIDS))
    if dropped_leads and dv:
        surv[MUID] = dv
        ordered = theirs + ([sv] if sv else []) + own
    else:
        ordered = own + ([dv] if dv else []) + theirs
    absorbed = [u for u in dict.fromkeys(ordered) if u != surv.get(MUID)]
    if absorbed:
        surv[ABS_UIDS] = absorbed


def S_expand(vals):
    out = set()
    for raw in vals:
        v = raw.strip() if isinstance(raw, str) else ""
        if v:
            out.add(v)
            out.update(p for p in (x.strip() for x in v.split("|")) if p)
    return out


def S_call_vars(tc):
    if not isinstance(tc, dict):
        return set()
    return S_expand(tc.get(k) for k in ("call_id", "id", "response_item_id"))


def S_result_vars(tid):
    return S_expand((tid,))


def S_coalesce(tc):
    if not isinstance(tc, dict):
        return ""
    for raw in (tc.get("call_id"), tc.get("id")):
        v = raw.strip() if isinstance(raw, str) else ""
        if v:
            return v.split("|", 1)[0].strip() or v
    return ""


def S_uid_occ(v):
    return list(v) if isinstance(v, list) else [v]


def S_merge_uids(into, extra):
    merged = dict(into)
    for k, u in extra.items():
        merged[k] = S_uid_occ(merged[k]) + S_uid_occ(u) if k in merged else u
    return merged


def S_per_occ(uids, calls):
    counts = Counter(S_coalesce(tc) for tc in calls if S_coalesce(tc))
    expanded = dict(uids)
    for cid, n in counts.items():
        if n > 1 and isinstance(expanded.get(cid), str):
            expanded[cid] = [expanded[cid]] * n
    return expanded


def S_retired_payload(m):
    p = {"role": m.get("role"), "content": m.get("content")}
    if m.get("tool_call_id"):
        p["tool_call_id"] = m["tool_call_id"]
    calls = [c.get("id") for c in (m.get("tool_calls") or []) if isinstance(c, dict)]
    if calls:
        p["tool_call_ids"] = calls
    return p


_RE_FENCE = re.compile(r'</?\s*memory-context\s*>', re.IGNORECASE)
_RE_CTX = re.compile(r'<\s*memory-context\s*>[\s\S]*?</\s*memory-context\s*>', re.IGNORECASE)
_RE_NOTE = re.compile(
    r'\[System note:\s*The following is recalled memory context,\s*NOT new user input\.\s*Treat as (?:informational background data|authoritative reference data[^\]]*)\.\]\s*',
    re.IGNORECASE)


def S_sanitize(text):
    """Independent re-based mirror of the C hand-ported fixed regexes."""
    if not isinstance(text, str):
        return text
    for pattern in (_RE_CTX, _RE_NOTE, _RE_FENCE):
        text = pattern.sub('', text)
    return text


def S_loaded(role, content):
    if role in ("user", "assistant") and isinstance(content, str):
        return S_sanitize(content).strip()
    return content


def S_durable(content):
    def mm(v):
        if isinstance(v, str):
            return v
        ts = v.get("text_summary") if isinstance(v, dict) else None
        if S_truthy(ts):
            return str(ts)
        parts = [str(p.get("text", "")) for p in (v.get("content") or [])
                 if isinstance(p, dict) and p.get("type") == "text"]
        return "\n".join(parts) if parts else "[multimodal tool result]"
    def img_marker(p):
        url = p.get("image_url")
        detail = p.get("detail")
        if not detail and isinstance(url, dict):
            detail = url.get("detail")
        return f"[screenshot detail={detail}]" if detail in ("low", "high", "auto") else "[screenshot]"

    if isinstance(content, dict) and content.get("_multimodal") is True and isinstance(content.get("content"), list):
        return mm(content)
    if not isinstance(content, list):
        return content
    txt = [str(p.get("text", "")) if p.get("type") == "text" else img_marker(p)
           for p in content if isinstance(p, dict) and (p.get("type") == "text" or p.get("type") in ("image", "image_url", "input_image"))]
    return "\n".join(txt) if txt else None


def S_content_payload(c):
    if isinstance(c, str):
        return bool(c.strip())
    if not isinstance(c, list):
        return c not in (None, "")
    return any((b.get("type") != "text" or (isinstance(b.get("text"), str) and b["text"].strip()))
               if isinstance(b, dict) else bool(b) for b in c)


def S_msg_payload(m, exclude=None):
    if S_content_payload(m.get("content")):
        return True
    if exclude != "tool_calls" and m.get("tool_calls"):
        return True
    return bool((isinstance(m.get("reasoning_content"), str) and m["reasoning_content"].strip())
                or m.get("reasoning") or m.get("reasoning_details")
                or m.get("codex_message_items") or m.get("codex_reasoning_items"))


def S_is_codex(m):
    return bool(m.get("codex_reasoning_items") or m.get("codex_message_items")
                or m.get("finish_reason") == "incomplete")


def S_is_int_id(v, own):
    return isinstance(v, int) and not isinstance(v, bool) and v > 0 and v != own


def S_remember_absorbed(surv, drop, folded):
    own_id = surv.get("_row_id")
    ids = []
    rid = drop.get("_row_id")
    if isinstance(rid, int) and not isinstance(rid, bool) and rid > 0 and rid != own_id:
        ids.append(rid)
    for older in drop.get("_absorbed_row_ids") or ():
        if isinstance(older, int) and not isinstance(older, bool) and older > 0 and older not in ids:
            ids.append(older)
    if ids:
        absorbed = surv.setdefault("_absorbed_row_ids", [])
        for row_id in ids:
            if row_id not in absorbed:
                absorbed.append(row_id)
    if drop.get(UDUR):
        surv[UDUR] = int(surv.get(UDUR) or 0) + int(drop[UDUR])
    if drop.get(RDUR):
        surv.setdefault(RDUR, []).extend({k: v for k, v in row.items() if k != OWN} for row in drop[RDUR])
    if folded:
        S_record_absorbed(surv, drop, False)


def S_loaded_row(m):
    return bool(m.get(DBP) or m.get(UDUR))


def S_count_unnamed(surv, retired):
    if S_loaded_row(surv) and S_loaded_row(retired) and not isinstance(retired.get("_row_id"), int):
        surv[UDUR] = int(surv.get(UDUR) or 0) + 1
        if not any(row.get(OWN) for row in retired.get(RDUR) or ()):
            surv.setdefault(RDUR, []).append(S_retired_payload(retired))


def S_remember_own(surv):
    recorded = surv.get(RDUR) or ()
    if (surv.get(DBP) and not isinstance(surv.get("_row_id"), int)
            and not any(isinstance(row, dict) and row.get(OWN) for row in recorded)):
        surv.setdefault(RDUR, []).append({**S_retired_payload(surv), OWN: True})


def S_retire_dropped(kept, drop, leading):
    if not kept:
        leading.append(drop)
    elif isinstance(kept[-1], dict):
        S_count_unnamed(kept[-1], drop)
        S_remember_absorbed(kept[-1], drop, False)


def S_retire_leading(kept, leading):
    for drop in (leading if kept and isinstance(kept[0], dict) else ()):
        S_count_unnamed(kept[0], drop)
        S_remember_absorbed(kept[0], drop, False)


def S_merge_asst(prev, msg):
    prev_calls = list(prev.get("tool_calls") or [])
    new_calls = list(msg.get("tool_calls") or [])
    calls_changed = False
    if new_calls:
        prev["tool_calls"] = prev_calls + new_calls
        if isinstance(msg.get(TCU), dict) and msg.get(TCU):
            prev[TCU] = S_merge_uids(
                S_per_occ(prev.get(TCU) if isinstance(prev.get(TCU), dict) else {}, prev_calls),
                S_per_occ(msg[TCU], new_calls))
        calls_changed = True
    elif prev_calls:
        prev["tool_calls"] = prev_calls
    else:
        calls_changed = "tool_calls" in prev
        prev.pop("tool_calls", None)
    pc, nc = prev.get("content"), msg.get("content")
    rewritten = False
    text_kept = not nc
    if isinstance(pc, str) and isinstance(nc, str):
        text_kept = True
        joined = "\n".join(p for p in (pc.strip(), nc.strip()) if p)
        prev["content"] = joined
        rewritten = joined != pc
    elif not pc and nc is not None:
        prev["content"] = nc
        rewritten = nc != pc
        text_kept = True
    carried = False
    if not prev.get("reasoning_content") and msg.get("reasoning_content"):
        prev["reasoning_content"] = msg["reasoning_content"]
        carried = True
    if rewritten:
        prev.pop("api_content", None)
    if rewritten or calls_changed or carried:
        prev.pop(DBP, None)
    return text_kept


def S_pass0(messages):
    repairs = 0
    collapsed = []
    for msg in messages:
        prev = collapsed[-1] if collapsed and isinstance(collapsed[-1], dict) else None
        if (prev is not None and prev.get("role") == "assistant"
                and isinstance(msg, dict) and msg.get("role") == "assistant"
                and not S_is_codex(msg) and not S_is_codex(prev)):
            if prev.get("finish_reason") in {"verification_required", "verify_hook_continue"}:
                S_count_unnamed(msg, prev)
                S_remember_absorbed(msg, prev, False)
                collapsed[-1] = msg
            else:
                S_count_unnamed(prev, msg)
                S_remember_own(prev)
                S_remember_absorbed(prev, msg, folded=S_merge_asst(prev, msg))
            repairs += 1
            continue
        collapsed.append(msg)
    return collapsed, repairs


def S_pass1(messages):
    repairs = 0
    known, matched = {}, set()
    nxt = 0
    filtered, leading = [], []
    for msg in messages:
        role = msg.get("role") if isinstance(msg, dict) else None
        if role in ("assistant", "user"):
            known, matched = {}, set()
            for tc in ((msg.get("tool_calls") or []) if role == "assistant" else ()):
                variants = S_call_vars(tc)
                if variants:
                    for cid in variants:
                        known.setdefault(cid, nxt)
                    nxt += 1
        elif role == "tool":
            rv = S_result_vars(msg.get("tool_call_id"))
            cand = {known[cid] for cid in rv if cid in known and known[cid] not in matched}
            if rv and not cand:
                S_retire_dropped(filtered, msg, leading)
                repairs += 1
                continue
            if cand:
                matched.add(min(cand))
        filtered.append(msg)
    S_retire_leading(filtered, leading)
    return filtered, repairs


def S_pass2(messages):
    repairs = 0
    pruned, leading = [], []
    for i, msg in enumerate(messages):
        if not (isinstance(msg, dict) and msg.get("role") == "assistant"
                and msg.get("tool_calls") and not S_is_codex(msg)):
            pruned.append(msg)
            continue
        answered = set()
        for f in messages[i + 1:]:
            if not (isinstance(f, dict) and f.get("role") == "tool"):
                break
            tid = (f.get("tool_call_id") or "").strip()
            if tid:
                answered.update(S_result_vars(tid))
        kept = [tc for tc in msg["tool_calls"] if S_call_vars(tc) & answered]
        if len(kept) != len(msg["tool_calls"]):
            repairs += 1
            if not kept and not S_msg_payload({k: v for k, v in msg.items() if k != "tool_calls"}):
                S_retire_dropped(pruned, msg, leading)
                continue
            if kept:
                msg["tool_calls"] = kept
            else:
                msg.pop("tool_calls", None)
            msg.pop(DBP, None)
        pruned.append(msg)
    S_retire_leading(pruned, leading)
    return pruned, repairs


def S_pass3(messages):
    def plain(c):
        return isinstance(c, str) and not c.startswith(PFX)
    repairs = 0
    merged = []
    for msg in messages:
        prev = merged[-1] if merged and isinstance(merged[-1], dict) else None
        cond = (prev is not None and prev.get("role") == "user"
                and isinstance(msg, dict) and msg.get("role") == "user"
                and prev.get("compressed_summary_metadata") is None
                and prev.get("display_kind") != STEER
                and plain(prev.get("content", "")) and plain(msg.get("content", ""))
                and bool(prev.get("observed")) == bool(msg.get("observed")))
        if cond:
            pc, nc = prev.get("content", ""), msg.get("content", "")
            mc = (pc + "\n\n" + nc) if pc and nc else (pc or nc)
            had = "api_content" in prev
            if (prev.get(DBP) or prev.get(MDUR)) and msg.get(DBP):
                prev[MDUR] = int(prev.get(MDUR) or 1) + 1
            else:
                prev.pop(MDUR, None)
            prev["content"] = mc
            if pc:
                prev[MERGED_PREFIX] = pc
            prev.pop("api_content", None)
            if prev.get("display_kind") and not msg.get("display_kind"):
                mrid = prev.get("_row_id")
                prev.pop("display_kind", None)
                if msg.get("_row_id") is not None:
                    if isinstance(mrid, int) and not isinstance(mrid, bool):
                        a = prev.setdefault("_absorbed_row_ids", [])
                        if mrid not in a:
                            a.append(mrid)
                    prev["_row_id"] = msg["_row_id"]
                prev.pop(DBP, None)
            if mc != pc or had:
                prev.pop(DBP, None)
            S_record_absorbed(prev, msg, True)
            repairs += 1
            continue
        merged.append(msg)
    return merged, repairs


def S_normalize(messages):
    for msg in messages:
        if not isinstance(msg, dict):
            continue
        c = msg.get("content")
        if isinstance(c, str) and c.startswith(PFX):
            try:
                d = json.loads(c[len(PFX):])
            except Exception:
                continue
            if d is not c:
                msg["content"] = d


def S_repair(messages):
    if not messages:
        return 0
    S_normalize(messages)
    repairs = 0
    current = messages
    for p in (S_pass0, S_pass1, S_pass2, S_pass3):
        current, made = p(current)
        repairs += made
    if repairs > 0:
        messages[:] = current
    return repairs


def shadow(func, args):
    """Literal transcription of tf5_* entry points; mutates args like the C."""
    g = args.get
    if func == "sanitize_context":
        return S_sanitize(g("text", "")), args
    if func == "_loaded_view_content":
        return S_loaded(g("role"), g("content")), args
    if func == "comparison_content":
        m = g("message") or {}
        return S_loaded(m.get("role"), S_durable(m.get("content"))), args
    if func == "_content_has_payload":
        return S_content_payload(g("content")), args
    if func == "_msg_has_payload":
        return S_msg_payload(g("msg") or {}), args
    if func == "_merge_assistant_into":
        return S_merge_asst(g("prev"), g("msg")), args
    if func == "_remember_absorbed_row":
        S_remember_absorbed(g("survivor"), g("dropped"), g("folded", False)); return None, args
    if func == "_count_unnamed_row":
        S_count_unnamed(g("survivor"), g("retired")); return None, args
    if func == "_remember_own_row":
        S_remember_own(g("survivor")); return None, args
    if func == "_retire_dropped_row":
        S_retire_dropped(g("kept"), g("dropped"), g("leading")); return None, args
    if func == "_retire_leading_drops":
        S_retire_leading(g("kept"), g("leading")); return None, args
    if func == "_merge_consecutive_assistants":
        a, n = S_pass0(g("messages") or []); return [a, n], args
    if func == "_drop_stray_tool_results":
        a, n = S_pass1(g("messages") or []); return [a, n], args
    if func == "_prune_unanswered_tool_calls":
        a, n = S_pass2(g("messages") or []); return [a, n], args
    if func == "_merge_consecutive_users":
        a, n = S_pass3(g("messages") or []); return [a, n], args
    if func == "_normalize_sentinel_encoded_content":
        S_normalize(g("messages") or []); return None, args
    if func == "repair_message_sequence":
        return S_repair(g("messages") or []), args
    raise RuntimeError(f"unknown shadow func {func}")


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "turns_flow5.jsonl"


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
    csrc = open(_REPO / "src" / "session_rules" / "turns_flow5.c", encoding="utf-8").read()
    pop_missing = [k for k in KINDS if f"tf5_{k.lstrip('_')}" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_turns_flow5: PoP funcs absent from C port: {pop_missing}")
        sys.exit(2)
    bad = audit_exceptions()
    if bad:
        print(f"FATAL oracle_turns_flow5: exception hierarchy drift: {bad[:8]}")
        sys.exit(2)
    fix = fixpath()
    if fix.exists():
        counts, bad_io = {}, set()
        for case in each_fixture():
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
            bad_io |= set((case.get("in") or {}).get("io") or {})
        if bad_io - set(SEAM_NAMES):
            print(f"FATAL oracle_turns_flow5: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_turns_flow5: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_turns_flow5: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_turns_flow5: funcs with <5 fixture cases: {thin}")
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
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:500]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:500]}")
    ws = (want.get("state") or {}).get("args")
    gs = (got.get("state") or {}).get("args")
    if ws != gs:
        print(f"  state.args want={json.dumps(ws, ensure_ascii=False)[:500]}")
        print(f"  state.args got ={json.dumps(gs, ensure_ascii=False)[:500]}")


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
    print(f"ORACLE turns_flow5: {n} cases, {m} mismatches")
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
    print(f"SELFTEST turns_flow5: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


def shadow_test():
    n = bad = 0
    for case in each_fixture():
        n += 1
        func = case["func"]
        inp = case.get("in") or {}
        want = drive(func, inp)
        ret, args = shadow(func, copy.deepcopy(inp.get("args") or {}))
        got = canon({"ret": ret, "trace": [], "state": {"args": args}})
        if got != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:500]}")
            print(f"  shad={json.dumps(got, ensure_ascii=False)[:500]}")
    print(f"SHADOW turns_flow5: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
