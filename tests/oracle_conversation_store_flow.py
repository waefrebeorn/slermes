#!/usr/bin/env python3
"""Oracle for the conversation-store carried-row coverage FLOW port
(src/persistence/conversation_store_flow.c).

The named target persistence/conversation/store.py does not exist in the quarry;
it resolves to the real defining module
    persistence/sqlite/conversation/_store/coverage.py
("Carried-row coverage proofs for compaction"). This oracle drives the LIVE
coverage.py defs with SCRIPTED seams: every collaborator the module imports
(_row_identity, _parse_tool_calls, _decode_content, _encode_content,
_loaded_view_content, _serialized_message_row, message_uid_or_none,
_is_image_part, coerce_epoch, strip_leading_message_timestamps,
transcript_row_snapshot, _placeholders, _id_chunks) and the sqlite connection
(conn.execute) are recording stubs fed the same scripts the C harness feeds its
universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace"), the final return ("ret") and the mutated
state ("state"; coverage never mutates inputs, so always {}).

The live defs are AST-extracted from coverage.py (FunctionDef nodes only; the
top-level imports are skipped and their names seeded). Constants are seeded.
Function-body `from ... import ...` statements resolve against sys.modules stub
modules (session_rules.turns, _store.rules, _store.transcript_repair). Every exec
namespace is a registered sys.modules module (py3.14). The heavy quarry import
chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE conversation_store_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are Python ints; JSON bools are bools. A Python float
  travels as {"__float__": true, "v": <number>}; a Python tuple as
  {"__tuple__": true, "v": [...]}. Row identities (_row_identity seam returns)
  travel as plain JSON arrays (lists) so identity equality is list==list.
- A sqlite3.Row is a plain JSON object; conn.execute is a seam returning the
  scripted row list and .fetchall() is inert on both sides.
- holder is an opaque token passed through to seams (fixtures use null).
- Sets return as SORTED arrays (canon sorts set/frozenset on both sides); tuples
  fold to arrays and integral floats to ints in canon. Dict key order is
  irrelevant (canon compares parsed JSON objects by content).
- Fixture domain: identity components are str/int/float/None/list (no bool, no
  nested tuple); timestamps are int/float/None; text is ASCII; covered/tail/row
  ids are positive ints; no literal "__float__"/"__tuple__" keys inside
  content/row trees.
"""
import ast
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections import Counter
from pathlib import Path
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
COV_SRC = os.path.join(Q, "persistence", "sqlite", "conversation", "_store", "coverage.py")

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
DB_ROW_SNAPSHOT = "_db_row_snapshot"
ABSORBED_ROW_IDS = "_absorbed_row_ids"
MERGED_DURABLE_ROWS = "_merged_durable_rows"
UNNAMED_DURABLE_ROWS = "_unnamed_durable_rows"
RETIRED_DURABLE_ROWS = "_retired_durable_rows"
RETIRED_ROW = "_retired_row"
OWN_ROW = "_own_row"
_DB_PERSISTED_MARKER = "_db_persisted"
REPLAY_SIDECARS = ("reasoning", "reasoning_content", "reasoning_details",
                   "codex_reasoning_items", "codex_message_items")
_SQL_IN_CHUNK = 900

KINDS = ["_text_of", "_holds_as_block", "_images_replaced", "_carried_tail_slots",
         "_uncounted_merged_rows", "_tail_originals", "_index_held_dicts",
         "_replays_as", "_copy_of_current_row", "_claim_stored_copy",
         "_rewrite_explains", "_rewritten_carry", "_uids_active_outside",
         "_carried_tail_ids", "_matching_active_ids", "_matching_retired_ids",
         "_resolve_carried_row_ids", "_merged_user_run", "_proved_coverage"]

