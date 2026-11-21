#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow8.c — drives the LIVE
agent/context_compressor.py eighth-wave FLOW defs (12 UNCLAIMED module
functions / methods) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the mutated-state window. Live defs are AST-extracted in
source order with decorators stripped and future-annotations; module constants
are seeded, claimed siblings / foreign helpers / self-methods / logger / clock
are recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. FULL Python
builtin exception hierarchy (build_exc): a scripted base-only kind propagates as
a BaseException exactly as live.

Static prompt constants are normalized to shared sentinel strings on both
sides; the control flow / ordering / branching under test is the LIVE body.

Summary: 'ORACLE context_compressor_flow8: N cases, M mismatches'.
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
CSRC = _REPO / "src" / "agent" / "context_compressor_flow8.c"
ANN_FLAG = __future__.annotations.compiler_flag
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow8.jsonl"

FUNCS = [
    "_reset_micro_compact_cursor_state", "_reset_real_usage_pairing",
    "_reset_consecutive_overload_aborts", "_reset_proactive_prune_rearm",
    "_temporal_anchoring_rule", "_structural_no_op_result",
    "_warn_reclamation_no_op", "_spared_pending_tool_round",
    "_demote_tool_result_at", "_serialize_records_for_summary",
    "_summary_template_sections", "_build_summary_prompt",
]

# The universal-seam alphabet. MUST equal the seam names fired by the C port
# and the io keys used in the fixture file (SEAM-NAME SET EQUALITY).
SEAMS = [
    "_persist_consecutive_overload_aborts", "_today_for_prompt",
    "_record_structural_no_op", "_billed_basis_over_threshold", "_log_warning",
    "_pending_tool_round", "_effective_input_window", "_estimate_msg_budget_tokens",
    "_strip_images_from_tool_msg", "_is_summary_stub", "_json_dict",
    "_summarize_tool_result", "_summary_part_text", "_redact_compaction_text",
    "_media_directive_sub", "_strip_think_blocks", "_elide_middle",
    "_render_tool_call_for_summary", "_temporal_anchoring_rule",
    "_section_instructions", "_memory_provider_section", "_bound_summary_input",
    "_summary_template_sections",
]

# Self/cls-method seams: bound on the SimpleNamespace self/cls under the LIVE
# attr name. Module seams are installed directly in the namespace below.
SELF_SEAM_BUILDERS = {
    "_persist_consecutive_overload_aborts": lambda s, a, kw: {},
    "_record_structural_no_op": lambda s, a, kw: {"reason": a[0], "n_messages": a[1]},
    "_billed_basis_over_threshold": lambda s, a, kw: {"current_tokens": a[0]},
    "_effective_input_window": lambda s, a, kw: {"context_length": a[0], "max_tokens": a[1]},
    "_render_tool_call_for_summary": lambda s, a, kw: {"tool_call": a[0]},
    "_bound_summary_input": lambda s, a, kw: {"summary": a[0]},
    "_temporal_anchoring_rule": lambda s, a, kw: {},
    "_summary_template_sections": lambda s, a, kw: {
        "section": a[0], "summary_budget": a[1], "session_log_section": a[2]},
}
SELF_SEAM_SET = set(SELF_SEAM_BUILDERS)

# Sentinel constants shared verbatim with the C port (see CC8_SENT_* there).
SENTINELS = {
    "EPISTEMIC_STANCE_RULE": "<<EPISTEMIC_STANCE_RULE>>",
    "HISTORICAL_TASK_HEADING": "<<HISTORICAL_TASK_HEADING>>",
    "UNVERIFIED_HYPOTHESES_SECTION": "<<UNVERIFIED_HYPOTHESES_SECTION>>",
    "_PRUNED_SKILLS_SECTION_HEADING": "<<PRUNED_SKILLS_SECTION_HEADING>>",
    "_LEAN_SESSION_LOG_SECTION": "<<LEAN_SESSION_LOG_SECTION>>",
    "_LEAN_SESSION_LOG_BUDGET_TOKENS": 4000,
}

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
class _SectionInstructions(dict):
    """_SECTION_INSTRUCTIONS double: __getitem__ fires a seam."""
    def __getitem__(self, key):
        return _fire("_section_instructions", {"has_user_turn": key})


