#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow4.c — drives the LIVE
agent/context_compressor.py fourth-wave FLOW defs (11 UNCLAIMED module functions)
under IDENTICAL scripted universal seams.

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
from `builtins` (including the py3.14 additions and the OSError/ImportError/
Warning families). Unknown custom names -> RuntimeError (Exception descendant).

`--shadow` mode reads the fixture file and diffs a literal Python transcription
of the C port against the LIVE defs (shadow-diff 0 expected).

Summary: 'ORACLE context_compressor_flow4: N cases, M mismatches'.
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
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow4.jsonl"

FUNCS = [
    "_effective_input_window", "_model_threshold_key_rank",
    "_is_text_only_content", "_is_clarify_non_response",
    "_restated_request_text", "_authored_request_text", "_pending_tool_round",
    "_image_payload", "_sum_skill_view", "_collect_paths_from_jsonish",
    "_sum_clarify",
]
# real (non-seamed) helpers the live defs call — extract from the quarry
HELPERS = ["_dedupe_append", "_collect_path_mentions"]

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

    def findall(self, text):
        return _fire(self.seam, {"text": text})


class _FakeJson:
    @staticmethod
    def dumps(value, ensure_ascii=True):
        return _fire("_json_dumps", {"value": value, "ensure_ascii": ensure_ascii})