# ── coverage.py namespace (live defs + seeded constants/collaborators) ──
COLLAB_ARGS = {
    "_row_identity": lambda role, content, tool_call_id, tool_calls: {
        "role": role, "content": content, "tool_call_id": tool_call_id, "tool_calls": tool_calls},
    "_parse_tool_calls": lambda tool_calls: {"tool_calls": tool_calls},
    "_decode_content": lambda content: {"content": content},
    "_encode_content": lambda content: {"content": content},
    "_loaded_view_content": lambda role, content: {"role": role, "content": content},
    "_serialized_message_row": lambda holder, session_id, msg, message_timestamp: {
        "holder": holder, "session_id": session_id, "msg": msg, "message_timestamp": message_timestamp},
    "message_uid_or_none": lambda msg: {"msg": msg},
    "_is_image_part": lambda part: {"part": part},
    "coerce_epoch": lambda value, *, session_id=None, field="timestamp": {
        "value": value, "session_id": session_id, "field": field},
    "strip_leading_message_timestamps": lambda content, tz=None: {"content": content, "tz": tz},
    "transcript_row_snapshot": lambda row: {"row": row},
    "_placeholders": lambda items: {"items": items},
    "_id_chunks": lambda ids, size=_SQL_IN_CHUNK: {"ids": ids, "size": size},
}


def extract_coverage():
    mod = new_module("cst_oracle_coverage")
    ns = mod.__dict__
    ns.update({"Counter": Counter, "Any": Any, "Optional": Optional, "StoreHolder": object})
    ns.update({"MESSAGE_UID": MESSAGE_UID, "DB_ROW_SNAPSHOT": DB_ROW_SNAPSHOT,
               "ABSORBED_ROW_IDS": ABSORBED_ROW_IDS, "MERGED_DURABLE_ROWS": MERGED_DURABLE_ROWS,
               "UNNAMED_DURABLE_ROWS": UNNAMED_DURABLE_ROWS, "RETIRED_DURABLE_ROWS": RETIRED_DURABLE_ROWS,
               "RETIRED_ROW": RETIRED_ROW, "OWN_ROW": OWN_ROW, "_DB_PERSISTED_MARKER": _DB_PERSISTED_MARKER})
    for name in COLLAB_ARGS:
        ns[name] = None   # per-case recording stubs installed in drive()
    src = open(COV_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, COV_SRC)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_conversation_store_flow: defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_coverage()

# ── sys.modules stubs for the function-body runtime imports ──
TURNS_MOD = new_module("session_rules.turns")
RULES_MOD = new_module("persistence.sqlite.conversation._store.rules")
REPAIR_MOD = new_module("persistence.sqlite.conversation._store.transcript_repair")
TURNS_MOD.MESSAGE_UID = MESSAGE_UID
TURNS_MOD.MERGED_DURABLE_ROWS = MERGED_DURABLE_ROWS
TURNS_MOD.UNNAMED_DURABLE_ROWS = UNNAMED_DURABLE_ROWS
TURNS_MOD.RETIRED_DURABLE_ROWS = RETIRED_DURABLE_ROWS
TURNS_MOD.RETIRED_ROW = RETIRED_ROW
TURNS_MOD.OWN_ROW = OWN_ROW
TURNS_MOD._DB_PERSISTED_MARKER = _DB_PERSISTED_MARKER
RULES_MOD.DB_ROW_SNAPSHOT = DB_ROW_SNAPSHOT
RULES_MOD.ABSORBED_ROW_IDS = ABSORBED_ROW_IDS
REPAIR_MOD.REPLAY_SIDECARS = REPLAY_SIDECARS


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, dict):
            if x.get("__float__") is True:
                f = float(x.get("v", 0))
                return int(f) if f.is_integer() else f
            if x.get("__tuple__") is True:
                return [conv(i) for i in x.get("v", [])]
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "OSError": OSError, "StopIteration": StopIteration,
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


# ── per-case script backend (mirrors the C cst_call exactly) ──
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


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── value materialization ──
def deep_mat(v):
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(deep_mat(i) for i in v.get("v", []))
        return {k: deep_mat(val) for k, val in v.items()}
    if isinstance(v, list):
        return [deep_mat(i) for i in v]
    return v


def mat_by_row(spec):
    """by_row keys are int row ids live; JSON carries them as strings."""
    return {int(k): deep_mat(v) for k, v in (spec or {}).items()}


class CursorDouble:
    def __init__(self, rows):
        self._rows = rows

    def fetchall(self):
        return self._rows


class ConnDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def execute(self, sql, params=None):
        rows = self._ctx.call("conn.execute",
                              {"sql": sql, "params": list(params) if params is not None else None})
        return CursorDouble(deep_mat(rows))