class _MediaRe:
    """_MEDIA_DIRECTIVE_RE double: .sub fires a seam."""
    def sub(self, repl, content):
        return _fire("_media_directive_sub", {"repl": repl, "content": content})


def _mk_logger():
    class L:
        def info(self, fmt, *a):
            return _fire("_log_info", {"fmt": fmt, "args": list(a)})

        def warning(self, fmt, *a):
            return _fire("_log_warning", {"fmt": fmt, "args": list(a)})

        def debug(self, fmt, *a):
            return _fire("_log_debug", {"fmt": fmt, "args": list(a)})
    return L()


def install_seams(ns):
    # module seams (positional -> arg dict matching the C port exactly)
    ns["_today_for_prompt"] = lambda: _fire("_today_for_prompt", {})
    ns["_pending_tool_round"] = (
        lambda messages: range(*_fire("_pending_tool_round", {"messages": messages})))
    ns["_estimate_msg_budget_tokens"] = (
        lambda msg, charge_stale_thinking=True: _fire("_estimate_msg_budget_tokens", {
            "message": msg, "charge_stale_thinking": charge_stale_thinking}))
    ns["_strip_images_from_tool_msg"] = (
        lambda msg: _fire("_strip_images_from_tool_msg", {"msg": msg}))
    ns["_is_summary_stub"] = (
        lambda content: _fire("_is_summary_stub", {"content": content}))
    ns["_json_dict"] = (
        lambda data, default=None: _fire("_json_dict", {"text": data}))
    ns["_summarize_tool_result"] = (
        lambda tool_name, tool_args, tool_content: _fire("_summarize_tool_result", {
            "tool_name": tool_name, "tool_args": tool_args, "content": tool_content}))
    ns["_summary_part_text"] = (
        lambda part: _fire("_summary_part_text", {"part": part}))
    ns["_redact_compaction_text"] = (
        lambda text: _fire("_redact_compaction_text", {"content": text}))
    ns["_elide_middle"] = (
        lambda text, head, tail: _fire("_elide_middle", {
            "content": text, "head": head, "tail": tail}))
    ns["_memory_provider_section"] = (
        lambda memory_context: _fire("_memory_provider_section", {
            "memory_context": memory_context}))

    # pattern/dict doubles
    ns["_MEDIA_DIRECTIVE_RE"] = _MediaRe()
    ns["_SECTION_INSTRUCTIONS"] = _SectionInstructions()

    # lazy in-function import -> stub module
    ag = sys.modules.get("agent")
    if ag is None:
        ag = types.ModuleType("agent")
        ag.__path__ = []
        sys.modules["agent"] = ag
    ahelpers = types.ModuleType("agent.agent_runtime_helpers")
    ahelpers.strip_think_blocks = (
        lambda owner, content: _fire("_strip_think_blocks", {"content": content}))
    sys.modules["agent.agent_runtime_helpers"] = ahelpers
    try:
        ag.agent_runtime_helpers = ahelpers
    except Exception:
        pass

    ns["logger"] = _mk_logger()

    # normalized static constants (identical literals in the C port)
    for k, v in SENTINELS.items():
        ns[k] = v


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


NS_BASE = {"__name__": "cc8_oracle_quarry"}


# ── self double with method seams ─────────────────────────────────────────
def mk_self(d):
    slf = SimpleNamespace(**(d or {}))

    # LIVE stores the rearm dedup key as a tuple; JSON fixtures carry a list.
    # Normalize so equality against LIVE's freshly-built tuple matches.
    wb = getattr(slf, "_last_reclaim_block_warn", None)
    if isinstance(wb, list) and len(wb) == 2:
        slf._last_reclaim_block_warn = (wb[0], wb[1])

    def bind(name):
        build = SELF_SEAM_BUILDERS[name]

        def f(*a, **kw):
            return _fire(name, build(slf, a, kw))
        return f

    for name in SELF_SEAM_BUILDERS:
        setattr(slf, name, bind(name))
    return slf


def cls_double():
    slf = SimpleNamespace()

    def bind(name):
        build = SELF_SEAM_BUILDERS[name]

        def f(*a, **kw):
            return _fire(name, build(slf, a, kw))
        return f

    for name in SELF_SEAM_BUILDERS:
        setattr(slf, name, bind(name))
    return slf


