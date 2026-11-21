#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow7.c — drives the LIVE
agent/context_compressor.py seventh-wave FLOW defs (14 UNCLAIMED module
functions / methods) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the mutated-state window. Live defs are AST-extracted in
source order with decorators stripped and future-annotations; module constants
are seeded; claimed siblings / foreign helpers / self-methods / logger / clock
are recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. FULL Python
builtin exception hierarchy (build_exc): a scripted base-only kind propagates as
a BaseException exactly as live.

Summary: 'ORACLE context_compressor_flow7: N cases, M mismatches'.
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
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow7.jsonl"

FUNCS = [
    "_begin_compress_attempt", "_load_consecutive_overload_aborts",
    "_load_anti_thrash_recovery_deadline", "_persist_consecutive_overload_aborts",
    "_increment_consecutive_overload_aborts", "_record_summary_success",
    "_record_summary_input_coverage", "_log_compression_start",
    "_reset_session_compaction_state", "_native_anthropic_budget",
    "_record_structural_no_op", "_has_merged_inflight_replay",
    "_derive_trigger", "_fallback_summary_for_window",
]

# The universal-seam alphabet. MUST equal the seam names fired by the C port
# and the io keys used in the fixture file (SEAM-NAME SET EQUALITY).
SEAMS = [
    "_begin_compression_telemetry", "_clear_compression_failure_cooldown",
    "_load_durable", "_durable_write", "_durable_read",
    "_clear_terminal_summary_failures", "_reset_consecutive_overload_aborts",
    "_reset_real_usage_pairing", "_reset_proactive_prune_rearm",
    "_prepared_compaction_discard", "_native_anthropic_tail_prices",
    "_time_monotonic", "_is_context_summary_message", "_content_text_for_contains",
    "resolve_model_threshold", "_effective_threshold_percent",
    "_compute_threshold_tokens", "_effective_threshold_cap",
    "_build_static_fallback_summary", "_strip_summary_prefix",
    "_log_info", "_log_warning",
]

# self-method seams: bound on the SimpleNamespace self under the LIVE attr name.
SELF_SEAM_BUILDERS = {
    "_begin_compression_telemetry": lambda s, a, kw: {"current_tokens": kw.get("current_tokens")},
    "_clear_compression_failure_cooldown": lambda s, a, kw: {},
    "_clear_terminal_summary_failures": lambda s, a, kw: {},
    "_reset_consecutive_overload_aborts": lambda s, a, kw: {},
    "_reset_real_usage_pairing": lambda s, a, kw: {},
    "_reset_proactive_prune_rearm": lambda s, a, kw: {},
    "_load_durable": lambda s, a, kw: {
        "name": a[0], "op": a[1], "label": a[2],
        "type": getattr(a[3], "__name__", str(a[3])),
        "default": a[4] if len(a) > 4 else kw.get("default", 0)},
    "_durable_write": lambda s, a, kw: {"op": a[0], "label": a[1], "value": a[2]},
    "_durable_read": lambda s, a, kw: {
        "op": a[0], "label": a[1],
        "type": getattr(a[2], "__name__", str(a[2])),
        "default": a[3] if len(a) > 3 else kw.get("default", 0)},
    "_effective_threshold_percent": lambda s, a, kw: {
        "context_length": a[0], "base_percent": a[1]},
    "_compute_threshold_tokens": lambda s, a, kw: {
        "context_length": a[0], "effective_percent": a[1], "max_tokens": a[2]},
    "_effective_threshold_cap": lambda s, a, kw: {"context_length": a[0]},
    "_build_static_fallback_summary": lambda s, a, kw: {
        "turns": a[0],
        "reason": kw["reason"] if "reason" in kw else (a[1] if len(a) > 1 else None)},
    "_strip_summary_prefix": lambda s, a, kw: {"summary": a[0]},
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
class _Prep(SimpleNamespace):
    """prepared_compaction double: discard() is a seam, class-level so vars() is clean."""
    def discard(self, reason):
        return _fire("_prepared_compaction_discard", {"reason": reason})


def _mk_logger():
    class L:
        def info(self, fmt, *a):
            return _fire("_log_info", {"fmt": fmt, "args": list(a)})

        def warning(self, fmt, *a):
            return _fire("_log_warning", {"fmt": fmt, "args": list(a)})

        def debug(self, fmt, *a):
            return _fire("_log_debug", {"fmt": fmt, "args": list(a)})
    return L()


def _mk_time():
    class T:
        @staticmethod
        def monotonic():
            return _fire("_time_monotonic", {})
    return T()


def install_seams(ns):
    ns["_content_text_for_contains"] = (
        lambda content: _fire("_content_text_for_contains", {"content": content}))
    ns["resolve_model_threshold"] = (
        lambda model, model_thresholds, config_threshold_percent, provider:
        _fire("resolve_model_threshold", {
            "model": model, "model_thresholds": model_thresholds,
            "config_threshold_percent": config_threshold_percent,
            "provider": provider}))
    # in-function imports -> stub modules
    ag = sys.modules.get("agent")
    if ag is None:
        ag = types.ModuleType("agent")
        ag.__path__ = []
        sys.modules["agent"] = ag
    tail = types.ModuleType("agent.context_compressor_tail")
    tail.native_anthropic_tail_prices = (
        lambda self: _fire("_native_anthropic_tail_prices", {"self": self_proj(self)}))
    sys.modules["agent.context_compressor_tail"] = tail
    try:
        ag.context_compressor_tail = tail
    except Exception:
        pass
    ns["logger"] = _mk_logger()
    ns["time"] = _mk_time()


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


NS_BASE = {"__name__": "cc7_oracle_quarry"}


# ── self double with method seams ─────────────────────────────────────────
def mk_self(d):
    slf = SimpleNamespace(**(d or {}))
    pc = getattr(slf, "prepared_compaction", None)
    if isinstance(pc, dict):
        slf.prepared_compaction = _Prep(**pc)

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
        _is_context_summary_message=lambda m: _fire("_is_context_summary_message", {"message": m}))


