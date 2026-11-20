#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow3.c — drives the LIVE
agent/context_compressor.py third-wave FLOW defs (15 UNCLAIMED module functions)
under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted in source
order with future-annotations; module constants (_PRUNE_MIN_CHARS,
_IMAGE_PART_TYPES) are seeded; claimed siblings / foreign helpers are recording
seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. FULL Python
builtin exception hierarchy (build_exc): a scripted base-only kind
(SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseExceptionGroup)
propagates as a BaseException exactly as live; every other builtin resolves
from `builtins` (including the py3.14 additions and the OSError/ImportError/
Warning families). Unknown custom names -> RuntimeError (Exception descendant).

`--shadow` mode reads the fixture file directly and diffs a literal Python
transcription of the C port against the LIVE defs (shadow-diff 0 expected).

Summary: 'ORACLE context_compressor_flow3: N cases, M mismatches'.
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
FIXTURE = _HERE.parent / "fixtures" / "context_compressor_flow3.jsonl"

FUNCS = [
    "_sum_terminal", "_sum_write_file", "_sum_search_files", "_sum_browser",
    "_sum_delegate_task", "_sum_execute_code", "_sum_web_extract",
    "_sum_skills_list", "_sum_skill_manage", "_sum_cronjob_manage",
    "_sum_process_manage", "_summarize_refused_tool_result", "_bullets",
    "_tool_result_parts", "_summary_part_text",
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
    # OSError aliases that Python exposes as distinct names resolve fine.
    return cls(msg)


# ── canonicalization ──────────────────────────────────────────────────────
def canon(v, drop_null=False):
    if isinstance(v, dict):
        return {k: canon(x, drop_null) for k, x in v.items()
                if not (drop_null and x is None)}
    if isinstance(v, (list, tuple)):
        return [canon(x, drop_null) for x in v]
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


def install_seams(ns):
    def s_str_arg(args, key, default=""):
        return _fire("_str_arg", {"args": args, "key": key, "default": default})
    def s_result_failure_suffix(content):
        return _fire("_result_failure_suffix", {"content": content})
    def s_payload_failure_suffix(payload):
        return _fire("_payload_failure_suffix", {"payload": payload})
    def s_failure_suffix(reason, label="FAILED"):
        return _fire("_failure_suffix", {"reason": reason, "label": label})
    def s_json_dict(text):
        return _fire("_json_dict", {"text": text})
    def s_image_part_label(part):
        return _fire("_image_part_label", {"part": part})
    ns["_str_arg"] = s_str_arg
    ns["_result_failure_suffix"] = s_result_failure_suffix
    ns["_payload_failure_suffix"] = s_payload_failure_suffix
    ns["_failure_suffix"] = s_failure_suffix
    ns["_json_dict"] = s_json_dict
    ns["_image_part_label"] = s_image_part_label
    ns["re"] = _re
    ns["json"] = json


def build_ns():
    ns = dict(NS_BASE)
    install_seams(ns)
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
    # constants imported from session_rules.turns / declared locally
    ns["_PRUNE_MIN_CHARS"] = 200
    ns["_IMAGE_PART_TYPES"] = {"image", "image_url", "input_image"}
    # AST-extract the LIVE target defs in source order
    by_name = {}
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
            by_name[node.name] = node
    for name in FUNCS:
        node = by_name.get(name)
        if node is None:
            raise SystemExit("LIVE def not found: " + name)
        seg = ast.get_source_segment(SRC_TEXT, node)
        seg = ast.unparse(node)  # dedents method bodies; module defs unchanged
        exec(compile(seg, SRC, "exec", flags=ANN_FLAG, dont_inherit=True), ns)
    return ns


NS_BASE = {"__name__": "cc3_oracle_quarry"}


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
        ret = canon(val)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": ret, "trace": [canon(t) for t in trace],
            "st": canon(inp.get("state", {}))}


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
    print("ORACLE context_compressor_flow3: %d cases, %d mismatches" % (len(recs), mism))
    if not recs:
        print("ORACLE context_compressor_flow3: 0 cases => FAIL CLOSED", file=sys.stderr)
        return 1
    return 1 if mism else 0


# ── shadow: literal Python mirror of the C port (independent of LIVE defs) ──
_WS = " \t\n\r\v\f"


def s_strip(s):
    return (s or "").strip(_WS)


def s_lstrip(s):
    return (s or "").lstrip(_WS)


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


def s_get(o, k, default):
    v = o.get(k) if isinstance(o, dict) else None
    return default if v is None else s_py_str(v)


def s_comma(n):
    return format(int(n), ",")


def s_extract_num(text, key, neg):
    m = _re.search(r'"%s"\s*:\s*(%s\d+)' % (_re.escape(key), r"\-?" if neg else ""), text or "")
    return m.group(1) if m else ""


def s_truth(v):
    return bool(v)


SHADOW_DISPATCH = {}


