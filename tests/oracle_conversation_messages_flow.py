#!/usr/bin/env python3
"""Oracle for the transcript-contract FLOW port
(src/persistence/conversation_messages_flow.c).

Drives the LIVE persistence/conversation/messages.py defs (module helpers,
dataclass validators/converters/projections and the stamp_live mutation flow)
with SCRIPTED seams: json.loads (the _json_text decode) and the ReadCheck
_check callable are recording stubs fed the same scripts the C harness feeds
its universal seam. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace"), the final return ("ret") and the mutated
state ("state", stamp_live's messages).

The live defs are AST-extracted from messages.py (module constants seeded,
Assign AND AnnAssign, plain-Name targets only; every dataclass/enum exec'd
except the MessageRepository Protocol). RowVisibility is extracted from
persistence/core/conventions.py and PersistenceError from
persistence/core/errors.py. Every exec namespace is a registered sys.modules
module (py3.14). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE conversation_messages_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- Plain JSON numbers are Python ints; JSON bools are bools (bool IS an int
  instance, but type() is not int). A Python float travels as
  {"__float__": true, "v": <number>}; a Python tuple (where isinstance(tuple)
  gates read it) as {"__tuple__": true, "v": [...]}.
- A nested HeldMessage instance: {"__held__": true, "v": {<fields>}}; a
  BatchAppendProgress instance: {"__progress__": true, "v": {"outcomes":
  [...], "uids": [...]}}.
- Enum fields (RowOutcome.disposition, TranscriptRead.view,
  TranscriptRow.visibility) travel as the enum VALUE string; an invalid value
  is the live ValueError "'x' is not a valid <EnumName>" at materialization.
- Record specs are plain field dicts; absent keys take the dataclass default;
  absent required / unknown keys raise the live __init__ TypeErrors.
- stamp_live result: null | {"kind":"progress"|"batch"|"replace", ...}; the
  mutated messages list is out.state.messages.
- batch_progress error: {"attrs": {...}} (a SimpleNamespace double) or a plain
  JSON value (no attributes -> getattr default None).
- ReadCheck check param: "seam" -> the recording stub named "check"; any other
  value is a non-callable attribute (TypeError on call).
- Dataclass-instance returns project to asdict with enums -> .value; tuples
  fold to lists and integral floats to ints in canon.
- ResumeTooLarge.__init__ projects the constructed error to {"class",
  "message", "message_count", "limit", "scope", "is_persistence_error",
  "is_value_error"}; ReadCheck.__init__ to {"pinned": <value>}; the
  __post_init__ validators return null on success.
- Fixture domain: int() string literals are ASCII digits with optional +/-
  and single underscores between digits (plus ASCII whitespace padding);
  str() of non-strings covers int/bool/None only; content/metadata trees never
  carry literal "__float__"/"__tuple__"/"__held__"/"__progress__" keys;
  check-seam scripts are plain JSON values; json.loads scripts never carry
  __tuple__; no case combines a binding error with an invalid nested
  HeldMessage.
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from dataclasses import asdict, dataclass, field
from enum import Enum
from pathlib import Path
from typing import (Any, Callable, ContextManager, Iterator, Mapping,
                    MutableMapping, Optional, Protocol, Sequence, TypeAlias,
                    runtime_checkable)

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
MSG_SRC = os.path.join(Q, "persistence", "conversation", "messages.py")
CONV_SRC = os.path.join(Q, "persistence", "core", "conventions.py")
ERR_SRC = os.path.join(Q, "persistence", "core", "errors.py")

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


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def extract_class(src, path, name, ns):
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == name)
    exec_node(src, cls, ns, path)


# ── messages.py namespace (live defs + seeded constants) ──
def extract_messages():
    mod = new_module("cmf_oracle_messages")
    ns = mod.__dict__
    ns.update({"dataclass": dataclass, "field": field, "Enum": Enum, "json": json,
               "Any": Any, "Callable": Callable, "ContextManager": ContextManager,
               "Iterator": Iterator, "Mapping": Mapping, "MutableMapping": MutableMapping,
               "Optional": Optional, "Protocol": Protocol, "Sequence": Sequence,
               "TypeAlias": TypeAlias, "runtime_checkable": runtime_checkable,
               "os": os, "re": re})
    # NewType identity anchors (runtime NewType is a pass-through callable).
    ns["MessageId"] = lambda v: v
    ns["SessionId"] = lambda v: v
    ns["JsonValue"] = object()
    # sibling classes the module imports
    conv = open(CONV_SRC, encoding="utf-8").read()
    errs = open(ERR_SRC, encoding="utf-8").read()
    extract_class(conv, CONV_SRC, "RowVisibility", ns)
    extract_class(errs, ERR_SRC, "PersistenceError", ns)
    src = open(MSG_SRC, encoding="utf-8").read()
    seed_constants(src, ns, MSG_SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef):
            if node.name == "MessageRepository":
                continue  # the Protocol contract has no executable bodies
            exec_node(src, node, ns, MSG_SRC)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, MSG_SRC)
    # json is a per-case scripted seam; install a placeholder loads now.
    ns["json"] = types.SimpleNamespace(loads=None)
    required = ["NewMessage", "RowStamp", "RowOutcome", "BatchAppendResult", "RowUids",
                "BatchAppendProgress", "HeldMessage", "HeldCoverage", "ReplaceResult",
                "CompactionResult", "CooldownSnapshot", "ConversationRevision",
                "ResumeTooLarge", "DelegationDelivery", "ConversationMessage",
                "TranscriptRead", "TranscriptRow", "ReadCheck", "RowDisposition",
                "TranscriptView", "ReplaceMode", "RowVisibility", "PersistenceError",
                "_live_or_column", "_count", "_json_text", "batch_progress",
                "stamp_live", "_same", "_CONVERSATION_KEYS", "_CONVERSATION_FIELD_BY_KEY",
                "_AFTER_IDENTITY_KEYS", "_PRESENT_WHEN_NOT_NONE", "_TO_WIRE", "_FROM_WIRE",
                "_WIRE_ROW_KEYS", "DB_PERSISTED_MARKER_KEY"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_conversation_messages_flow: defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_messages()


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, Enum):
            return x.value
        if isinstance(x, bytes):
            return x.decode("latin-1")
        if isinstance(x, types.MethodType):
            return x.__func__.__name__
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def proj_data(obj):
    """asdict with enum members folded to their value (canon folds the rest)."""
    def conv(x):
        if isinstance(x, Enum):
            return x.value
        if isinstance(x, dict):
            return {k: conv(v) for k, v in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return conv(asdict(obj))


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


# ── per-case script backend (mirrors the C cm_call exactly) ──
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
    """__float__ -> float, __tuple__ -> tuple, recursively; other tags left."""
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(deep_mat(i) for i in v.get("v", []))
        return {k: deep_mat(val) for k, val in v.items()}
    if isinstance(v, list):
        return [deep_mat(i) for i in v]
    return v


def _held_seq(raw):
    """Materialize a retired/unresolved sequence: __held__ elements become real
    HeldMessage instances (their own validation runs NOW, before the outer
    construction); a list stays a list (fails the isinstance(tuple) gate)."""
    if isinstance(raw, (list, tuple)):
        elems = [mat_held(e["v"]) if isinstance(e, dict) and e.get("__held__") is True else e
                 for e in raw]
        return tuple(elems) if isinstance(raw, tuple) else elems
    return raw


def mat_held(spec):
    fields = {}
    for k, v in spec.items():
        raw = deep_mat(v)
        fields[k] = _held_seq(raw) if k == "retired_durable_rows" else raw
    return NS["HeldMessage"](**fields)


def mat_coverage(spec):
    fields = {}
    for k, v in spec.items():
        raw = deep_mat(v)
        fields[k] = _held_seq(raw) if k == "unresolved" else raw
    return NS["HeldCoverage"](**fields)


def mat_row_outcome(spec):
    fields = {k: deep_mat(v) for k, v in spec.items()}
    if "disposition" in fields:
        fields["disposition"] = NS["RowDisposition"](fields["disposition"])
    return NS["RowOutcome"](**fields)


def mat_row_stamp(spec):
    return NS["RowStamp"](**{k: deep_mat(v) for k, v in spec.items()})


def mat_row_uids(spec):
    return NS["RowUids"](**{k: deep_mat(v) for k, v in spec.items()})


def mat_progress(spec):
    fields = {}
    for k, v in spec.items():
        if k == "outcomes":
            fields[k] = tuple(mat_row_outcome(o) for o in v)
        elif k == "uids":
            fields[k] = tuple(mat_row_uids(u) for u in v)
        else:
            fields[k] = deep_mat(v)
    return NS["BatchAppendProgress"](**fields)


def mat_result(spec):
    if spec is None:
        return None
    kind = spec.get("kind")
    body = {k: v for k, v in spec.items() if k != "kind"}
    if kind == "progress":
        return mat_progress(body)
    if kind == "batch":
        fields = {k: (tuple(mat_row_outcome(o) for o in v) if k == "outcomes" else deep_mat(v))
                  for k, v in body.items()}
        return NS["BatchAppendResult"](**fields)
    if kind == "replace":
        fields = {k: (tuple(mat_row_stamp(r) for r in v) if k == "rows" else deep_mat(v))
                  for k, v in body.items()}
        return NS["ReplaceResult"](**fields)
    raise RuntimeError(f"unknown result kind {kind!r}")


def mat_new_message(spec):
    return NS["NewMessage"](**{k: deep_mat(v) for k, v in spec.items()})


def mat_conversation_message(spec):
    return NS["ConversationMessage"](**{k: deep_mat(v) for k, v in spec.items()})


def mat_transcript_read(spec):
    fields = {k: deep_mat(v) for k, v in spec.items()}
    if "view" in fields:
        fields["view"] = NS["TranscriptView"](fields["view"])
    return NS["TranscriptRead"](**fields)


def mat_transcript_row(spec):
    fields = {k: deep_mat(v) for k, v in spec.items()}
    if "visibility" in fields:
        fields["visibility"] = NS["RowVisibility"](fields["visibility"])
    return NS["TranscriptRow"](**fields)


def mat_read_check(spec, ctx):
    args = []
    if "check" in spec:
        c = spec["check"]
        args.append(rec(ctx, "check", lambda: {}, build=deep_mat) if c == "seam" else deep_mat(c))
    kwargs = {"pinned": deep_mat(spec["pinned"])} if "pinned" in spec else {}
    return NS["ReadCheck"](*args, **kwargs)


def _seq_fields(spec, key, mat):
    return {k: (tuple(mat(r) for r in v) if k == key else deep_mat(v))
            for k, v in spec.items()}


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = {}
    ctx = Ctx(inp.get("io"))
    NS["json"].loads = rec(ctx, "json.loads", lambda s: {"s": s}, build=deep_mat)
    try:
        if func == "_live_or_column":
            ret = NS["_live_or_column"](deep_mat(args["message"]), args["live_key"], args["column"])
        elif func == "_count":
            ret = NS["_count"](deep_mat(args["value"]))
        elif func == "_json_text":
            ret = NS["_json_text"](deep_mat(args["value"]), deep_mat(args["empty"]))
        elif func == "_same":
            ret = NS["_same"](deep_mat(args["value"]))
        elif func == "NewMessage.__post_init__":
            mat_new_message(args["fields"])
            ret = None
        elif func == "NewMessage.from_live":
            ret = proj_data(NS["NewMessage"].from_live(deep_mat(args["message"])))
        elif func == "RowOutcome.as_row_annotation":
            ret = mat_row_outcome(args["outcome"]).as_row_annotation()
        elif func == "BatchAppendResult.inserted":
            ret = NS["BatchAppendResult"](**_seq_fields(args, "outcomes", mat_row_outcome)).inserted
        elif func == "batch_progress":
            espec = args.get("error")
            if isinstance(espec, dict) and "attrs" in espec:
                attrs = {}
                for k, v in espec["attrs"].items():
                    if isinstance(v, dict) and v.get("__progress__") is True:
                        attrs[k] = mat_progress(v["v"])
                    else:
                        attrs[k] = deep_mat(v)
                error = types.SimpleNamespace(**attrs)
            else:
                error = deep_mat(espec)
            prog = NS["batch_progress"](error)
            ret = proj_data(prog) if prog is not None else None
        elif func == "HeldMessage.__post_init__":
            mat_held(args["fields"])
            ret = None
        elif func == "HeldCoverage.__post_init__":
            mat_coverage(args["fields"])
            ret = None
        elif func == "ReplaceResult.survivor_ids":
            ret = NS["ReplaceResult"](**_seq_fields(args, "rows", mat_row_stamp)).survivor_ids
        elif func == "stamp_live":
            msgs = [deep_mat(m) for m in args["messages"]]
            state["messages"] = msgs  # capture before the call: a raise leaves the live mutation history
            NS["stamp_live"](msgs, mat_result(args.get("result")))
            ret = None
        elif func == "CompactionResult.row_ids":
            ret = NS["CompactionResult"](**_seq_fields(args, "rows", mat_row_stamp)).row_ids
        elif func == "CompactionResult.row_snapshots":
            ret = NS["CompactionResult"](**_seq_fields(args, "rows", mat_row_stamp)).row_snapshots
        elif func == "CooldownSnapshot.__post_init__":
            NS["CooldownSnapshot"](**{k: deep_mat(v) for k, v in args["fields"].items()})
            ret = None
        elif func == "ConversationRevision.as_tuple":
            ret = NS["ConversationRevision"](**{k: deep_mat(v) for k, v in args["fields"].items()}).as_tuple()
        elif func == "ResumeTooLarge.__init__":
            exc = NS["ResumeTooLarge"](args["message_count"], args["limit"],
                                       args.get("scope", "across its lineage"))
            ret = {"class": type(exc).__name__, "message": str(exc),
                   "message_count": exc.message_count, "limit": exc.limit, "scope": exc.scope,
                   "is_persistence_error": isinstance(exc, NS["PersistenceError"]),
                   "is_value_error": isinstance(exc, ValueError)}
        elif func == "DelegationDelivery.__post_init__":
            NS["DelegationDelivery"](**{k: deep_mat(v) for k, v in args["fields"].items()})
            ret = None
        elif func == "DelegationDelivery.identity":
            ret = NS["DelegationDelivery"](**{k: deep_mat(v) for k, v in args["fields"].items()}).identity
        elif func == "ConversationMessage.as_openai_message":
            ret = mat_conversation_message(args["fields"]).as_openai_message()
        elif func == "ConversationMessage.from_openai_message":
            ret = proj_data(NS["ConversationMessage"].from_openai_message(deep_mat(args["message"])))
        elif func == "TranscriptRead.__post_init__":
            mat_transcript_read(args["fields"])
            ret = None
        elif func == "TranscriptRow.as_wire_row":
            ret = mat_transcript_row(args["fields"]).as_wire_row()
        elif func == "ReadCheck.__init__":
            ret = {"pinned": mat_read_check(args, ctx).pinned}
        elif func == "ReadCheck.__call__":
            ret = mat_read_check(args, ctx)()
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


KINDS = ["_live_or_column", "_count", "_json_text", "_same",
         "NewMessage.__post_init__", "NewMessage.from_live",
         "RowOutcome.as_row_annotation", "BatchAppendResult.inserted", "batch_progress",
         "HeldMessage.__post_init__", "HeldCoverage.__post_init__",
         "ReplaceResult.survivor_ids", "stamp_live", "CompactionResult.row_ids",
         "CompactionResult.row_snapshots", "CooldownSnapshot.__post_init__",
         "ConversationRevision.as_tuple", "ResumeTooLarge.__init__",
         "DelegationDelivery.__post_init__", "DelegationDelivery.identity",
         "ConversationMessage.as_openai_message", "ConversationMessage.from_openai_message",
         "TranscriptRead.__post_init__", "TranscriptRow.as_wire_row",
         "ReadCheck.__init__", "ReadCheck.__call__"]

# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = ["json.loads", "check"]


def cross_check():
    csrc = open(_REPO / "src" / "persistence" / "conversation_messages_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_conversation_messages_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_conversation_messages_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "conversation_messages_flow.jsonl"
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
            print(f"FATAL oracle_conversation_messages_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unknown_funcs = fix_funcs - set(KINDS)
        if unknown_funcs:
            print(f"FATAL oracle_conversation_messages_flow: fixture funcs without a drive: {sorted(unknown_funcs)}")
            sys.exit(2)
        untested = set(KINDS) - fix_funcs
        if untested:
            print(f"FATAL oracle_conversation_messages_flow: drive funcs without a fixture case: {sorted(untested)}")
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
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE conversation_messages_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fix = _REPO / "tests" / "fixtures" / "conversation_messages_flow.jsonl"
    n = bad = 0
    counts = {}
    # names whose appearance in an AttributeError spec signals a binding bug
    # (a ported method or seam stub read as missing on a double).
    watch = ["from_live", "as_row_annotation", "as_openai_message", "from_openai_message",
             "as_wire_row", "as_tuple", "survivor_ids", "row_ids", "row_snapshots",
             "identity", "inserted", "stamp_live", "batch_progress", "_json_text",
             "_live_or_column", "_count", "_same", "loads", "json", "check", "_check",
             "pinned", "HeldMessage", "NewMessage", "ConversationMessage"]
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
    print(f"SELFTEST conversation_messages_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()