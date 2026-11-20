#!/usr/bin/env python3
"""Oracle for the conversation-store message-identity codec FLOW port
(src/persistence/store_identity_flow.c).

Defining module: persistence/sqlite/conversation/_store/identity.py ("The codec
between the live message-identity keys and the identity columns of a message
row."). All 12 identity.py defs are driven LIVE, plus the 2 unclaimed pure
leaves identity.py imports, credited to their REAL defining modules:
_json_or (persistence/sqlite/_state/common.py) and coalesce_tool_call_id
(session_rules/turns.py, with its _tc_field helper) — 14 functions. The
CLAIMED sibling uid_list (session_rules/turns.py, ported by store_rules_flow)
is SEAMED, not re-driven.

The live defs are AST-extracted (FunctionDef nodes; the whitelisted constant
Assigns of turns.py/rules.py exec in source order). Top-level imports are
skipped and their names seeded (json is the REAL stdlib module — json.loads /
json.dumps are pure and transcribed in the C port, not seamed). Every exec
namespace is a registered sys.modules module (py3.14). The heavy quarry import
chain is never imported.

SCRIPTED seams: every external collaborator is a recording stub fed the same
scripts the C harness feeds its universal seam: uid_list (claimed sibling),
logger.warning (common.py's deserialization-failure warning, single positional
arg). Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"}
in "trace"), the final return ("ret") and the mutated dict params ("state",
keyed by parameter name). Ported leaves call each other directly on both sides
(no seam).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE store_identity_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}
  (OSError adds "errno"). Plain JSON numbers are Python ints; JSON bools are
  bools. A Python float travels as {"__float__": true, "v": <number>}; a tuple
  as {"__tuple__": true, "v": [...]}. The C strict parser tags a parsed number
  float iff its literal carries '.'/'e'/'E' (json.loads parse_int/parse_float).
- Mutating funcs (_fill_missing_tool_call_uids, _restore_row_identity,
  _restore_identity_columns) mutate msg in place; state echoes the mutated msg
  (row is read-only). Tuples fold to arrays and integral floats to ints in
  canon. Dict key order is irrelevant to canon; the trace is ordered.
- Fixture domain (ASCII scope): strings ASCII (control chars ride \\u00xx);
  uids non-empty ASCII strings; uid_list seam results are lists of non-empty
  ASCII strings; uid-map values non-empty str or non-empty list of non-empty
  str; JSON texts within depth 32, int magnitude <= 2^53, finite floats; NO
  NaN/Infinity literals, NO lone-surrogate \\u escapes, NO bytes/bytearray,
  NO non-ASCII whitespace in strip positions, NO quote/backslash KeyError
  keys. identity.py has NO except-Exception site: every seam raise propagates
  (BaseException included).
"""
import ast
import json
import math
import os
import sys
import textwrap
import types
import __future__ as _fut
from collections.abc import Mapping, MutableMapping
from pathlib import Path
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
IDENTITY_SRC = os.path.join(Q, "persistence", "sqlite", "conversation", "_store", "identity.py")
COMMON_SRC = os.path.join(Q, "persistence", "sqlite", "_state", "common.py")
TURNS_SRC = os.path.join(Q, "session_rules", "turns.py")
RULES_SRC = os.path.join(Q, "persistence", "sqlite", "conversation", "_store", "rules.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


# ── seeded constants (as the quarry defines them) ──
MESSAGE_UID = "message_uid"
ABSORBED_MESSAGE_UIDS = "_absorbed_message_uids"
TOOL_CALL_UIDS = "_tool_call_uids"
TOOL_CALL_UID = "_tool_call_uid"

IDENTITY_KINDS = ["_live_or_column", "_uid_list", "_uid_map", "_tool_call_uid_map",
                  "_absorbed_uids_json", "_tool_call_uids_json", "_tool_call_uid_or_none",
                  "_live_extends", "_fill_missing_tool_call_uids", "_restore_row_identity",
                  "_restore_identity_columns", "_stable_tool_key"]
LEAF_KINDS = ["_json_or", "coalesce_tool_call_id"]
KINDS = IDENTITY_KINDS + LEAF_KINDS
TURNS_CONSTS = {"MESSAGE_UID", "ABSORBED_MESSAGE_UIDS", "TOOL_CALL_UIDS"}
TURNS_KINDS = ["coalesce_tool_call_id", "_tc_field"]
RULES_CONSTS = {"TOOL_CALL_UID"}
COMMON_KINDS = ["_json_or"]


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            if x.get("__float__") is True:
                f = float(x.get("v", 0))
                return int(f) if math.isfinite(f) and f.is_integer() else f
            if x.get("__tuple__") is True:
                return [conv(i) for i in x.get("v", [])]
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float):
            return int(x) if math.isfinite(x) and x.is_integer() else x
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


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    en = spec.get("errno")
    if en is not None and issubclass(cls, OSError):
        return cls(en, msg)
    return cls(msg)