def shadow(name, inp):
    fire = lambda nm, args: _fire(nm, args)
    fn = SHADOW_DISPATCH.get(name)
    if fn is None:
        raise KeyError(name)
    return fn(inp, fire)


def _sh_sum_terminal(inp, fire):
    args, content = inp["args"], inp.get("content")
    cmd = fire("_str_arg", {"args": args, "key": "command", "default": ""})
    cmd = cmd if len(cmd) <= 80 else cmd[:77] + "..."
    code = s_extract_num(content, "exit_code", True) or "?"
    return "[terminal] ran `%s` -> exit %s, %d lines output" % (cmd, code, inp["line_count"])


def _sh_sum_write_file(inp, fire):
    args, content = inp["args"], inp.get("content")
    path = s_get(args, "path", "?")
    failed = fire("_result_failure_suffix", {"content": content})
    if failed:
        return "[write_file] %s%s" % (path, failed)
    lines = "?"
    if s_truth(args.get("content")):
        c = fire("_str_arg", {"args": args, "key": "content", "default": ""})
        lines = str(c.count("\n") + 1)
    return "[write_file] wrote to %s (%s lines)" % (path, lines)


def _sh_sum_search_files(inp, fire):
    args, content = inp["args"], inp.get("content")
    count = s_extract_num(content, "total_count", False) or "?"
    return "[search_files] %s search for '%s' in %s -> %s matches" % (
        s_get(args, "target", "content"), s_get(args, "pattern", "?"),
        s_get(args, "path", "."), count)


def _sh_sum_browser(inp, fire):
    args, content = inp["args"], inp.get("content")
    url, ref = s_get(args, "url", ""), s_get(args, "ref", "")
    detail = (" " + url) if url else ((" ref=" + ref) if ref else "")
    fail = fire("_result_failure_suffix", {"content": content})
    return "[%s]%s (%s chars)%s" % (inp["name"], detail, s_comma(inp["content_len"]), fail)


def _sh_sum_delegate_task(inp, fire):
    args, content = inp["args"], inp.get("content")
    goal = fire("_str_arg", {"args": args, "key": "goal", "default": ""})
    goal = goal if len(goal) <= 60 else goal[:57] + "..."
    fail = fire("_result_failure_suffix", {"content": content})
    return "[delegate_task] '%s' (%s chars result)%s" % (goal, s_comma(inp["content_len"]), fail)


def _sh_sum_execute_code(inp, fire):
    args = inp["args"]
    code = fire("_str_arg", {"args": args, "key": "code", "default": ""})
    preview = code[:60].replace("\n", " ") + ("..." if len(code) > 60 else "")
    return "[execute_code] `%s` (%d lines output)" % (preview, inp["line_count"])


def _sh_sum_web_extract(inp, fire):
    args, content = inp["args"], inp.get("content")
    urls = args.get("urls", [])
    first = "?"
    if isinstance(urls, list) and urls:
        f0 = urls[0]
        if isinstance(f0, dict):
            first = f0.get("url") or f0.get("href") or "?"
        elif isinstance(f0, str):
            first = f0
        else:
            first = "?"
        if len(urls) > 1:
            first += " (+%d more)" % (len(urls) - 1)
    fail = fire("_result_failure_suffix", {"content": content})
    return "[web_extract] %s (%s chars)%s" % (first, s_comma(inp["content_len"]), fail)


def _sh_sum_skills_list(inp, fire):
    args, content = inp["args"], inp.get("content")
    cat = fire("_str_arg", {"args": args, "key": "category", "default": ""})
    scope = (" category=" + cat) if cat else ""
    payload = fire("_json_dict", {"text": content})
    count = payload.get("count")
    listed = (" %s skills" % s_py_str(count)) if isinstance(count, int) else ""
    fail = fire("_result_failure_suffix", {"content": content})
    return "[skills_list]%s%s%s (%s chars)" % (scope, listed, fail, s_comma(inp["content_len"]))


def _sh_sum_skill_manage(inp, fire):
    args, content = inp["args"], inp.get("content")
    ops = args.get("operations")
    if isinstance(ops, list) and ops:
        rendered = []
        for op in ops:
            if not isinstance(op, dict):
                continue
            a = fire("_str_arg", {"args": op, "key": "action", "default": "?"})
            n = fire("_str_arg", {"args": op, "key": "name", "default": "?"})
            rendered.append("%s %s" % (a, n))
        summary = "[skill_manage] " + "; ".join(rendered[:3])
        if len(ops) > 3:
            summary += " (+%d more)" % (len(ops) - 3)
    else:
        a = fire("_str_arg", {"args": args, "key": "action", "default": "?"})
        n = fire("_str_arg", {"args": args, "key": "name", "default": "?"})
        summary = "[skill_manage] %s %s" % (a, n)
    fail = fire("_result_failure_suffix", {"content": content})
    return "%s%s (%s chars)" % (summary, fail, s_comma(inp["content_len"]))


