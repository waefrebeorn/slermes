#!/usr/bin/env python3
"""Oracle for src/agent/context_budget_flow.c — drives the LIVE context-window
budget planning defs under IDENTICAL scripted universal seams.

The task names agent/context_budget.py, which does not exist in the quarry; the
real defining module is agent/context_compressor_window.py (CompactionWindowMixin
+ summary_view / _restart_probe_extra). This oracle AST-extracts the 9 unclaimed
window FLOW defs (the mixin ClassDef + the two module functions) plus the leaves
tail_cut_progress_floor (agent/context_compressor_tail.py), _tool_calls_by_id /
_tc_get / _rewritten (agent/context_compressor.py) and archive_refs_in_text
(session_rules/media.py). Ported siblings stay REAL so composition is exercised;
claimed siblings (_protect_head_size, _effective_protect_first_n,
_restart_handoff_probe_bounds, _align_boundary_forward/_backward,
_find_tail_cut_by_tokens, _find_last_user_message_idx, _is_context_summary_message,
_prune_old_tool_results, drop_stale_api_content) and heavy/foreign collaborators
(_drop_blank_echoes, _demote_tool_result_at, _demote_stale_tail_tools,
_assemble_compressed, the tool_result_storage archive/backend functions,
prepared_compaction adopt/take/stop, logger) are recording seams fed the SAME
scripts the C harness feeds.

The mixin ClassDef is exec'd live (annotations lazy under __future__), so the
self double is a REAL CompactionWindowMixin instance: direct attribute reads
raise AttributeError naming 'CompactionWindowMixin'; class defaults
prepared_compaction=None / pin_task_turn=False apply. Seamed collaborators bind
as instance-attribute recorders (no self passed); ported methods stay as the
class's live methods.

In-function imports of CONSTANTS fire one "import_const" seam via the stub
module's __getattr__ (dunder lookups silent); imports of ported functions bind
the live leaf (compose inline); imports of seamed functions bind recorders.

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
(recorded as the event ret before propagation); 0 cases => exit 1. A scripted
BaseException (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
propagates past every guard exactly as live (none of the 14 defs has a
try/except). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed.

Value model: attribute doubles {"__attr__": ...} <-> SimpleNamespace (tagged
__cbg_raw__ so arg projection re-emits the raw wrapper the C seam carries);
rows / payloads are PLAIN dicts; integral floats normalize to ints; sets
normalize to sorted lists on both sides.

Domain (ASCII scope, documented in the port header): arrays of plain rows;
str tool ids/names; integral index seams; head_end/start/end/cut_idx >= 0
(summary_view slice bounds may be negative); _offload range bounds within
[0, len(plan)]; prepared/scan doubles carry summary/tail_start; import_const
scripts never raise AttributeError; seam RESULTS are plain records/arrays.

Summary: 'ORACLE context_budget_flow: N cases, M mismatches'.
"""
import ast
import copy
import faulthandler
import json
import os
import re as _re
import sys
import textwrap
import types
import __future__
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "context_compressor_window.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

WINDOW_SRC = str(QUARRY / "agent" / "context_compressor_window.py")
TAIL_SRC = str(QUARRY / "agent" / "context_compressor_tail.py")
CC_SRC = str(QUARRY / "agent" / "context_compressor.py")
MEDIA_SRC = str(QUARRY / "session_rules" / "media.py")

ANN_FLAG = __future__.annotations.compiler_flag

# ported window FLOW defs (mixin methods + module functions)
WINDOW_METHODS = [
    "_compress_window", "_plan_compaction_window", "_commit_window",
    "_assemble_with_archive_list", "_offload_window_results",
    "_take_prepared_summary", "_adopt_prepared_summary",
]
WINDOW_FUNCS = ["_restart_probe_extra", "summary_view"]
# ported leaves
LEAVES = ["tail_cut_progress_floor", "_tool_calls_by_id", "_tc_get",
          "_rewritten", "archive_refs_in_text"]
FUNCS = WINDOW_FUNCS + WINDOW_METHODS + LEAVES

