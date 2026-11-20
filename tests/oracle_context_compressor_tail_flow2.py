#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_tail_flow2.c — drives the LIVE
agent/context_compressor_tail.py UNCLAIMED-remainder FLOW + leaf defs (12 PoP
entries: 10 module functions + the nested prices / unruled closures) under
IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window ({} — every entry is pure / build-new).
Live defs are AST-extracted in source order; module constants are seeded
(plain-Name Assign), plus the agent.context_compressor constants the targets
import (_MIN_SUMMARY_TOKENS / LEAN_TAIL_* / SKILL_PRUNED_MARKER_PREFIX). Ported
siblings stay REAL so composition is exercised (demote -> skill_view_names;
with_skill_safety_rule -> unruled -> without_skill_safety_rule; elide_keeping ->
without_skill_safety_rule; native_anthropic_tail_prices -> prices). Claimed
siblings / foreign I/O (_lean_recovery_stub, _is_summary_stub, _json_dict,
_result_failure_suffix, _skill_pruned_marker, _rewritten, _tc_get,
_content_text_for_contains, elide, _estimate_msg_budget_tokens, the anthropic
policy/replay/sanitization + tool_result_storage collaborators, and the
compressor._tail_row_pricer / ._is_context_summary_message methods) are
recording seams fed the SAME scripts the C harness feeds.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every guard exactly as live (none of the 12 defs swallows
Exception). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (tagged
with __cct2_raw__ so arg/ret projection re-emits the raw wrapper form the C
seam carries); callable doubles {"__callable__": "<seam>"}; messages / rows are
PLAIN dicts (isinstance(x, dict) is False for an __attr__ double); integral
floats normalize to ints on both sides.

Domain (ASCII scope, documented in the port header): text positions carry ASCII
bytes plus the byte-pinned U+2014 em dash; len() counts code points; str()
positions carry str/integral/bool/None; tool_calls iterables are list/str/dict/
falsy; tail_start/head_end/probe/limit integral with negative-index resolution.

Summary: 'ORACLE context_compressor_tail_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import json
import math
import os
import re as _re
import sys
import types
import __future__
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Callable, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "context_compressor_tail.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "context_compressor_tail.py")
CC_SRC = str(QUARRY / "agent" / "context_compressor.py")

ANN_FLAG = __future__.annotations.compiler_flag

# 10 module functions + 2 nested closures (prices, unruled).
FUNCS = [
    "lean_tail_budget", "is_demoted_stub", "skill_view_names_by_call_id",
    "demote_stale_tail_tools", "native_anthropic_tail_prices", "prices",
    "tail_cut_pricer", "with_skill_safety_rule", "unruled",
    "without_skill_safety_rule", "elide_keeping_skill_safety_rule",
    "with_pruned_row_skill_safety_rule",
]
MODULE_FUNCS = [f for f in FUNCS if f not in ("prices", "unruled")]
NESTED = {"prices": "native_anthropic_tail_prices", "unruled": "with_skill_safety_rule"}

SEAM_NAMES = [
    "has_archive_pointer", "_tc_get", "_json_dict", "_lean_recovery_stub",
    "_is_summary_stub", "_result_failure_suffix", "_rewritten",
    "_skill_pruned_marker", "archive_compaction_drops",
    "native_anthropic_preserves_prior_thinking", "session_rejected_thinking",
    "strip_rejected_thinking", "native_anthropic_row_costs",
    "_estimate_msg_budget_tokens", "compressor._tail_row_pricer",
    "compressor._is_context_summary_message", "_content_text_for_contains",
    "elide",
]

WANT_CONST = {
    "_LEAN_TAIL_KEEP_TOOL_ROUNDS", "_LEAN_TAIL_DEMOTE_MIN_CHARS",
    "_DEMOTED_STUB_RE", "TAIL_MAX_THRESHOLD_FRACTION", "SKILL_SAFETY_HEADING",
    "SKILL_SAFETY_RULE", "_SKILL_SAFETY_SUFFIX",
}
CC_CONST = {
    "_MIN_SUMMARY_TOKENS", "LEAN_TAIL_FLOOR_TOKENS", "LEAN_TAIL_CAP_TOKENS",
    "SKILL_PRUNED_MARKER_PREFIX",
}