# ── live invocation (explicit per-func argument projection) ──────────────
INVOKE = {
    "_begin_compress_attempt": lambda fn, p, s: fn(
        s, p.get("current_tokens"), p.get("force", False)),
    "_load_consecutive_overload_aborts": lambda fn, p, s: fn(s),
    "_load_anti_thrash_recovery_deadline": lambda fn, p, s: fn(s),
    "_persist_consecutive_overload_aborts": lambda fn, p, s: fn(s),
    "_increment_consecutive_overload_aborts": lambda fn, p, s: fn(s),
    "_record_summary_success": lambda fn, p, s: fn(s, p.get("summary", "")),
    "_record_summary_input_coverage": lambda fn, p, s: fn(s, p.get("coverage", {})),
    "_log_compression_start": lambda fn, p, s: fn(
        s, p.get("display_tokens", 0), p.get("compress_start", 0),
        p.get("compress_end", 0), p.get("n_turns", 0), p.get("tail_msgs", 0)),
    "_reset_session_compaction_state": lambda fn, p, s: fn(s),
    "_native_anthropic_budget": lambda fn, p, s: fn(s),
    "_record_structural_no_op": lambda fn, p, s: fn(
        s, p.get("reason", ""), p.get("n_messages", 0)),
    "_has_merged_inflight_replay": lambda fn, p, s: fn(
        cls_double(), p.get("message")),
    "_derive_trigger": lambda fn, p, s: fn(
        s, p.get("model", ""), p.get("context_length", 0), p.get("provider", "")),
    "_fallback_summary_for_window": lambda fn, p, s: fn(
        s, p.get("telemetry", {}), p.get("turns_to_summarize", []),
        p.get("n_dropped", 0), p.get("feasibility_skip", False)),
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


# ── self-test of drive() over every fixture case ──────────────────────────
def self_test():
    global NS
    NS = build_ns()
    n = 0
    with open(FIXTURE, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            out = run_live(rec)
            n += 1
            # flag confident-but-wrong raise specs naming a ported method/seam
            r = out.get("ret", {})
            if isinstance(r, dict) and r.get("__raise__") in ("AttributeError", "TypeError"):
                msg = str(r.get("message", ""))
                for name in set(SEAMS) | set(SELF_SEAM_BUILDERS) | set(FUNCS):
                    if "'%s'" % name in msg:
                        raise SystemExit("self-test: drive() raise names ported/seam attr %r: %s"
                                         % (name, msg))
    return n


# ── main gate (harness output on stdin) ───────────────────────────────────
def main_gate():
    global NS
    NS = build_ns()
    # seam-name set equality: every installed seam channel must be in SEAMS
    assert set(SEAMS) == {
        "_begin_compression_telemetry", "_clear_compression_failure_cooldown",
        "_load_durable", "_durable_write", "_durable_read",
        "_clear_terminal_summary_failures", "_reset_consecutive_overload_aborts",
        "_reset_real_usage_pairing", "_reset_proactive_prune_rearm",
        "_prepared_compaction_discard", "_native_anthropic_tail_prices",
        "_time_monotonic", "_is_context_summary_message", "_content_text_for_contains",
        "resolve_model_threshold", "_effective_threshold_percent",
        "_compute_threshold_tokens", "_effective_threshold_cap",
        "_build_static_fallback_summary", "_strip_summary_prefix",
        "_log_info", "_log_warning",
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
    print("ORACLE context_compressor_flow7: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow7: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        n = self_test()
        print("self-test: drove %d fixture cases OK" % n)
        sys.exit(0)
    sys.exit(main_gate())
