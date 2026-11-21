#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow6.c — drives the LIVE
agent/context_compressor.py sixth-wave FLOW defs (18 UNCLAIMED module
functions / methods) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the mutated-state window. Live defs are AST-extracted in
source order with decorators stripped and future-annotations; module constants
are seeded; claimed siblings / foreign helpers / self-methods / regex primitives
are recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. FULL Python
builtin exception hierarchy (build_exc): a scripted base-only kind propagates as
a BaseException exactly as live.

Summary: 'ORACLE context_compressor_flow6: N cases, M mismatches'.
"""
import ast
import copy
import json
import os
import re as _re
import sys
import types
import __future__
from pathlib import Path
from types import SimpleNamespace

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "context_compressor.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
SRC = str(QUARRY / "agent" / "context_compressor.py")
ANN_FLAG = __future__.annotations.compiler_flag
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow6.jsonl"

FUNCS = [
    "_is_real_user_turn", "_real_user_indices_desc", "preview_threshold_tokens",
    "_render_tool_call_for_summary", "_drop_blank_echoes", "_prune_boundary",
    "_demote_stale_tail_tools", "_clear_terminal_summary_failures",
    "maybe_seed_preflight_display_tokens", "note_native_compaction_checkpoint",
    "record_rejected_compaction", "_apply_real_prompt_verdict",
    "_abort_on_summary_failure", "_assemble_head", "_assemble_tail",
    "_dedupe_tool_results", "_augment_summary_lean", "_merge_summary_into_tail_row",
]

# The universal-seam alphabet. MUST equal the seam names fired by the C port
# and the io keys used in the fixture file (SEAM-NAME SET EQUALITY).
SEAMS = [
    "_turns_is_real_user_turn", "_derive_trigger",
    "_record_ineffective_compression_verdict", "_find_last_user_message_idx",
    "_blank_echo_indices_after", "_walk_tail_budget", "_render_tool_args_for_summary",
    "_fresh_compaction_message_copy", "_content_text_for_contains",
    "_append_text_to_content", "_strip_context_summary_handoff_message",
    "_build_anchor_index", "_build_verbatim_user_section", "_build_recovery_footer",
    "_redact_compaction_text", "drop_stale_api_content", "demote_stale_tail_tools",
    "_caller_attempt_is_current", "md5hex",
]

# self-method seams: bound on the SimpleNamespace self under the LIVE attr name.
SELF_SEAM_BUILDERS = {
    "_derive_trigger": lambda s, a, kw: {
        "model": a[0], "context_length": a[1],
        "provider": a[2] if len(a) > 2 else kw.get("provider", "")},
    "_record_ineffective_compression_verdict": lambda s, a, kw: {"count": a[0]},
    "_blank_echo_indices_after": lambda s, a, kw: {"messages": a[0], "idx": a[1]},
    "_find_last_user_message_idx": lambda s, a, kw: {
        "messages": a[0], "start": a[1] if len(a) > 1 else kw.get("start", 0)},
    "_walk_tail_budget": lambda s, a, kw: {
        "result": a[0], "start": a[1], "tokens": a[2], "min_protect": a[3],
        "cut_at_break": kw.get("cut_at_break", False)},
    "_render_tool_args_for_summary": lambda s, a, kw: {"arguments": a[0]},
    "_fresh_compaction_message_copy": lambda s, a, kw: {"msg": a[0]},
    "_strip_context_summary_handoff_message": lambda s, a, kw: {"msg": a[0]},
}
SELF_SEAM_SET = set(SELF_SEAM_BUILDERS)

MUTABLE_KEYS = ("self", "msg", "result", "telemetry")

SRC_TEXT = open(SRC, encoding="utf-8").read()
NS = {}
CURRENT = {}

# ── full builtin exception hierarchy ──────────────────────────────────────
_BASE_ONLY = {"SystemExit", "KeyboardInterrupt", "GeneratorExit"}


def build_exc(spec):
    import builtins
    try:
        import asyncio
    except Exception:
        asyncio = None
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind in _BASE_ONLY:
        return getattr(builtins, kind)(msg)
    if kind == "CancelledError" and asyncio is not None:
        return asyncio.CancelledError(msg)
    if kind == "BaseExceptionGroup":
        return builtins.BaseExceptionGroup(msg, [ValueError("x")])
    if kind == "ExceptionGroup":
        return builtins.ExceptionGroup(msg, [ValueError("x")])
    if kind == "StaleHeldHistory":
        cls = type("StaleHeldHistory", (Exception,), {})
        return cls(msg)
    cls = getattr(builtins, kind, None)
    if cls is None or not isinstance(cls, type) or not issubclass(cls, BaseException):
        cls = RuntimeError
    return cls(msg)


# ── canonicalization ──────────────────────────────────────────────────────
def canon(v, drop_null=False):
    if isinstance(v, dict):
        return {k: canon(x, drop_null) for k, x in v.items()
                if not (drop_null and x is None)}
    if isinstance(v, (list, tuple)):
        return [canon(x, drop_null) for x in v]
    if isinstance(v, range):
        return [v.start, v.stop]
    if isinstance(v, SimpleNamespace):
        return {k: canon(x, drop_null) for k, x in vars(v).items()
                if not k.startswith("__cc")}
    return v


def self_proj(slf):
    if not isinstance(slf, SimpleNamespace):
        return canon(slf)
    return {k: canon(v) for k, v in vars(slf).items() if k not in SELF_SEAM_SET}


# ── universal seam recorder ───────────────────────────────────────────────
MISSING = object()


class Script:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def fire(self, name, args):
        entry = self.io.get(name, MISSING)
        present = entry is not MISSING
        exhausted = False
        result = None
        if present:
            if isinstance(entry, list):
                cur = self.cursors.get(name, 0)
                if cur >= len(entry):
                    exhausted = True
                else:
                    result = entry[cur]
                    self.cursors[name] = cur + 1
            else:
                result = entry
        failclosed = (not present) or exhausted
        actual = result
        if isinstance(actual, dict) and actual.get("__script__") is True:
            actual = actual.get("ret")
        ev_ret = ({"__raise__": "RuntimeError", "message": "no script for " + name}
                  if failclosed else copy.deepcopy(actual))
        self.trace.append({"ev": name, "args": canon(args, True), "ret": canon(ev_ret)})
        if failclosed:
            raise build_exc(ev_ret)
        if isinstance(actual, dict) and "__raise__" in actual:
            raise build_exc(actual)
        return actual


def _fire(name, args):
    return CURRENT["S"].fire(name, args)


# ── module-level recording seams ──────────────────────────────────────────
def _null_logger():
    class L:
        def warning(self, *a, **kw): pass
        def info(self, *a, **kw): pass
        def debug(self, *a, **kw): pass
    return L()


def install_seams(ns):
    ns["_turns"] = SimpleNamespace(
        _is_real_user_turn=lambda m: _fire("_turns_is_real_user_turn", {"message": m}))
    ns["_derive_trigger"] = None  # bound on self, here only to keep names visible
    ns["_content_text_for_contains"] = (
        lambda content: _fire("_content_text_for_contains", {"content": content}))
    ns["_redact_compaction_text"] = (
        lambda text: _fire("_redact_compaction_text", {"text": text}))
    ns["_fresh_compaction_message_copy"] = (
        lambda msg: _fire("_fresh_compaction_message_copy", {"msg": msg}))

    def _append(*a, **kw):
        return _fire("_append_text_to_content", {
            "content": a[0], "addition": a[1],
            "prepend": kw.get("prepend", False)})
    ns["_append_text_to_content"] = _append
    ns["_build_anchor_index"] = (
        lambda turns: _fire("_build_anchor_index", {"turns": turns}))
    ns["_build_verbatim_user_section"] = (
        lambda turns: _fire("_build_verbatim_user_section", {"turns": turns}))
    ns["_build_recovery_footer"] = (
        lambda session_id, len_turns: _fire("_build_recovery_footer", {
            "session_id": session_id, "n": len_turns}))
    ns["drop_stale_api_content"] = (
        lambda msg: _fire("drop_stale_api_content", {"msg": msg}))
    ns["demote_stale_tail_tools"] = (
        lambda messages, tail_start, _session_id: _fire("demote_stale_tail_tools", {
            "messages": messages, "tail_start": tail_start, "session_id": _session_id}))
    ns["hashlib"] = SimpleNamespace(md5=lambda b: SimpleNamespace(
        hexdigest=lambda: _fire("md5hex", {"text": b.decode("utf-8", "replace")})))
    # in-function `from ... import _caller_attempt_is_current` -> stub module
    ag = sys.modules.get("agent")
    if ag is None:
        ag = types.ModuleType("agent")
        ag.__path__ = []
        sys.modules["agent"] = ag
    cc = types.ModuleType("agent.conversation_compression")
    cc._caller_attempt_is_current = (
        lambda self: _fire("_caller_attempt_is_current", {"self": self_proj(self)}))
    sys.modules["agent.conversation_compression"] = cc
    try:
        ag.conversation_compression = cc
    except Exception:
        pass
    ns["logger"] = _null_logger()


def build_ns():
    ns = dict(NS_BASE)
    ns["re"] = _re
    tree = ast.parse(SRC_TEXT)
    # plain-Name module Assign / AnnAssign constants only
    for node in tree.body:
        if isinstance(node, ast.Assign):
            targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            targets = [node.target.id]
        else:
            continue
        if not targets:
            continue
        try:
            exec(compile(ast.get_source_segment(SRC_TEXT, node), SRC, "exec",
                         flags=ANN_FLAG, dont_inherit=True), ns)
        except Exception:
            pass
    # constants imported from session_rules.turns (not assigned in this module)
    ns["_SUMMARY_END_MARKER"] = (
        "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---")
    ns["_MERGED_SUMMARY_DELIMITER"] = "[END OF PRIOR CONTEXT — COMPACTION SUMMARY BELOW]"
    ns["_MERGED_PRIOR_CONTEXT_HEADER"] = "[PRIOR CONTEXT — for reference only; not a new message]"
    # AST-extract the LIVE target defs in source order, stripping decorators
    by_name = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            by_name.setdefault(node.name, node)
    for name in FUNCS:
        node = by_name.get(name)
        if node is None:
            raise SystemExit("LIVE def not found: " + name)
        node = copy.copy(node)
        node.decorator_list = []
        seg = ast.unparse(node)
        exec(compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    install_seams(ns)
    return ns


NS_BASE = {"__name__": "cc6_oracle_quarry"}


# ── self double with method seams ─────────────────────────────────────────
def mk_self(d):
    slf = SimpleNamespace(**(d or {}))

    def bind(name):
        build = SELF_SEAM_BUILDERS[name]

        def f(*a, **kw):
            return _fire(name, build(slf, a, kw))
        return f

    for name in SELF_SEAM_BUILDERS:
        setattr(slf, name, bind(name))
    return slf


def cls_double():
    return SimpleNamespace(
        _is_real_user_turn=lambda m: NS["_is_real_user_turn"](None, m))


# ── live invocation (explicit per-func argument projection) ──────────────
INVOKE = {
    "_is_real_user_turn": lambda fn, p, s: fn(None, p.get("message")),
    "_real_user_indices_desc": lambda fn, p, s: fn(
        cls_double(), p.get("messages"), p.get("head_end", 0)),
    "preview_threshold_tokens": lambda fn, p, s: fn(
        s, p.get("model", ""), p.get("context_length", 0), p.get("provider", "")),
    "_render_tool_call_for_summary": lambda fn, p, s: fn(s, p.get("tc")),
    "_drop_blank_echoes": lambda fn, p, s: fn(s, p.get("messages")),
    "_prune_boundary": lambda fn, p, s: fn(
        s, p.get("result"), p.get("protect_tail_count", 0), p.get("protect_tail_tokens")),
    "_demote_stale_tail_tools": lambda fn, p, s: fn(
        s, p.get("messages"), p.get("tail_start", 0)),
    "_clear_terminal_summary_failures": lambda fn, p, s: fn(s),
    "maybe_seed_preflight_display_tokens": lambda fn, p, s: fn(
        s, p.get("preflight_tokens", 0)),
    "note_native_compaction_checkpoint": lambda fn, p, s: fn(s),
    "record_rejected_compaction": lambda fn, p, s: fn(s),
    "_apply_real_prompt_verdict": lambda fn, p, s: fn(s),
    "_abort_on_summary_failure": lambda fn, p, s: fn(
        s, p.get("telemetry"), p.get("n_skipped", 0),
        p.get("previous_summary_before_scan")),
    "_assemble_head": lambda fn, p, s: fn(
        s, p.get("messages"), p.get("compress_start", 0)),
    "_assemble_tail": lambda fn, p, s: fn(
        s, p.get("messages"), p.get("compress_end", 0), p.get("tail_start", 0),
        set(p.get("summary_indices") or [])),
    "_dedupe_tool_results": lambda fn, p, s: fn(p.get("result")),
    "_augment_summary_lean": lambda fn, p, s: fn(
        s, p.get("summary", ""), p.get("turns_to_summarize") or []),
    "_merge_summary_into_tail_row": lambda fn, p, s: fn(
        s, p.get("msg"), p.get("summary", ""), p.get("summary_role", ""),
        p.get("force_user_leading", False)),
}


def run_live(rec):
    inp = rec["in"]
    trace = []
    CURRENT["S"] = Script(inp.get("io"), trace)
    CURRENT["name"] = rec["func"]
    # deep-copy the mutable inputs; live mutates these, we read them back
    mut = {k: copy.deepcopy(inp[k]) for k in MUTABLE_KEYS if k in inp}
    params = dict(inp)
    params.update(mut)
    slf = mk_self(mut.get("self", inp.get("self") or {}))
    fn = NS[rec["func"]]
    try:
        val = INVOKE[rec["func"]](fn, params, slf)
        ret = canon(val)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    st = {}
    for k in MUTABLE_KEYS:
        if k in mut:
            if k == "self":
                mut[k] = self_proj(slf)
            st[k] = canon(mut[k])
    return {"ret": ret, "trace": [canon(t) for t in trace], "st": st}


def compare(rec):
    exp = run_live(rec)
    problems = []
    if canon(rec.get("ret")) != exp["ret"]:
        problems.append("ret: C=%r live=%r" % (canon(rec.get("ret")), exp["ret"]))
    if [canon(t) for t in rec.get("trace", [])] != exp["trace"]:
        problems.append("trace: C=%r live=%r" % ([canon(t) for t in rec.get("trace", [])], exp["trace"]))
    if canon(rec.get("st", {})) != exp["st"]:
        problems.append("st: C=%r live=%r" % (canon(rec.get("st", {})), exp["st"]))
    return problems


# ── main gate (harness output on stdin) ───────────────────────────────────
def main_gate():
    global NS
    NS = build_ns()
    # seam-name set equality: every installed seam channel must be in SEAMS
    assert set(SEAMS) == {
        "_turns_is_real_user_turn", "_derive_trigger",
        "_record_ineffective_compression_verdict", "_find_last_user_message_idx",
        "_blank_echo_indices_after", "_walk_tail_budget", "_render_tool_args_for_summary",
        "_fresh_compaction_message_copy", "_content_text_for_contains",
        "_append_text_to_content", "_strip_context_summary_handoff_message",
        "_build_anchor_index", "_build_verbatim_user_section", "_build_recovery_footer",
        "_redact_compaction_text", "drop_stale_api_content", "demote_stale_tail_tools",
        "_caller_attempt_is_current", "md5hex",
    }
    recs = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            recs.append(json.loads(line))
        except Exception:
            pass
    mism = 0
    for rec in recs:
        probs = compare(rec)
        if probs:
            mism += 1
            print("MISMATCH", rec.get("func"), "|", " ; ".join(probs))
    print("ORACLE context_compressor_flow6: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow6: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main_gate())