CURRENT = {"rec": None}
NS = {}


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    f.__seam_name__ = seam
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    """Resolve the named class against the FULL builtin hierarchy (builtins
    itself), then asyncio.CancelledError, then a custom Exception subclass."""
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        if kind == "CancelledError":
            import asyncio
            cls = asyncio.CancelledError
        else:
            cls = type(kind, (Exception,), {})
    if kind == "KeyError":
        try:
            return cls(ast.literal_eval(msg))
        except (ValueError, SyntaxError):
            return cls(msg)
    return cls(msg)


_MISSING = object()


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def norm_value(v):
    if isinstance(v, SimpleNamespace):
        if hasattr(v, "__cct2_raw__"):
            return norm_value(v.__cct2_raw__)
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float) and math.isfinite(v) and v == int(v):
        return int(v)
    return v


def norm_args(v):
    # Mirrors the C harness cct2_h_strip_nulls: object keys with a null value
    # are dropped recursively; ARRAYS are copied verbatim (null elements and
    # null-valued keys inside array elements are kept), only normalized.
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_value(x) for x in v]
    return v


def proj(name, a, k):
    if name == "has_archive_pointer":
        return {"content": a[0]}
    if name == "_tc_get":
        return {"obj": a[0], "key": a[1], "default": a[2] if len(a) > 2 else ""}
    if name == "_json_dict":
        return {"text": a[0]}
    if name == "_lean_recovery_stub":
        return {"tool_name": a[0], "content_len": a[1], "session_id": a[2]}
    if name == "_is_summary_stub":
        return {"content": a[0]}
    if name == "_result_failure_suffix":
        return {"content": a[0]}
    if name == "_rewritten":
        return {"msg": a[0], "content": a[1]}
    if name == "_skill_pruned_marker":
        return {"skill_name": a[0]}
    if name == "archive_compaction_drops":
        return {"messages": a[0], "dropped": a[1], "session_id": a[2]}
    if name == "native_anthropic_preserves_prior_thinking":
        return {"base_url": a[0], "model": a[1]}
    if name == "session_rejected_thinking":
        return {"holder": a[0], "session_db": a[1], "session_id": a[2],
                "persistence": k.get("persistence")}
    if name == "strip_rejected_thinking":
        return {"message": a[0], "rejected": a[1]}
    if name == "native_anthropic_row_costs":
        return {"messages": a[0]}
    if name == "_estimate_msg_budget_tokens":
        return {"msg": a[0]}
    if name == "compressor._tail_row_pricer":
        return {"messages": a[0]}
    if name == "compressor._is_context_summary_message":
        return {"msg": a[0]}
    if name == "_content_text_for_contains":
        return {"content": a[0]}
    if name == "elide":
        return {"text": a[0], "limit": a[1]}
    return {**{("arg%d" % i): v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return materialize_result(name, raw)
        return call


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            ns = SimpleNamespace(**{k: materialize_double(v) for k, v in inner.items()})
            ns.__cct2_raw__ = j
            return ns
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def materialize_rows(rows):
    """Shallow top-level row materialization: an __attr__ wrapper becomes a
    SimpleNamespace (isinstance dict False, .get raises); plain dicts stay raw
    so dict(m) / .get / {**m} behave live. Nested values stay raw JSON."""
    if not isinstance(rows, list):
        return rows
    out = []
    for m in rows:
        if isinstance(m, dict) and "__attr__" in m:
            inner = m["__attr__"] or {}
            ns = SimpleNamespace(**inner)
            ns.__cct2_raw__ = m
            out.append(ns)
        else:
            out.append(m)
    return out


def materialize_result(name, raw):
    if name == "_json_dict":
        return raw   # plain dict passthrough so .get("name") works live
    if name == "native_anthropic_row_costs" and isinstance(raw, list):
        out = []
        for pair in raw:
            if isinstance(pair, list) and len(pair) >= 1:
                p = list(pair)
                if isinstance(p[0], dict) and "__attr__" in p[0]:
                    p[0] = materialize_double(p[0])
                out.append(p)
            else:
                out.append(pair)
        return out
    if isinstance(raw, dict) and ("__attr__" in raw or "__callable__" in raw):
        return materialize_double(raw)
    return raw


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
    import textwrap
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    exec(compile(seg, path, "exec", flags=ANN_FLAG, dont_inherit=True), ns)


def _assign_consts(src, path, want, ns):
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if names & want:
                _exec_seg(src, node, ns, path)
        elif isinstance(node, ast.AnnAssign) and node.value is not None and isinstance(node.target, ast.Name):
            if node.target.id in want:
                _exec_seg(src, node, ns, path)


def load_ns():
    global NS
    mod = types.ModuleType("cct2_live")
    sys.modules["cct2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cct2_live", "json": json, "re": _re,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Dict": dict, "List": list, "Tuple": tuple, "Sequence": list,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    _assign_consts(src, SRC, WANT_CONST, ns)
    # constants imported from agent.context_compressor
    csrc = Path(CC_SRC).read_text(encoding="utf-8")
    cc_consts = {}
    _assign_consts(csrc, CC_SRC, CC_CONST, cc_consts)

    tree = ast.parse(src)
    found = set()
    nested_nodes = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in MODULE_FUNCS:
            _exec_seg(src, node, ns, SRC)
            found.add(node.name)
            for sub in ast.walk(node):
                if isinstance(sub, ast.FunctionDef) and sub.name in NESTED:
                    nested_nodes[sub.name] = sub
    missing = set(MODULE_FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # nested closures standalone (free vars become per-case namespace globals)
    for nm in NESTED:
        if nm not in nested_nodes:
            raise RuntimeError("missing nested def: %s" % nm)
        _exec_seg(src, nested_nodes[nm], ns, SRC)
    for nm in sorted(WANT_CONST):
        if nm not in ns:
            raise RuntimeError("constant not seeded: %s" % nm)

    # shared recorder stubs (ns globals for the standalone prices closure AND
    # the sys.modules import stubs resolve to the SAME recorders).
    stubs = {nm: _stub_fn(nm) for nm in SEAM_NAMES}
    ns["strip_rejected_thinking"] = stubs["strip_rejected_thinking"]
    ns["native_anthropic_row_costs"] = stubs["native_anthropic_row_costs"]
    ns["_estimate_msg_budget_tokens"] = stubs["_estimate_msg_budget_tokens"]
    # _lean_recovery_stub is a CLAIMED module-global sibling (cc_lean_recovery_stub
    # in context_compressor_pure.c) referenced bare by demote_stale_tail_tools —
    # seam it into the namespace so the live demote records the same event.
    ns["_lean_recovery_stub"] = stubs["_lean_recovery_stub"]

    # in-function import stubs (overwrite parent packages so the real quarry
    # submodule cannot leak past the stub).
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    sr_pkg = types.ModuleType("session_rules"); sr_pkg.__path__ = []
    sys.modules["session_rules"] = sr_pkg
    tools_pkg = types.ModuleType("tools"); tools_pkg.__path__ = []
    sys.modules["tools"] = tools_pkg

    cc = types.ModuleType("agent.context_compressor")
    for k, v in cc_consts.items():
        setattr(cc, k, v)
    for nm in ("_tc_get", "_json_dict", "_is_summary_stub", "_result_failure_suffix",
               "_rewritten", "_skill_pruned_marker", "_estimate_msg_budget_tokens"):
        setattr(cc, nm, stubs[nm])
    sys.modules["agent.context_compressor"] = cc
    agent_pkg.context_compressor = cc

    pol = types.ModuleType("agent.anthropic_thinking_policy")
    pol.native_anthropic_preserves_prior_thinking = stubs["native_anthropic_preserves_prior_thinking"]
    sys.modules["agent.anthropic_thinking_policy"] = pol
    agent_pkg.anthropic_thinking_policy = pol

    rep = types.ModuleType("agent.anthropic_thinking_replay")
    rep.session_rejected_thinking = stubs["session_rejected_thinking"]
    rep.strip_rejected_thinking = stubs["strip_rejected_thinking"]
    sys.modules["agent.anthropic_thinking_replay"] = rep
    agent_pkg.anthropic_thinking_replay = rep

    san = types.ModuleType("agent.message_sanitization")
    san.native_anthropic_row_costs = stubs["native_anthropic_row_costs"]
    sys.modules["agent.message_sanitization"] = san
    agent_pkg.message_sanitization = san

    cm = types.ModuleType("agent.compression_marker")
    cm.elide = stubs["elide"]
    sys.modules["agent.compression_marker"] = cm
    agent_pkg.compression_marker = cm

    turns = types.ModuleType("session_rules.turns")
    turns._content_text_for_contains = stubs["_content_text_for_contains"]
    sys.modules["session_rules.turns"] = turns
    sr_pkg.turns = turns

    trs = types.ModuleType("tools.tool_result_storage")
    trs.has_archive_pointer = stubs["has_archive_pointer"]
    trs.archive_compaction_drops = stubs["archive_compaction_drops"]
    sys.modules["tools.tool_result_storage"] = trs
    tools_pkg.tool_result_storage = trs
    return ns


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    if func == "lean_tail_budget":
        return NS[func](inp.get("context_length"), inp.get("threshold_tokens")), {}
    if func == "is_demoted_stub":
        return NS[func](inp.get("content")), {}
    if func == "skill_view_names_by_call_id":
        return NS[func](materialize_rows(inp.get("messages"))), {}
    if func == "demote_stale_tail_tools":
        return NS[func](materialize_rows(inp.get("messages")), inp.get("tail_start"),
                        inp.get("session_id")), {}
    if func == "native_anthropic_tail_prices":
        comp = materialize_double(inp.get("compressor"))
        ret = NS[func](comp)
        if ret is None:
            return None, {}
        return ret(materialize_rows(inp.get("messages"))), {}
    if func == "prices":
        NS["rejected"] = inp.get("rejected")
        return NS["prices"](materialize_rows(inp.get("messages"))), {}
    if func == "tail_cut_pricer":
        comp = materialize_double(inp.get("compressor"))
        ret = NS[func](comp, inp.get("messages"), inp.get("head_end"))
        return ret(inp.get("probe")), {}
    if func == "with_skill_safety_rule":
        return NS[func](inp.get("summary"), materialize_rows(inp.get("head")),
                        materialize_rows(inp.get("tail"))), {}
    if func == "unruled":
        return NS["unruled"](materialize_rows(inp.get("rows"))), {}
    if func == "without_skill_safety_rule":
        return NS[func](inp.get("content")), {}
    if func == "elide_keeping_skill_safety_rule":
        return NS[func](inp.get("content"), inp.get("limit")), {}
    if func == "with_pruned_row_skill_safety_rule":
        return NS[func](materialize_rows(inp.get("original")),
                        materialize_rows(inp.get("pruned"))), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


def self_test():
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    load_ns()
    fx = _REPO / "tests" / "fixtures" / "context_compressor_tail_flow2.jsonl"
    cases = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
    funcs_seen = set()
    bad = 0
    names = set(FUNCS) | set(SEAM_NAMES)
    for c in cases:
        func = c["func"]
        funcs_seen.add(func)
        try:
            res = run_case(func, c["in"])
        except BaseException as e:
            print("DRIVE-EXC [%s]: %s: %s" % (func, type(e).__name__, e))
            bad += 1
            continue
        ret = res["ret"]
        if isinstance(ret, dict) and "__raise__" in ret:
            kind = ret["__raise__"]
            msg = ret.get("message", "")
            if kind in ("AttributeError", "NameError"):
                if any(("'%s'" % nm) in msg for nm in names):
                    print("SUSPECT [%s]: %s: %s" % (func, kind, msg))
                    bad += 1
    missing = funcs_seen - set(FUNCS)
    untested = set(FUNCS) - funcs_seen
    if missing:
        print("FUNC NOT IN FUNCS:", sorted(missing)); bad += 1
    if untested:
        print("FUNC UNTESTED:", sorted(untested)); bad += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELF-TEST %d cases, %d suspect" % (len(cases), bad))
    return 1 if bad else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    try:
        load_ns()
    except BaseException as e:
        print("PYERR load_ns: %s: %s" % (type(e).__name__, e))
        return 1
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE context_compressor_tail_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