def install_stubs(ctx):
    for name, args_fn in COLLAB_ARGS.items():
        NS[name] = rec(ctx, name, args_fn, build=deep_mat)
    # runtime-imported collaborators bind from the sys.modules stubs
    RULES_MOD._is_image_part = NS["_is_image_part"]
    RULES_MOD.strip_leading_message_timestamps = NS["strip_leading_message_timestamps"]
    RULES_MOD.coerce_epoch = NS["coerce_epoch"]
    REPAIR_MOD.transcript_row_snapshot = NS["transcript_row_snapshot"]
    TURNS_MOD.message_uid_or_none = NS["message_uid_or_none"]


# ── per-case drive ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    state = {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    install_stubs(ctx)

    def mat(k, default=None):
        return deep_mat(args.get(k, default))

    holder = mat("holder")
    conn = ConnDouble(ctx)
    try:
        if func == "_text_of":
            ret = NS["_text_of"](mat("content"))
        elif func == "_holds_as_block":
            ret = NS["_holds_as_block"](mat("inner"), mat("outer"))
        elif func == "_images_replaced":
            ret = NS["_images_replaced"](mat("stored"), mat("held"))
        elif func == "_carried_tail_slots":
            ret = NS["_carried_tail_slots"](mat("carried"), mat("compacted_count"), mat("tail_count"))
        elif func == "_uncounted_merged_rows":
            ret = NS["_uncounted_merged_rows"](mat("tail"))
        elif func == "_tail_originals":
            ret = NS["_tail_originals"](mat("covered_active"), mat("tail_count"), mat("merged_away"))
        elif func == "_index_held_dicts":
            ret = NS["_index_held_dicts"](holder, mat("compacted_messages"))
        elif func == "_replays_as":
            ret = NS["_replays_as"](holder, mat("row"), mat("held"), mat("same_content"))
        elif func == "_copy_of_current_row":
            ret = NS["_copy_of_current_row"](mat("row"), mat("held"))
        elif func == "_claim_stored_copy":
            ret = NS["_claim_stored_copy"](holder, mat("row"), mat("compacted_messages"),
                                           mat_by_row(args.get("by_row")), mat("by_uid"), mat("claimed"))
        elif func == "_rewrite_explains":
            ret = NS["_rewrite_explains"](holder, mat("row"), mat("held"))
        elif func == "_rewritten_carry":
            ret = NS["_rewritten_carry"](holder, mat("row"), mat("row_uid"), mat("compacted_messages"),
                                         mat_by_row(args.get("by_row")), mat("by_uid"), mat("claimed"))
        elif func == "_uids_active_outside":
            ret = NS["_uids_active_outside"](conn, mat("rows"), mat("tail_ids"), mat("named_uids"))
        elif func == "_carried_tail_ids":
            ret = NS["_carried_tail_ids"](holder, conn, mat("tail_ids"), mat("compacted_messages"))
        elif func == "_matching_active_ids":
            ret = NS["_matching_active_ids"](holder, conn, mat("session_id"), mat("message"))
        elif func == "_matching_retired_ids":
            ret = NS["_matching_retired_ids"](holder, conn, mat("session_id"), mat("retired"), mat("watermark"))
        elif func == "_resolve_carried_row_ids":
            ret = NS["_resolve_carried_row_ids"](holder, conn, mat("session_id"), mat("carried_messages"))
        elif func == "_merged_user_run":
            ret = NS["_merged_user_run"](holder, conn, mat("session_id"), mat("message"), mat("watermark"))
        elif func == "_proved_coverage":
            ret = NS["_proved_coverage"](holder, conn, mat("session_id"), mat("covered_ids"),
                                         mat("unresolved_held"), mat("watermark"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = list(COLLAB_ARGS.keys()) + ["conn.execute"]


def cross_check():
    csrc = open(_REPO / "src" / "persistence" / "conversation_store_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_conversation_store_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_conversation_store_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "conversation_store_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        fix_funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_conversation_store_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unknown_funcs = fix_funcs - set(KINDS)
        if unknown_funcs:
            print(f"FATAL oracle_conversation_store_flow: fixture funcs without a drive: {sorted(unknown_funcs)}")
            sys.exit(2)
        untested = set(KINDS) - fix_funcs
        if untested:
            print(f"FATAL oracle_conversation_store_flow: drive funcs without a fixture case: {sorted(untested)}")
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
    for line in sys.stdin:
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
    print(f"ORACLE conversation_store_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "conversation_store_flow.jsonl"
    n = bad = 0
    counts = {}
    watch = KINDS + list(COLLAB_ARGS.keys()) + ["conn", "execute", "fetchall", "holder"]
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
    print(f"SELFTEST conversation_store_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