# ── live invocation (explicit per-func argument projection) ──────────────
INVOKE = {
    "_reset_micro_compact_cursor_state": lambda fn, p, s: fn(s),
    "_reset_real_usage_pairing": lambda fn, p, s: fn(s),
    "_reset_consecutive_overload_aborts": lambda fn, p, s: fn(s),
    "_reset_proactive_prune_rearm": lambda fn, p, s: fn(s),
    "_temporal_anchoring_rule": lambda fn, p, s: fn(),
    "_structural_no_op_result": lambda fn, p, s: fn(
        s, p.get("telemetry", {}), p.get("failure_class", ""),
        p.get("reason", ""), p.get("n_messages", 0)),
    "_warn_reclamation_no_op": lambda fn, p, s: fn(
        s, p.get("reason", ""), p.get("current_tokens"), p.get("before")),
    "_spared_pending_tool_round": lambda fn, p, s: fn(s, p.get("messages", [])),
    "_demote_tool_result_at": lambda fn, p, s: fn(
        p.get("result", []), p.get("idx", 0), p.get("call_id_to_tool", {}),
        p.get("min_prune_chars", 0), p.get("protected_skills")),
    "_serialize_records_for_summary": lambda fn, p, s: fn(s, p.get("turns", [])),
    "_summary_template_sections": lambda fn, p, s: fn(
        cls_double(), p.get("_section", {}), p.get("summary_budget", 0),
        p.get("_session_log_section", "")),
    "_build_summary_prompt": lambda fn, p, s: fn(
        s, p.get("content_to_summarize", ""), p.get("summary_budget", 0),
        p.get("focus_topic"), p.get("memory_context", ""),
        p.get("has_user_turn", False)),
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
        ret = [val.start, val.stop] if isinstance(val, range) else canon(val)
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


# ── seam-name set equality (static C scan + fixture/io audit) ────────────
def check_seam_sets():
    csrc = CSRC.read_text(encoding="utf-8")
    cseams = set(_re.findall(r'cc8_call\(io,\s*"([^"]+)"', csrc))
    want = set(SEAMS)
    if cseams != want:
        raise SystemExit("SEAM-NAME SET EQUALITY failed: C-only=%r oracle-only=%r"
                         % (sorted(cseams - want), sorted(want - cseams)))
    return cseams


# ── self-test of drive() over every fixture case ─────────────────────────
def self_test():
    global NS
    NS = build_ns()
    check_seam_sets()
    n = 0
    fired = set()
    with open(FIXTURE, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            io = rec.get("in", {}).get("io", {}) or {}
            for k in io:
                if k not in SEAMS:
                    raise SystemExit("self-test: fixture io key %r not in SEAMS" % k)
            for t in run_live(rec).get("trace", []):
                fired.add(t.get("ev"))
            n += 1
            r = rec.get("ret", {})
            if isinstance(r, dict) and r.get("__raise__") in ("AttributeError", "TypeError"):
                msg = str(r.get("message", ""))
                for name in set(SEAMS) | set(SELF_SEAM_BUILDERS) | set(FUNCS):
                    if "'%s'" % name in msg:
                        raise SystemExit("self-test: drive() raise names ported/seam attr %r: %s"
                                         % (name, msg))
    return n, fired


# ── main gate (harness output on stdin) ───────────────────────────────────
def main_gate():
    global NS
    NS = build_ns()
    check_seam_sets()
    assert set(SEAMS) == {
        "_persist_consecutive_overload_aborts", "_today_for_prompt",
        "_record_structural_no_op", "_billed_basis_over_threshold", "_log_warning",
        "_pending_tool_round", "_effective_input_window", "_estimate_msg_budget_tokens",
        "_strip_images_from_tool_msg", "_is_summary_stub", "_json_dict",
        "_summarize_tool_result", "_summary_part_text", "_redact_compaction_text",
        "_media_directive_sub", "_strip_think_blocks", "_elide_middle",
        "_render_tool_call_for_summary", "_temporal_anchoring_rule",
        "_section_instructions", "_memory_provider_section", "_bound_summary_input",
        "_summary_template_sections",
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
    print("ORACLE context_compressor_flow8: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow8: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        n, fired = self_test()
        print("self-test: drove %d fixture cases OK; fired %d/%d seams"
              % (n, len(fired), len(SEAMS)))
        sys.exit(0)
    sys.exit(main_gate())