# claimed / heavy collaborators seamed on the self double
SEAMED_METHODS = [
    "_protect_head_size", "_effective_protect_first_n",
    "_restart_handoff_probe_bounds", "_align_boundary_forward",
    "_align_boundary_backward", "_find_tail_cut_by_tokens",
    "_find_last_user_message_idx", "_is_context_summary_message",
    "_prune_old_tool_results", "_drop_blank_echoes", "_demote_tool_result_at",
    "_demote_stale_tail_tools", "_assemble_compressed",
]
# module-level seamed functions (in-function imports bind these recorders)
SEAM_GLOBALS = [
    "archive_compaction_drops", "archive_pruned_originals", "has_archive_pointer",
    "_backend_reads_host_files", "pruned_archive_refs", "archive_list_for_summary",
    "adopt",
]

CURRENT = {"rec": None, "state_raw": None}
NS = {}
MEDIA_NS = {}


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
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError, "RuntimeError": RuntimeError,
               "AttributeError": AttributeError, "KeyError": KeyError, "Exception": Exception,
               "TimeoutError": TimeoutError, "OSError": OSError, "LookupError": LookupError,
               "IndexError": IndexError, "ImportError": ImportError, "StopIteration": StopIteration,
               "NotImplementedError": NotImplementedError, "ZeroDivisionError": ZeroDivisionError,
               "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
               "ArithmeticError": ArithmeticError, "BufferError": BufferError,
               "EOFError": EOFError, "MemoryError": MemoryError, "NameError": NameError,
               "UnboundLocalError": UnboundLocalError, "ReferenceError": ReferenceError,
               "SyntaxError": SyntaxError, "IndentationError": IndentationError,
               "TabError": TabError, "SystemError": SystemError, "TypeError": TypeError,
               "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
               "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
               "AssertionError": AssertionError, "BlockingIOError": BlockingIOError,
               "BrokenPipeError": BrokenPipeError, "ChildProcessError": ChildProcessError,
               "ConnectionError": ConnectionError, "ConnectionAbortedError": ConnectionAbortedError,
               "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
               "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
               "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
               "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
               "ProcessLookupError": ProcessLookupError, "RecursionError": RecursionError,
               "StopAsyncIteration": StopAsyncIteration, "Warning": Warning,
               "BytesWarning": BytesWarning, "DeprecationWarning": DeprecationWarning,
               "EncodingWarning": EncodingWarning, "FutureWarning": FutureWarning,
               "ImportWarning": ImportWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
               "ResourceWarning": ResourceWarning, "RuntimeWarning": RuntimeWarning,
               "SyntaxWarning": SyntaxWarning, "UnicodeWarning": UnicodeWarning,
               "UserWarning": UserWarning}
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
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
        return {k: norm_value(x) for k, x in vars(v).items() if k != "__cbg_raw__"}
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=lambda z: json.dumps(z, sort_keys=True))
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
    if isinstance(v, SimpleNamespace) and hasattr(v, "__cbg_raw__"):
        return norm_value(v.__cbg_raw__)
    return norm_value(v)


def state_proj(slf):
    """compressor double -> the instance-attribute state dict (fixture keys).
    No ported def writes a self attribute, so the pristine raw state is exact."""
    return norm_value(CURRENT["state_raw"])


