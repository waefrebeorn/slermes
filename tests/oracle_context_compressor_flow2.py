#!/usr/bin/env python3
"""Oracle for src/agent/context_compressor_flow2.c — drives the LIVE
agent/context_compressor.py second-wave FLOW + leaf defs (24 UNCLAIMED module
functions) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window. Live defs are AST-extracted in source
order; module constants are seeded (plain-Name Assign/AnnAssign, plus the
session_rules.turns marker constants the targets import). Ported siblings stay
REAL so composition is exercised; claimed siblings / foreign I/O
(estimate_messages_tokens_rough, elide_keeping_skill_safety_rule,
without_skill_safety_rule, is_demoted_stub, _coerce_llm_message, _message_field,
held_archive_watermark, skills_loaded_by_scaffold, describe_skill_invocation,
and the persistence / session_db watermark methods) are recording seams fed the
SAME scripts the C harness feeds.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every guard exactly as live (none of the 24 defs swallows
Exception). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (tagged
with __cc2_raw__ so arg projection re-emits the raw wrapper form the C seam
carries); callable doubles {"__callable__": "<seam>"}; messages / payloads are
PLAIN dicts (isinstance(x, dict) is False for an __attr__ double); integral
floats normalize to ints on both sides.

Domain (ASCII scope, documented in the port header): str()/strip()/split()
positions carry ASCII text; _failure_suffix reason is str/integral/bool/None;
_json_dict stays in the json.loads/libjson agreement zone; int() coercion accepts
null/bool/number/signed-digit-string (int strip set excludes \\x1c-\\x1f).

Summary: 'ORACLE context_compressor_flow2: N cases, M mismatches'.
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
from typing import Any, Callable, Optional

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
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
SRC = str(QUARRY / "agent" / "context_compressor.py")
TURNS_SRC = str(QUARRY / "session_rules" / "turns.py")
CC_SRC = str(QUARRY / "agent" / "conversation_compression.py")

ANN_FLAG = __future__.annotations.compiler_flag

FUNCS = [
    "_is_checkpoint_item", "_newest_checkpoint_carrier", "_set_sidecar",
    "drop_shadowed_checkpoints", "_last_index_with_role", "_last_assistant_index",
    "_prune_stale_reasoning_replay", "stamp_db_persisted_markers",
    "_looks_like_compaction_summary", "_salvage_reduce_todo_snapshot",
    "salvage_grown_transcript", "_archive_watermark_for", "_exc_status_code",
    "_is_summary_refusal", "_response_refusal_text", "_is_refusal_response",
    "_is_summary_stub", "_skill_view_failed", "_synthetic_user_row",
    "_typed_user_text", "_json_dict", "_failure_suffix",
    "_payload_failure_suffix", "_result_failure_suffix",
]

# module-global seams (imported names the targets call; live name == seam name)
SEAM_GLOBALS = [
    "estimate_messages_tokens_rough", "elide_keeping_skill_safety_rule",
    "without_skill_safety_rule", "is_demoted_stub",
    "_coerce_llm_message", "_message_field",
]

WANT_CONST = {
    "_PRUNE_MIN_CHARS", "_PRUNED_TOOL_PLACEHOLDER", "_SALVAGE_SUMMARY_MAX_CHARS",
    "_SALVAGE_KEEP_RECENT_TOOLS", "_SYNTHETIC_USER_ROW_PREFIXES",
    "COMPRESSED_SUMMARY_METADATA_KEY", "_STALE_REPLAY_PRUNE_KEYS",
    "_SUMMARY_REFUSAL_PREFIX_RE",
}
TURNS_CONST = {"_DB_PERSISTED_MARKER", "_SUMMARY_END_MARKER", "_MERGED_PRIOR_CONTEXT_HEADER"}

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


# Full Python 3.14 builtin exception hierarchy.  Base-only (BaseException
# descendants that are not Exception descendants): the trio + CancelledError
# (asyncio) + BaseExceptionGroup.  Every other builtin exception class is an
# Exception descendant and is resolved from `builtins` by name, so the whole
# hierarchy (ArithmeticError, OSError family, ImportError family, Warning
# family, py3.14 PythonFinalizationError/_IncompleteInputError, ...) is covered
# without a hand-maintained partial table.  OSError aliases and unknown custom
# names fall through to Exception subclasses, matching the C cc2_exc_parent()
# walk in agent/context_compressor_flow2.c.
_BASE_ONLY_BUILTINS = {"SystemExit", "KeyboardInterrupt", "GeneratorExit"}


def build_exc(spec):
    import builtins
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    if kind in _BASE_ONLY_BUILTINS:
        cls = getattr(builtins, kind)
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    elif kind == "BaseExceptionGroup":
        cls = builtins.BaseExceptionGroup
    elif kind == "ExceptionGroup":
        cls = builtins.ExceptionGroup
    elif kind == "StaleHeldHistory":
        cls = NS.get("StaleHeldHistory") or type("StaleHeldHistory", (RuntimeError,), {})
    else:
        candidate = getattr(builtins, kind, None)
        if isinstance(candidate, type) and issubclass(candidate, BaseException) \
                and issubclass(candidate, Exception):
            cls = candidate  # any builtin Exception descendant (full hierarchy)
        else:
            cls = type(kind, (Exception,), {})  # unknown/custom -> Exception subclass
    if isinstance(cls, type) and issubclass(cls, KeyError):
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
        return {k: norm_value(x) for k, x in vars(v).items() if k != "__cc2_raw__"}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, float) and v == int(v):
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def double_proj(v):
    """Project a materialized attribute double back to its RAW wrapper form
    (the shape the C seam args carry). Non-doubles pass through norm_value."""
    if isinstance(v, SimpleNamespace) and hasattr(v, "__cc2_raw__"):
        return norm_value(v.__cc2_raw__)
    return norm_value(v)


def proj(name, a, k):
    if name == "estimate_messages_tokens_rough":
        return {"messages": norm_value(a[0])}
    if name == "elide_keeping_skill_safety_rule":
        return {"content": a[0], "limit": a[1]}
    if name in ("without_skill_safety_rule", "is_demoted_stub", "skills_loaded_by_scaffold"):
        return {"content": a[0]}
    if name == "_coerce_llm_message":
        return {"response": double_proj(a[0])}
    if name == "_message_field":
        return {"msg": double_proj(a[0]), "name": a[1]}
    if name == "held_archive_watermark":
        return {"session_db": double_proj(a[0]), "session_id": a[1], "watermark": a[2],
                "messages": norm_value(a[3]), "stale_raises": k.get("stale_raises"),
                "persistence": double_proj(k.get("persistence"))}
    if name == "describe_skill_invocation":
        return {"content": a[0], "separator": k.get("separator")}
    if name in ("persistence.messages.active_watermark", "session_db.get_active_message_watermark"):
        return {"session_id": a[0]}
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
            ns.__cc2_raw__ = j
            return ns
        cb = j.get("__callable__")
        if isinstance(cb, str):
            return CURRENT["rec"](cb)
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def materialize_result(name, raw):
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
    mod = types.ModuleType("cc2_live")
    sys.modules["cc2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "cc2_live", "json": json, "re": _re,
        "Any": Any, "Optional": Optional, "Callable": Callable,
        "Dict": dict, "List": list, "Tuple": tuple, "Sequence": list,
    })
    src = Path(SRC).read_text(encoding="utf-8")
    _assign_consts(src, SRC, WANT_CONST, ns)
    # marker constants imported from session_rules.turns
    tsrc = Path(TURNS_SRC).read_text(encoding="utf-8")
    _assign_consts(tsrc, TURNS_SRC, TURNS_CONST, ns)
    # target defs (source order)
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(src, node, ns, SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    for nm in sorted(WANT_CONST | TURNS_CONST):
        if nm not in ns:
            raise RuntimeError("constant not seeded: %s" % nm)
    # module-global seams
    for nm in SEAM_GLOBALS:
        ns[nm] = _stub_fn(nm)
    # in-function import stubs (overwrite parent packages so the real quarry
    # submodule cannot leak past the stub).
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    sr_pkg = types.ModuleType("session_rules"); sr_pkg.__path__ = []
    sys.modules["session_rules"] = sr_pkg
    cc = types.ModuleType("agent.conversation_compression")
    cc.held_archive_watermark = _stub_fn("held_archive_watermark")
    csrc = Path(CC_SRC).read_text(encoding="utf-8")
    cns = {}
    _assign_consts(csrc, CC_SRC, {"_PRUNED_SKILL_RELOAD_NOTICE_HEADER"}, cns)
    cc._PRUNED_SKILL_RELOAD_NOTICE_HEADER = cns["_PRUNED_SKILL_RELOAD_NOTICE_HEADER"]
    sys.modules["agent.conversation_compression"] = cc
    agent_pkg.conversation_compression = cc
    sc = types.ModuleType("agent.skill_commands")
    sc.skills_loaded_by_scaffold = _stub_fn("skills_loaded_by_scaffold")
    sys.modules["agent.skill_commands"] = sc
    agent_pkg.skill_commands = sc
    turns = types.ModuleType("session_rules.turns")
    turns.describe_skill_invocation = _stub_fn("describe_skill_invocation")
    sys.modules["session_rules.turns"] = turns
    sr_pkg.turns = turns
    return ns


def _state(inp, key):
    st = inp.get("state") or {}
    return copy.deepcopy(st.get(key))


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    if func == "_is_checkpoint_item":
        return NS[func](materialize_double(inp.get("item"))), {}
    if func == "_newest_checkpoint_carrier":
        return NS[func](inp.get("messages") or [], inp.get("key", "codex_reasoning_items")), {}
    if func == "_set_sidecar":
        msg = _state(inp, "msg")
        NS[func](msg, inp.get("key", ""), inp.get("kept") or [])
        return None, {"msg": norm_value(msg)}
    if func == "drop_shadowed_checkpoints":
        messages = _state(inp, "messages")
        ret = NS[func](messages, inp.get("key", "codex_reasoning_items"), before=inp.get("before"))
        return ret, {"messages": norm_value(messages)}
    if func == "_last_index_with_role":
        return NS[func](inp.get("messages") or [], inp.get("role", "")), {}
    if func == "_last_assistant_index":
        return NS[func](inp.get("messages") or []), {}
    if func == "_prune_stale_reasoning_replay":
        messages = _state(inp, "messages")
        ret = NS[func](messages)
        return ret, {"messages": norm_value(messages)}
    if func == "stamp_db_persisted_markers":
        messages = _state(inp, "messages")
        NS[func](messages)
        return None, {"messages": norm_value(messages)}
    if func == "_looks_like_compaction_summary":
        return NS[func](inp.get("msg") or {}, inp.get("content")), {}
    if func == "_salvage_reduce_todo_snapshot":
        out = _state(inp, "out")
        NS[func](out)
        return None, {"out": norm_value(out)}
    if func == "salvage_grown_transcript":
        return NS[func](inp.get("original") or [], inp.get("candidate") or [], inp.get("budget")), {}
    if func == "_archive_watermark_for":
        session_db = materialize_double(inp.get("session_db"))
        persistence = materialize_double(inp.get("persistence"))
        ret = NS[func](session_db, inp.get("session_id", ""), inp.get("held") or [],
                       inp.get("start_watermark"), persistence=persistence)
        return ret, {}
    if func == "_exc_status_code":
        return NS[func](materialize_double(inp.get("exc"))), {}
    if func == "_is_summary_refusal":
        return NS[func](inp.get("content")), {}
    if func == "_response_refusal_text":
        return NS[func](materialize_double(inp.get("response"))), {}
    if func == "_is_refusal_response":
        return NS[func](materialize_double(inp.get("response")), inp.get("content")), {}
    if func == "_is_summary_stub":
        return NS[func](inp.get("content")), {}
    if func == "_skill_view_failed":
        return NS[func](inp.get("content")), {}
    if func == "_synthetic_user_row":
        return NS[func](inp.get("content")), {}
    if func == "_typed_user_text":
        return NS[func](inp.get("content")), {}
    if func == "_json_dict":
        return NS[func](inp.get("text")), {}
    if func == "_failure_suffix":
        return NS[func](inp.get("reason"), label=inp.get("label", "FAILED")), {}
    if func == "_payload_failure_suffix":
        return NS[func](inp.get("payload") or {}), {}
    if func == "_result_failure_suffix":
        return NS[func](inp.get("content")), {}
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


def main():
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
    print("ORACLE context_compressor_flow2: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