def install_seams(ns):
    """Recording seams for claimed siblings / imported helpers."""
    ns["_content_text_for_contains"] = (
        lambda content: _fire("_content_text_for_contains", {"content": content}))
    ns["_result_failure_suffix"] = (
        lambda content: _fire("_result_failure_suffix", {"content": content}))
    ns["_skill_view_failed"] = (
        lambda content: _fire("_skill_view_failed", {"content": content}))
    ns["_is_summary_stub"] = (
        lambda content: _fire("_is_summary_stub", {"content": content}))
    ns["_skill_pruned_marker"] = (
        lambda skill_name: _fire("_skill_pruned_marker", {"skill": skill_name}))
    ns["_tool_result_parts"] = (
        lambda content: _fire("_tool_result_parts", {"content": content}))
    ns["_is_image_part"] = (
        lambda part: _fire("_is_image_part", {"part": part}))
    ns["_filter_legacy_clarify_answers"] = (
        lambda values: _fire("_filter_legacy_clarify_answers", {"values": values}))
    ns["_json_dict"] = (
        lambda text: _fire("_json_dict", {"text": text}))
    ns["elide"] = (
        lambda text, max_chars: _fire("_elide", {"text": text, "max_chars": max_chars}))
    ns["_GATEWAY_REPLY_POINTER_RE"] = _ReShim("_gateway_reply_pointer_sub")
    ns["_CLARIFY_HEADLESS_NOTICE"] = _ReShim("_clarify_headless_fullmatch")
    ns["_PATH_MENTION_RE"] = _ReShim("_path_mention_findall")
    ns["json"] = _FakeJson()
    ns["re"] = _re


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
    # constants imported from sibling modules / declared elsewhere
    ns["STEER_DISPLAY_KIND"] = "steer"
    ns["_PRUNE_MIN_CHARS"] = 200
    # AST-extract the LIVE target defs + real helpers in source order
    by_name = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            by_name.setdefault(node.name, node)
    for name in FUNCS + HELPERS:
        node = by_name.get(name)
        if node is None:
            raise SystemExit("LIVE def not found: " + name)
        seg = ast.unparse(node)  # dedents method bodies
        exec(compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    install_seams(ns)
    return ns


NS_BASE = {"__name__": "cc4_oracle_quarry"}


def kwargs_for(fn, inp):
    sig = inspect.signature(fn)
    kw = {}
    for p in sig.parameters.values():
        if p.name in inp:
            kw[p.name] = inp[p.name]
        elif p.default is inspect.Parameter.empty:
            raise KeyError("missing required arg %s for %s" % (p.name, fn.__name__))
    return kw


def run_live(rec):
    inp = rec["in"]
    trace = []
    CURRENT["S"] = Script(inp.get("io"), trace)
    fn = NS[rec["func"]]
    try:
        val = fn(**kwargs_for(fn, inp))
        ret = [val.start, val.stop] if isinstance(val, range) else canon(val)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    st = {}
    if rec["func"] == "_collect_paths_from_jsonish":
        st = {"relevant_files": canon(inp.get("relevant_files"))}
    return {"ret": ret, "trace": [canon(t) for t in trace], "st": canon(st)}


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
    print("ORACLE context_compressor_flow4: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow4: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


# ── shadow: literal Python mirror of the C port ───────────────────────────
_WS = " \t\n\r\v\f"


def s_strip(s):
    return (s or "").strip(_WS)


def s_rstrip(s, chars):
    s = s or ""
    i = len(s)
    while i and s[i - 1] in chars:
        i -= 1
    return s[:i]


def s_py_str(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return str(int(v)) if v == int(v) else repr(v)
    return str(v)


def s_comma(n):
    return format(int(n), ",")


def s_truth(v):
    return bool(v)


def s_text_of(c):
    return c if isinstance(c, str) else ""


SHADOW_DISPATCH = {}


def shadow(name, inp):
    fn = SHADOW_DISPATCH.get(name)
    if fn is None:
        raise KeyError(name)
    return fn(inp)


def _sh_effective_input_window(inp):
    mt = inp.get("max_tokens") or 0
    ew = inp["context_length"] - mt
    return ew if ew > 0 else inp["context_length"]


def _sh_model_threshold_key_rank(inp):
    key, model, provider = inp["key"], inp["model"], inp["provider"]
    scope, sep, substr = key.partition(":")
    if not sep:
        return [len(key), 0] if key in model else None
    return [len(substr), 1] if scope.strip().lower() == provider and substr in model else None


def _sh_is_text_only_content(inp):
    content = inp.get("content")
    if isinstance(content, str):
        return True
    if not isinstance(content, list):
        return False
    for part in content:
        if isinstance(part, str):
            continue
        if not isinstance(part, dict):
            return False
        if part.get("type") not in {"text", "input_text"}:
            return False
        if not isinstance(part.get("text"), str):
            return False
    return True


_TEXTS = ("The user cancelled. Use your best judgement to proceed.",)
_PREFIXES = ("The user did not provide a response", "[user did not respond",
             "[clarify prompt could not be delivered", "[oneshot mode:")


def _sh_is_clarify_non_response(inp):
    item = inp.get("item")
    if not isinstance(item, str):
        return False
    text = item.strip()
    if text in _TEXTS or text.startswith(_PREFIXES):
        return True
    return bool(_fire("_clarify_headless_fullmatch", {"text": text}))


def _sh_restated_request_text(inp):
    text = s_strip(_fire("_content_text_for_contains", {"content": inp.get("content")}))
    hdr = ("[STILL IN PROGRESS — this is the active request, restated after the "
           "compaction boundary because it was not finished yet. Continue it; do not "
           "start over.]")
    if hdr in text:
        text = s_strip(text.rsplit(hdr, 1)[1])
    return text


def _sh_authored_request_text(inp):
    text = _sh_restated_request_text(inp)
    return s_strip(_fire("_gateway_reply_pointer_sub", {"text": text}))


def _sh_pending_tool_round(inp):
    messages = inp.get("messages") or []
    end = len(messages)
    while end and (messages[end - 1].get("display_kind") if isinstance(messages[end - 1], dict) else None) == "steer":
        end -= 1
    start = end
    while start > 0 and (messages[start - 1].get("role") if isinstance(messages[start - 1], dict) else None) == "tool":
        start -= 1
    return [start, end]


def _sh_image_payload(inp):
    parts = _fire("_tool_result_parts", {"content": (inp.get("msg") or {}).get("content")})
    if not isinstance(parts, list):
        return []
    sources = []
    for p in parts:
        if not _fire("_is_image_part", {"part": p}):
            continue
        p = p if isinstance(p, dict) else {}
        iu, src = p.get("image_url"), p.get("source")
        data = (iu.get("url") if isinstance(iu, dict) else iu) or \
               (src.get("data") if isinstance(src, dict) else None)
        sources.append(data if isinstance(data, str) else "")
    return sources


def _sh_sum_skill_view(inp):
    args = inp.get("args") or {}
    skill = args.get("name", "?")
    failed = s_text_of(_fire("_result_failure_suffix", {"content": inp.get("content")}))
    if not failed and _fire("_skill_view_failed", {"content": inp.get("content")}):
        failed = " FAILED"
    stub = _fire("_is_summary_stub", {"content": inp.get("content")})
    prefix = "[SKILL_PRUNED:"
    content = inp.get("content")
    keep = ((prefix in content) if isinstance(content, str) else False) if stub else not bool(failed)
    marker = ""
    if skill != "?" and keep:
        marker = " " + s_text_of(_fire("_skill_pruned_marker", {"skill": s_py_str(skill)}))
    stub_txt = "[skill_view] name=%s%s (%s chars)" % (s_py_str(skill), failed, s_comma(inp["content_len"]))
    return stub_txt + marker


def _sh_dedupe_append(items, value, limit=12):
    value = s_strip(value)
    if value and value not in items and len(items) < limit:
        items.append(value)


def _sh_collect_obj(obj, files):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in {"path", "workdir", "file_path", "output_path"} and isinstance(v, str):
                _sh_dedupe_append(files, v, 12)
            _sh_collect_obj(v, files)
    elif isinstance(obj, list):
        for v in obj:
            _sh_collect_obj(v, files)
    elif isinstance(obj, str):
        for m in _fire("_path_mention_findall", {"text": obj}):
            _sh_dedupe_append(files, s_rstrip(m, ".,:;"), 12)


def _sh_collect_paths_from_jsonish(inp):
    files = list(inp.get("relevant_files") or [])
    _sh_collect_obj(inp.get("obj"), files)
    return files


def _sh_sum_clarify(inp):
    payload = _fire("_json_dict", {"text": inp.get("content")})
    payload = payload if isinstance(payload, dict) else {}
    responses = payload.get("responses")
    if not isinstance(responses, list) and "user_response" in payload:
        responses = [payload]
    answers = []
    for entry in responses if isinstance(responses, list) else ():
        if isinstance(entry, dict) and (entry.get("status") == "answered" or
                                        ("status" not in entry and "user_response" in entry)):
            value = entry.get("user_response")
            values = value if isinstance(value, list) else [value]
            if "status" not in entry:
                values = _fire("_filter_legacy_clarify_answers", {"values": values})
            answers.extend(values)
    answers = [a for a in answers if isinstance(a, str) and a]
    if not answers:
        return "[clarify] asked user a question"
    val = answers[0] if len(answers) == 1 else answers
    serialized = _fire("_json_dumps", {"value": val, "ensure_ascii": False})
    return _fire("_elide", {"text": "[clarify] user responded: " + serialized,
                            "max_chars": 199})


SHADOW_DISPATCH = {
    "_effective_input_window": _sh_effective_input_window,
    "_model_threshold_key_rank": _sh_model_threshold_key_rank,
    "_is_text_only_content": _sh_is_text_only_content,
    "_is_clarify_non_response": _sh_is_clarify_non_response,
    "_restated_request_text": _sh_restated_request_text,
    "_authored_request_text": _sh_authored_request_text,
    "_pending_tool_round": _sh_pending_tool_round,
    "_image_payload": _sh_image_payload,
    "_sum_skill_view": _sh_sum_skill_view,
    "_collect_paths_from_jsonish": _sh_collect_paths_from_jsonish,
    "_sum_clarify": _sh_sum_clarify,
}


def main_shadow():
    global NS
    NS = build_ns()
    lines = [json.loads(l) for l in open(FIXTURE, encoding="utf-8") if l.strip()]
    mism = 0
    for rec in lines:
        inp = rec["in"]
        shadow_trace = []
        CURRENT["S"] = Script(inp.get("io"), shadow_trace)
        sst = {}
        try:
            if rec["func"] == "_collect_paths_from_jsonish":
                files = list(inp.get("relevant_files") or [])
                sval = None
                sst = {"relevant_files": files}
                _sh_collect_obj(inp.get("obj"), files)
            else:
                sval = shadow(rec["func"], inp)
            sret = canon(sval)
        except BaseException as e:
            sret = {"__raise__": type(e).__name__, "message": str(e)}
        live = run_live(rec)
        if (sret != live["ret"] or
                [canon(t) for t in shadow_trace] != live["trace"] or
                canon(sst) != live["st"]):
            mism += 1
            print("SHADOW-MISMATCH", rec["func"], "shadow=", sret, "live=", live["ret"],
                  "strace=", [canon(t) for t in shadow_trace], "ltrace=", live["trace"],
                  "sst=", sst, "lst=", live["st"])
    print("SHADOW context_compressor_flow4: %d cases, %d mismatches" % (len(lines), mism))
    return 1 if mism else 0


if __name__ == "__main__":
    if "--shadow" in sys.argv:
        sys.exit(main_shadow())
    sys.exit(main_gate())