def proj(name, a, k):
    if name == "_protect_head_size":
        return {"messages": a[0]}
    if name in ("_align_boundary_forward", "_align_boundary_backward"):
        return {"messages": a[0], "idx": a[1]}
    if name in ("_find_tail_cut_by_tokens", "_find_last_user_message_idx"):
        return {"messages": a[0], "start": a[1]}
    if name == "_is_context_summary_message":
        return {"msg": a[0]}
    if name == "_prune_old_tool_results":
        return {"messages": a[0], "protect_tail_count": k.get("protect_tail_count"),
                "protect_tail_tokens": k.get("protect_tail_tokens")}
    if name == "_drop_blank_echoes":
        return {"messages": a[0]}
    if name == "_demote_tool_result_at":
        return {"result": a[0], "idx": a[1], "call_id_to_tool": a[2], "min_prune_chars": a[3]}
    if name == "_demote_stale_tail_tools":
        return {"messages": a[0], "tail_start": a[1]}
    if name == "_assemble_compressed":
        return {"messages": a[0], "start": a[1], "end": a[2],
                "scan": double_proj(a[3]), "summary": a[4]}
    if name == "stop_for_provider_context":
        return {}
    if name == "take":
        return {"messages": a[0], "session_id": a[1], "compress_start": a[2],
                "eligible": k.get("eligible")}
    if name == "adopt":
        return {"compressor": state_proj(a[0]), "prepared": double_proj(a[1])}
    if name == "logger.warning":
        return {"fmt": a[0], "args": list(a[1:])}
    if name == "drop_stale_api_content":
        return {"msg": a[0]}
    if name == "archive_compaction_drops":
        return {"messages": a[0], "dropped": a[1], "session_id": a[2]}
    if name == "_backend_reads_host_files":
        return {}
    if name == "archive_pruned_originals":
        return {"messages": a[0], "pruned": a[1], "session_id": a[2]}
    if name == "has_archive_pointer":
        return {"content": a[0]}
    if name == "pruned_archive_refs":
        return {"rows": a[0]}
    if name == "archive_list_for_summary":
        return {"named": sorted(a[0]), "kept": a[1]}
    if name == "import_const":
        return {"module": k.get("module"), "name": k.get("name")}
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
            # ---- collaborator-specific contracts ----
            if name == "_demote_tool_result_at":
                if isinstance(raw, dict) and "ret" in raw:
                    if raw["ret"] and "row" in raw:
                        a[0][a[1]] = raw["row"]
                    return raw["ret"]
                return raw
            if name == "drop_stale_api_content":
                a[0].pop("api_content", None)
                return None
            if name == "pruned_archive_refs":
                return set(raw or [])
            return materialize_result(name, raw)
        return call


def materialize_double(j):
    if isinstance(j, dict):
        if "__attr__" in j:
            inner = j["__attr__"] or {}
            ns = SimpleNamespace(**{k: materialize_double(v) for k, v in inner.items()})
            ns.__cbg_raw__ = j
            return ns
        return {k: materialize_double(v) for k, v in j.items()}
    if isinstance(j, list):
        return [materialize_double(v) for v in j]
    return j


def materialize_result(name, raw):
    if isinstance(raw, dict) and "__attr__" in raw:
        return materialize_double(raw)
    return raw


class RecLogger:
    def warning(self, fmt, *args):
        return CURRENT["rec"]("logger.warning")(fmt, *args)

    def debug(self, fmt, *args, **k):
        return CURRENT["rec"]("logger.debug")(fmt, *args, **k)


def _prepared_double(raw):
    """state.prepared_compaction: a PreparedCompaction double with seamed methods."""
    fields = {k: materialize_double(v) for k, v in (raw or {}).items()}
    ns = SimpleNamespace(**fields)
    ns.stop_for_provider_context = CURRENT["rec"]("stop_for_provider_context")
    ns.take = CURRENT["rec"]("take")
    ns.__cbg_raw__ = raw
    return ns