def proj_exc(exc):
    d = {"__raise__": type(exc).__name__, "message": str(exc)}
    if isinstance(exc, OSError) and exc.errno is not None:
        d["errno"] = exc.errno
    return d


# ── per-case script backend (mirrors the C si_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


# ── value materialization / transport ──
def deep_mat(v, ctx=None):
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(deep_mat(i, ctx) for i in v.get("v", []))
        return {k: deep_mat(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [deep_mat(i, ctx) for i in v]
    return v


def transport(v):
    if isinstance(v, float):
        return {"__float__": True, "v": v}
    if isinstance(v, tuple):
        return {"__tuple__": True, "v": [transport(i) for i in v]}
    if isinstance(v, dict):
        return {k: transport(x) for k, x in v.items()}
    if isinstance(v, list):
        return [transport(i) for i in v]
    return v


class LoggerDouble:
    """common.py's logger: warning(warning) — one positional string."""
    def __init__(self, ctx):
        self._ctx = ctx

    def warning(self, msg, *args):
        return self._ctx.call("logger.warning", {"message": msg})


# ── seam registry (name -> live call; args shapes mirrored in the C port) ──
SEAM_NAMES = ["uid_list", "logger.warning"]


# ── turns.py leaf namespace (live defs + whitelisted constants) ──
def extract_turns():
    mod = new_module("si_oracle_turns")
    ns = mod.__dict__
    ns.update({"Any": Any, "Optional": Optional, "Mapping": Mapping,
               "MutableMapping": MutableMapping})
    src = open(TURNS_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            if any(t.id in TURNS_CONSTS for t in node.targets):
                exec_node(src, node, ns, TURNS_SRC)
        elif isinstance(node, ast.FunctionDef) and node.name in TURNS_KINDS:
            exec_node(src, node, ns, TURNS_SRC)
    missing = [k for k in TURNS_KINDS if k not in ns] + [c for c in TURNS_CONSTS if c not in ns]
    if missing:
        print(f"FATAL oracle_store_identity_flow: turns defs missing: {missing}")
        sys.exit(2)
    for name, want in (("MESSAGE_UID", MESSAGE_UID),
                       ("ABSORBED_MESSAGE_UIDS", ABSORBED_MESSAGE_UIDS),
                       ("TOOL_CALL_UIDS", TOOL_CALL_UIDS)):
        if ns[name] != want:
            print(f"FATAL oracle_store_identity_flow: turns constant drift: {name}")
            sys.exit(2)
    return ns


NS_TURNS = extract_turns()


# ── rules.py constant namespace (TOOL_CALL_UID only) ──
def extract_rules_const():
    mod = new_module("si_oracle_rules")
    ns = mod.__dict__
    src = open(RULES_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            if any(t.id in RULES_CONSTS for t in node.targets):
                exec_node(src, node, ns, RULES_SRC)
    missing = [c for c in RULES_CONSTS if c not in ns]
    if missing:
        print(f"FATAL oracle_store_identity_flow: rules constants missing: {missing}")
        sys.exit(2)
    if ns["TOOL_CALL_UID"] != TOOL_CALL_UID:
        print("FATAL oracle_store_identity_flow: rules constant drift: TOOL_CALL_UID")
        sys.exit(2)
    return ns


NS_RULES = extract_rules_const()


# ── common.py leaf namespace (_json_or live; json real, logger per-case) ──
def extract_common():
    mod = new_module("si_oracle_common")
    ns = mod.__dict__
    ns.update({"Any": Any, "json": json, "logger": None})
    src = open(COMMON_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in COMMON_KINDS:
            exec_node(src, node, ns, COMMON_SRC)
    missing = [k for k in COMMON_KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_store_identity_flow: common defs missing: {missing}")
        sys.exit(2)
    return ns


NS_COMMON = extract_common()


# ── identity.py namespace (live defs + seeded constants/collaborators) ──
def extract_identity():
    mod = new_module("si_oracle_identity")
    ns = mod.__dict__
    ns.update({"Any": Any, "Optional": Optional, "Mapping": Mapping,
               "MutableMapping": MutableMapping, "json": json,
               "MESSAGE_UID": NS_TURNS["MESSAGE_UID"],
               "ABSORBED_MESSAGE_UIDS": NS_TURNS["ABSORBED_MESSAGE_UIDS"],
               "TOOL_CALL_UIDS": NS_TURNS["TOOL_CALL_UIDS"],
               "TOOL_CALL_UID": NS_RULES["TOOL_CALL_UID"],
               "uid_list": None,  # claimed sibling: per-case recorder
               "coalesce_tool_call_id": NS_TURNS["coalesce_tool_call_id"],
               "_tc_field": NS_TURNS["_tc_field"],
               "_json_or": NS_COMMON["_json_or"]})
    src = open(IDENTITY_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, IDENTITY_SRC)
    missing = [k for k in IDENTITY_KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_store_identity_flow: identity defs missing: {missing}")
        sys.exit(2)
    return ns


NS_IDENTITY = extract_identity()


def install(ctx):
    NS_COMMON["logger"] = LoggerDouble(ctx)

    def uid_list_rec(value):
        raw = ctx.call("uid_list", {"value": transport(value)})
        return deep_mat(raw, ctx)

    NS_IDENTITY["uid_list"] = uid_list_rec


# ── per-case drive ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    install(ctx)

    def mat(k, default=None):
        return deep_mat(args.get(k, default), ctx)

    mut = {}
    try:
        if func == "_live_or_column":
            ret = NS_IDENTITY["_live_or_column"](mat("msg"), args.get("live_key", ""),
                                                 args.get("column", ""))
        elif func == "_uid_list":
            ret = NS_IDENTITY["_uid_list"](mat("value"))
        elif func == "_uid_map":
            ret = NS_IDENTITY["_uid_map"](mat("value"))
        elif func == "_tool_call_uid_map":
            ret = NS_IDENTITY["_tool_call_uid_map"](mat("msg"))
        elif func == "_absorbed_uids_json":
            ret = NS_IDENTITY["_absorbed_uids_json"](mat("msg"))
        elif func == "_tool_call_uids_json":
            ret = NS_IDENTITY["_tool_call_uids_json"](mat("msg"))
        elif func == "_tool_call_uid_or_none":
            ret = NS_IDENTITY["_tool_call_uid_or_none"](mat("msg"))
        elif func == "_live_extends":
            ret = NS_IDENTITY["_live_extends"](mat("live_uid"), mat("stored_uid"))
        elif func == "_fill_missing_tool_call_uids":
            mut["msg"] = mat("msg")
            ret = NS_IDENTITY["_fill_missing_tool_call_uids"](mat("row"), mut["msg"])
        elif func == "_restore_row_identity":
            mut["msg"] = mat("msg")
            ret = NS_IDENTITY["_restore_row_identity"](mat("row"), mut["msg"])
        elif func == "_restore_identity_columns":
            mut["msg"] = mat("msg")
            ret = NS_IDENTITY["_restore_identity_columns"](mat("row"), mut["msg"])
        elif func == "_stable_tool_key":
            ret = NS_IDENTITY["_stable_tool_key"](mat("row"))
        elif func == "_json_or":
            ret = NS_COMMON["_json_or"](mat("raw"), mat("fallback"),
                                        args.get("warning", ""))
        elif func == "coalesce_tool_call_id":
            ret = NS_TURNS["coalesce_tool_call_id"](mat("tc"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {k: transport(v) for k, v in mut.items()}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
def cross_check():
    csrc = open(_REPO / "src" / "persistence" / "store_identity_flow.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_store_identity_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "store_identity_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        counts = {}
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_store_identity_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unknown_funcs = set(counts) - set(KINDS)
        if unknown_funcs:
            print(f"FATAL oracle_store_identity_flow: fixture funcs without a drive: {sorted(unknown_funcs)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_store_identity_flow: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_store_identity_flow: funcs with <5 fixture cases: {thin}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


def main():
    cross_check()
    n = m = shown = 0
    data = sys.stdin.buffer.read().decode("utf-8")
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
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE store_identity_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "store_identity_flow.jsonl"
    n = bad = 0
    counts = {}
    # Watch for AttributeErrors naming a SEAM or a ported func (signs of a
    # misbound stub). The .get-on-non-dict AttributeError is LEGITIMATE live
    # behavior this port reproduces (names 'get', not watched).
    watch = KINDS + SEAM_NAMES + ["warning"]
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        is_gap = isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
            str(r.get("message", "")).startswith(("no script", "script exhausted"))
        if case.get("fail_closed"):
            if not is_gap:
                bad += 1
                print(f"SELFTEST FAIL-CLOSED MISSING {func} case {n}: expected a seam RuntimeError, got {json.dumps(r, ensure_ascii=False)[:200]}")
        elif is_gap:
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            hit = [w for w in watch if f"'{w}'" in msg]
            if hit:
                bad += 1
                print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg} (names {hit})")
    print(f"SELFTEST store_identity_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