def _sh_sum_cronjob_manage(inp, fire):
    args, content = inp["args"], inp.get("content")
    stub = "[cronjob] " + s_get(args, "action", "?")
    payload = fire("_json_dict", {"text": content})
    suffix = fire("_payload_failure_suffix", {"payload": payload}) or ""
    if not suffix:
        job = payload.get("job")
        job = job if isinstance(job, dict) else {}
        error = job.get("execution_error")
        skipped = job.get("execution_skipped")
        if error is not None or (skipped is None and job.get("execution_success") is False):
            suffix = fire("_failure_suffix", {"reason": error, "label": "FAILED"})
        elif skipped is not None:
            suffix = fire("_failure_suffix", {"reason": skipped, "label": "SKIPPED"})
    return stub + suffix


def _sh_sum_process_manage(inp, fire):
    args, content = inp["args"], inp.get("content")
    stub = "[process] %s session=%s" % (s_get(args, "action", "?"), s_get(args, "session_id", "?"))
    payload = fire("_json_dict", {"text": content})
    suffix = fire("_payload_failure_suffix", {"payload": payload}) or ""
    if not suffix:
        ec = payload.get("exit_code")
        if isinstance(ec, int) and not isinstance(ec, bool) and ec != 0 and payload.get("completion_reason") != "killed":
            suffix = fire("_failure_suffix", {"reason": "exit code %d" % ec, "label": "FAILED"})
    return stub + suffix


def _sh_summarize_refused(inp, fire):
    args, content = inp["args"], inp.get("content")
    payload = fire("_json_dict", {"text": content})
    status = payload.get("status")
    error = payload.get("error") if isinstance(payload.get("error"), str) else ""
    if not error and (content or "").lstrip().startswith("BLOCKED"):
        error = content.strip()
    if status == "pending_approval":
        outcome = "awaiting the user's approval, not run"
    elif error and (status == "blocked" or error.lstrip().startswith("BLOCKED")):
        outcome = "BLOCKED, not run"
        if "NOT consented" in error:
            outcome += "; the user did NOT consent, do not retry or reach the same outcome another way"
    else:
        return None
    target = fire("_str_arg", {"args": args, "key": "command", "default": ""}) or \
             fire("_str_arg", {"args": args, "key": "path", "default": ""})
    target = (" `%s`" % (target if len(target) <= 60 else target[:57] + "...")) if target else ""
    return ("[%s]%s %s" % (inp["tool_name"], target, outcome))[:199]


def _sh_bullets(inp, fire):
    items = inp.get("items", [])
    limit = inp.get("limit", 8)
    unique = []
    for it in items:
        if len(unique) >= limit:      # Python [:limit]; limit==0 -> empty
            break
        s = s_strip(it if isinstance(it, str) else "")
        if s and s not in unique:
            unique.append(s)
    return "\n".join("- " + x for x in unique) if unique else "None."


def _sh_tool_result_parts(inp, fire):
    content = inp.get("content")
    if isinstance(content, dict) and content.get("_multimodal"):
        return content.get("content")
    return content


def _sh_summary_part_text(inp, fire):
    part = inp.get("part")
    if isinstance(part, str):
        return part
    ptype = part.get("type") if isinstance(part, dict) else None
    if ptype == "text":
        return part.get("text", "")
    if ptype in ("image", "image_url", "input_image"):
        return fire("_image_part_label", {"part": part})
    return "[%s]" % (ptype or "attachment")


SHADOW_DISPATCH = {
    "_sum_terminal": _sh_sum_terminal,
    "_sum_write_file": _sh_sum_write_file,
    "_sum_search_files": _sh_sum_search_files,
    "_sum_browser": _sh_sum_browser,
    "_sum_delegate_task": _sh_sum_delegate_task,
    "_sum_execute_code": _sh_sum_execute_code,
    "_sum_web_extract": _sh_sum_web_extract,
    "_sum_skills_list": _sh_sum_skills_list,
    "_sum_skill_manage": _sh_sum_skill_manage,
    "_sum_cronjob_manage": _sh_sum_cronjob_manage,
    "_sum_process_manage": _sh_sum_process_manage,
    "_summarize_refused_tool_result": _sh_summarize_refused,
    "_bullets": _sh_bullets,
    "_tool_result_parts": _sh_tool_result_parts,
    "_summary_part_text": _sh_summary_part_text,
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
        try:
            sval = shadow(rec["func"], inp)
            sret = canon(sval)
        except BaseException as e:
            sret = {"__raise__": type(e).__name__, "message": str(e)}
        live = run_live(rec)
        if sret != live["ret"] or [canon(t) for t in shadow_trace] != live["trace"]:
            mism += 1
            print("SHADOW-MISMATCH", rec["func"], "shadow=", sret, "live=", live["ret"],
                  "strace=", [canon(t) for t in shadow_trace], "ltrace=", live["trace"])
    print("SHADOW context_compressor_flow3: %d cases, %d mismatches" % (len(lines), mism))
    return 1 if mism else 0


if __name__ == "__main__":
    if "--shadow" in sys.argv:
        sys.exit(main_shadow())
    sys.exit(main_gate())