# ---- live namespace seeding ----
def _exec_seg(src, node, ns, path):
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
    global NS, MEDIA_NS
    mod = types.ModuleType("cbg_live")
    sys.modules["cbg_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({"__name__": "cbg_live", "json": json, "re": _re,
               "Any": Any, "Optional": Optional})
    # window module: constants, the two module functions, the mixin ClassDef
    wsrc = Path(WINDOW_SRC).read_text(encoding="utf-8")
    _assign_consts(wsrc, WINDOW_SRC, {"_SUMMARY_KEEPS_PRUNED_FORM"}, ns)
    wtree = ast.parse(wsrc)
    found = set()
    for node in wtree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WINDOW_FUNCS:
            _exec_seg(wsrc, node, ns, WINDOW_SRC)
            found.add(node.name)
        elif isinstance(node, ast.ClassDef) and node.name == "CompactionWindowMixin":
            _exec_seg(wsrc, node, ns, WINDOW_SRC)
            found.add("CompactionWindowMixin")
    if not {"_restart_probe_extra", "summary_view", "CompactionWindowMixin"} <= found:
        raise RuntimeError("missing window defs: %s" % sorted(found))
    # tail leaf
    tsrc = Path(TAIL_SRC).read_text(encoding="utf-8")
    for node in ast.parse(tsrc).body:
        if isinstance(node, ast.FunctionDef) and node.name == "tail_cut_progress_floor":
            _exec_seg(tsrc, node, ns, TAIL_SRC)
    # compressor leaves
    csrc = Path(CC_SRC).read_text(encoding="utf-8")
    ccwant = {"_tc_get", "_tool_calls_by_id", "_rewritten"}
    for node in ast.parse(csrc).body:
        if isinstance(node, ast.FunctionDef) and node.name in ccwant:
            _exec_seg(csrc, node, ns, CC_SRC)
    if not ccwant <= set(ns):
        raise RuntimeError("missing compressor leaves")
    # media leaf (separate namespace; needs re + the subdir/regex constants)
    mmod = types.ModuleType("cbg_media_live")
    sys.modules["cbg_media_live"] = mmod
    mns = mmod.__dict__
    MEDIA_NS = mns
    mns.update({"__name__": "cbg_media_live", "re": _re})
    msrc = Path(MEDIA_SRC).read_text(encoding="utf-8")
    _assign_consts(msrc, MEDIA_SRC, {"SPILLOVER_SUBDIR", "PRUNED_SUBDIR", "_ARCHIVE_NAME_RES"}, mns)
    for node in ast.parse(msrc).body:
        if isinstance(node, ast.FunctionDef) and node.name == "archive_refs_in_text":
            _exec_seg(msrc, node, mns, MEDIA_SRC)
    if "archive_refs_in_text" not in mns:
        raise RuntimeError("missing archive_refs_in_text")
    # logger (re-set AFTER any Assign seeding; module logger is an Assign)
    ns["logger"] = RecLogger()
    # claimed sibling: drop_stale_api_content (session_rules.turns) is a seam
    ns["drop_stale_api_content"] = _stub_fn("drop_stale_api_content")
    # ---- sys.modules stubs (None-halt the real quarry submodules) ----
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    cc = types.ModuleType("agent.context_compressor")
    cc._tool_calls_by_id = ns["_tool_calls_by_id"]   # ported leaf composes
    cc._rewritten = ns["_rewritten"]                 # ported leaf composes

    def cc_getattr(name):
        if name.startswith("__") and name.endswith("__"):
            raise AttributeError(name)
        return CURRENT["rec"]("import_const")(module="agent.context_compressor", name=name)
    cc.__getattr__ = cc_getattr
    sys.modules["agent.context_compressor"] = cc
    agent_pkg.context_compressor = cc
    tail = types.ModuleType("agent.context_compressor_tail")
    tail.tail_cut_progress_floor = ns["tail_cut_progress_floor"]
    sys.modules["agent.context_compressor_tail"] = tail
    agent_pkg.context_compressor_tail = tail
    pc = types.ModuleType("agent.prepared_compaction")
    pc.adopt = _stub_fn("adopt")
    sys.modules["agent.prepared_compaction"] = pc
    agent_pkg.prepared_compaction = pc
    tools_pkg = types.ModuleType("tools"); tools_pkg.__path__ = []
    sys.modules["tools"] = tools_pkg
    trs = types.ModuleType("tools.tool_result_storage")
    for nm in ("archive_compaction_drops", "archive_pruned_originals", "has_archive_pointer",
               "_backend_reads_host_files", "pruned_archive_refs", "archive_list_for_summary"):
        setattr(trs, nm, _stub_fn(nm))
    sys.modules["tools.tool_result_storage"] = trs
    tools_pkg.tool_result_storage = trs
    sr_pkg = types.ModuleType("session_rules"); sr_pkg.__path__ = []
    sys.modules["session_rules"] = sr_pkg
    turns = types.ModuleType("session_rules.turns")
    turns.drop_stale_api_content = _stub_fn("drop_stale_api_content")
    sys.modules["session_rules.turns"] = turns
    sr_pkg.turns = turns
    return ns


