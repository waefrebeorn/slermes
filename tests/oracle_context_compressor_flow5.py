#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow5.c — drives the LIVE
agent/context_compressor.py fifth-wave FLOW defs (18 UNCLAIMED module
functions / methods) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted in source
order with future-annotations; module constants are seeded; claimed siblings /
foreign helpers / regex primitives are recording seams fed the SAME scripts the
C harness feeds.

Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. FULL Python
builtin exception hierarchy (build_exc): a scripted base-only kind
(SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseExceptionGroup)
propagates as a BaseException exactly as live; every other builtin resolves
from `builtins`. Unknown custom names -> RuntimeError (Exception descendant).

Summary: 'ORACLE context_compressor_flow5: N cases, M mismatches'.
"""
import ast
import copy
import inspect
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
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow5.jsonl"

FUNCS = [
    "_is_image_part", "_content_has_images", "_replace_image_parts",
    "_strip_images_from_content", "_image_part_label", "_str_arg",
    "_tool_content_has_images", "_strip_images_from_tool_msg",
    "_filter_legacy_clarify_answers", "_memory_provider_section",
    "resolve_model_threshold", "_compact_fallback_turn",
    "is_user_originated_turn", "fallback_reason", "_effective_threshold_cap",
    "_tripped", "_tail_soft_ceiling", "_render_tool_args_for_summary",
]

# The universal-seam alphabet. MUST equal the seam names fired by the C port
# and the io keys used in the fixture file (SEAM-NAME SET EQUALITY).
SEAMS = [
    "_tool_result_parts", "_rewritten", "_clarify_headless_fullmatch",
    "_is_clarify_non_response", "_sanitize_memory_context", "_json_dumps",
    "_model_threshold_key_rank", "_redact_compaction_text",
    "_content_text_for_contains", "_gh_token_sub1", "_ws_collapse",
    "_elide", "_gh_token_sub2", "_user_originated_turn_view",
]

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


_MATCH = object()


class _ReShim:
    """Recording shim for a compiled-regex primitive used by a live def."""

    def __init__(self, seam):
        self.seam = seam

    def fullmatch(self, text):
        r = _fire(self.seam, {"text": text})
        return _MATCH if r else None

    def sub(self, repl, string, count=0):
        return _fire(self.seam, {"text": string})


class _ReSubShim:
    """Recording shim for module-level re.sub call sites (pattern -> seam)."""
    SUBS = {
        r"\bgh[pousr]_[A-Za-z0-9_]{8,}\b": "_gh_token_sub1",
        r"\s+": "_ws_collapse",
        r"\bgh[pousr]_[A-Za-z0-9_.-]+": "_gh_token_sub2",
    }

    def sub(self, pattern, repl, string, count=0, flags=0):
        seam = self.SUBS.get(pattern)
        if seam is None:
            return string
        return _fire(seam, {"text": string})


class _FakeJson:
    @staticmethod
    def dumps(value, ensure_ascii=True):
        return _fire("_json_dumps", {"value": value, "ensure_ascii": ensure_ascii})


def install_seams(ns):
    """Recording seams for claimed siblings / imported helpers / regex primitives."""
    ns["_content_text_for_contains"] = (
        lambda content: _fire("_content_text_for_contains", {"content": content}))
    ns["_redact_compaction_text"] = (
        lambda text: _fire("_redact_compaction_text", {"text": text}))
    ns["_rewritten"] = (
        lambda msg, content: _fire("_rewritten", {"msg": msg, "content": content}))
    ns["_tool_result_parts"] = (
        lambda content: _fire("_tool_result_parts", {"content": content}))
    ns["_CLARIFY_HEADLESS_NOTICE"] = _ReShim("_clarify_headless_fullmatch")
    ns["_is_clarify_non_response"] = (
        lambda item: _fire("_is_clarify_non_response", {"item": item}))
    ns["sanitize_memory_context"] = (
        lambda memory_context: _fire("_sanitize_memory_context", {"memory_context": memory_context}))
    ns["json"] = _FakeJson()
    ns["_model_threshold_key_rank"] = (
        lambda key, model, provider: _fire("_model_threshold_key_rank",
                                          {"key": key, "model": model, "provider": provider}))
    ns["re"] = _ReSubShim()
    ns["elide"] = (
        lambda text, max_chars: _fire("_elide", {"text": text, "max_chars": max_chars}))
    ns["user_originated_turn_view"] = (
        lambda message: _fire("_user_originated_turn_view", {"message": message}))


def build_ns():
    ns = dict(NS_BASE)
    ns["re"] = _re
    tree = ast.parse(SRC_TEXT)
    # plain-Name module Assign / AnnAssign constants only (no imports executed)
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
    # constant imported from session_rules.turns (not assigned in this module)
    ns["_IMAGE_PART_TYPES"] = {"image", "image_url", "input_image"}
    # AST-extract the LIVE target defs + methods in source order
    by_name = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            by_name.setdefault(node.name, node)
    for name in FUNCS:
        node = by_name.get(name)
        if node is None:
            raise SystemExit("LIVE def not found: " + name)
        seg = ast.unparse(node)  # dedents method bodies
        exec(compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    install_seams(ns)
    return ns


NS_BASE = {"__name__": "cc5_oracle_quarry"}


# ── live invocation (explicit per-func argument projection) ──────────────
def mk_self(inp):
    return SimpleNamespace(**(inp.get("self") or {}))


def invoke_plain(fn, inp):
    name = rec_name(CURRENT)
    if name == "_str_arg":
        if "default" in inp:
            return fn(inp.get("args") or {}, inp["key"], inp["default"])
        return fn(inp.get("args") or {}, inp["key"])
    table = {
        "_is_image_part": lambda: fn(inp.get("part")),
        "_content_has_images": lambda: fn(inp.get("content")),
        "_replace_image_parts": lambda: fn(inp.get("parts"), inp.get("placeholder", "")),
        "_strip_images_from_content": lambda: fn(inp.get("content")),
        "_image_part_label": lambda: fn(inp.get("part") or {}),
        "_tool_content_has_images": lambda: fn(inp.get("content")),
        "_strip_images_from_tool_msg": lambda: fn(inp.get("msg") or {}),
        "_filter_legacy_clarify_answers": lambda: fn(inp.get("values") or []),
        "_memory_provider_section": lambda: fn(inp.get("memory_context", "")),
        "resolve_model_threshold": lambda: fn(
            inp.get("model", ""), inp.get("model_thresholds"),
            inp.get("default", 0.0), inp.get("provider", "")),
        "_compact_fallback_turn": lambda: fn(inp.get("value")),
        "is_user_originated_turn": lambda: fn(inp.get("message")),
        "fallback_reason": lambda: fn(mk_self(inp)),
        "_effective_threshold_cap": lambda: fn(mk_self(inp), inp["context_length"]),
        "_tripped": lambda: fn(mk_self(inp)),
        "_tail_soft_ceiling": lambda: fn(mk_self(inp), inp["token_budget"]),
        "_render_tool_args_for_summary": lambda: fn(
            mk_self(inp), inp.get("arguments") or ""),
    }
    return table[name]()


def rec_name(_):
    return CURRENT["name"]


def run_live(rec):
    inp = rec["in"]
    trace = []
    CURRENT["S"] = Script(inp.get("io"), trace)
    CURRENT["name"] = rec["func"]
    fn = NS[rec["func"]]
    try:
        val = invoke_plain(fn, inp)
        ret = canon(val)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": ret, "trace": [canon(t) for t in trace], "st": {}}


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
        "_tool_result_parts", "_rewritten", "_clarify_headless_fullmatch",
        "_is_clarify_non_response", "_sanitize_memory_context", "_json_dumps",
        "_model_threshold_key_rank", "_redact_compaction_text",
        "_content_text_for_contains", "_gh_token_sub1", "_ws_collapse",
        "_elide", "_gh_token_sub2", "_user_originated_turn_view",
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
    print("ORACLE context_compressor_flow5: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow5: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main_gate())