def _self(inp):
    """Build the live mixin self double from in.state + bind seamed recorders."""
    state = copy.deepcopy(inp.get("state") or {})
    CURRENT["state_raw"] = state
    slf = NS["CompactionWindowMixin"]()
    for k, v in state.items():
        if k == "prepared_compaction" and isinstance(v, dict):
            setattr(slf, k, _prepared_double(v))
        else:
            setattr(slf, k, materialize_double(v))
    for nm in SEAMED_METHODS:
        setattr(slf, nm, CURRENT["rec"](nm))
    return slf


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    st = copy.deepcopy(inp.get("state") or {})
    if func in WINDOW_METHODS or func == "tail_cut_progress_floor":
        slf = _self(inp)
        if func == "tail_cut_progress_floor":
            ret = NS["tail_cut_progress_floor"](slf, inp.get("messages"),
                                                inp.get("head_end", 0), inp.get("cut_idx", 0))
        elif func == "_compress_window":
            ret = slf._compress_window(inp.get("messages"))
        elif func == "_plan_compaction_window":
            ret = slf._plan_compaction_window(inp.get("messages"))
        elif func == "_commit_window":
            ret = slf._commit_window(inp.get("originals"), inp.get("plan"),
                                     inp.get("pruned_count", 0), inp.get("start", 0), inp.get("end", 0))
        elif func == "_assemble_with_archive_list":
            ret = slf._assemble_with_archive_list(inp.get("messages"), inp.get("start", 0),
                                                  inp.get("end", 0), materialize_double(inp.get("scan")),
                                                  inp.get("summary"))
        elif func == "_offload_window_results":
            ret = slf._offload_window_results(inp.get("plan"), inp.get("start", 0), inp.get("end", 0))
        elif func == "_take_prepared_summary":
            ret = slf._take_prepared_summary(inp.get("messages"), inp.get("compress_start", 0),
                                             fresh=inp.get("fresh", False),
                                             provider_context=inp.get("provider_context", False))
        elif func == "_adopt_prepared_summary":
            prepared = inp.get("prepared")
            pdouble = SimpleNamespace(**{k: materialize_double(v) for k, v in (prepared or {}).items()})
            pdouble.__cbg_raw__ = prepared
            ret = slf._adopt_prepared_summary(pdouble)
        return ret, st
    if func == "_restart_probe_extra":
        return NS["_restart_probe_extra"](), {}
    if func == "summary_view":
        return NS["summary_view"](inp.get("plan"), inp.get("originals"),
                                  inp.get("start", 0), inp.get("end", 0)), {}
    if func == "_tool_calls_by_id":
        return NS["_tool_calls_by_id"](inp.get("messages")), {}
    if func == "_tc_get":
        return NS["_tc_get"](materialize_double(inp.get("obj")), inp.get("key"),
                             inp.get("default", "")), {}
    if func == "_rewritten":
        return NS["_rewritten"](inp.get("msg"), inp.get("content")), {}
    if func == "archive_refs_in_text":
        return MEDIA_NS["archive_refs_in_text"](*(inp.get("texts") or [])), {}
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


def self_test(fixtures):
    """Drive every fixture live; flag script gaps and uniform AttributeError
    families naming a ported method / seam stub (binding bugs)."""
    names = set(FUNCS) | set(SEAMED_METHODS) | set(SEAM_GLOBALS)
    names |= {"stop_for_provider_context", "take", "logger.warning", "import_const",
              "drop_stale_api_content"}
    problems = []
    seen_funcs = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        seen_funcs.add(func)
        if func not in FUNCS:
            problems.append("case %d: func %r not in extraction map" % (i, func))
            continue
        out = run_case(func, rec.get("in") or {})
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = r.get("message", "")
            # binding-bug signature: a DOUBLE (SimpleNamespace / mixin) missing a
            # ported/seamed attribute. Scalar-carrier method-name errors ('int'
            # object has no attribute 'take') are the intentional non-carrier
            # fail branch, not a binding gap.
            if not (msg.startswith("'types.SimpleNamespace'") or msg.startswith("'CompactionWindowMixin'")):
                continue
            for nm in names:
                if ("'%s'" % nm) in msg:
                    problems.append("case %d [%s]: AttributeError names ported/seamed %r: %s"
                                    % (i, func, nm, msg))
                    break
    missing = set(FUNCS) - seen_funcs
    if missing:
        problems.append("funcs with no fixture: %s" % sorted(missing))
    return problems


def main():
    faulthandler.dump_traceback_later(120, exit=True)
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
    print("ORACLE context_budget_flow: %d cases, %d mismatches" % (n, mism))
    faulthandler.cancel_dump_traceback_later()
    if n == 0:
        return 1
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())