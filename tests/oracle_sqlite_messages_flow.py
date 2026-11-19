#!/usr/bin/env python3
"""Oracle for the persistence/sqlite/conversation/messages.py FLOW + leaf ports
(src/persistence/sqlite_messages_flow.c).

Drives the LIVE SQLiteMessageRepository methods and module leaves with SCRIPTED
seams: the target module's leaves + the whole repository class are AST-extracted
from the quarry, the contract records/enums are extracted from
persistence/conversation/messages.py, the error family from
persistence/core/errors.py, RowVisibility from persistence/core/conventions.py,
the identity NewTypes from persistence/core/identity.py, the turn markers from
session_rules/turns.py and the row-state keys from _store/rules.py. Every live
collaborator outside the ported set (the _store.* verbs, refusals,
classify_persistence_error, message_uid_or_none, _parse_tool_calls,
_stamp_tool_call_uids, _append_messages_in_transaction,
_execute_transcript_write, _active_logical_message_row,
transcript_row_snapshot, _restore_row_identity, from_openai_message, the
timeline module, the read-ctx connection) is replaced by recording stubs
returning the SAME scripts the C harness feeds its universal seam. Compares
the ordered seam-call trace + the return (or raise spec).

Input: stdin JSON lines from t_port_sqlite_messages_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...]}}
Summary: ORACLE sqlite_messages_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.

Modeling notes (identical on the C side):
- refusals(holder, sid) is a SEAMED sibling (_errors.py): a passthrough CM
  recording structural events refusals.enter {session_id} and refusals.exit
  {session_id, exc} (exc = the str-form raise spec leaving the inner with-body
  BEFORE the ported _refusals conversion; null on success). No script.
- _execute_transcript_write is a SEAMED structural passthrough driver:
  enter {messages, patience_s}, the _do body runs between the events, exit
  {exc}; its retry/pre-state restore is outside the ported set.
- holder() / the read-ctx CMs are silent state doubles. conn.execute(sql,
  params) is a scripted seam whose ret is the fetchone() row as a KEYED dict
  or null.
- Store verbs project to {"args": [positionals after holder/conn], "kwargs":
  {...}} with ALL kwargs recorded as passed (None-valued included).
- MUTATING verbs return a script both sides apply to their own dicts:
  _append_messages_in_transaction {"inserted", "stamps"}, replace_messages
  {"stamps"}, archive_and_compact {"active", "stamps"}, rewrite_pruned_rows
  {"rewritten", "stamps"}, _stamp_tool_call_uids {"msg", "index"},
  _restore_row_identity {"msg"}. The trace ret is the script; the live flow
  receives the scalar (inserted/active/rewritten/None).
- classify_persistence_error {"cause": str-form spec or null} -> bucket string
  (claimed sibling). from_openai_message {"message"} -> a ConversationMessage
  FIELD dict (claimed sibling; materialized into the record here, projected
  with all 33 field defaults on the C side). plan_user_turn_rewind's ret
  materializes as the contract RewindPlan (all 10 fields required).
- Script semantics: io[name] absent -> RuntimeError "no script for <name>";
  array -> per-call queue (exhaustion -> RuntimeError); {"__script__":true,
  "ret":X} -> constant container; other value -> constant. A raise spec
  {"__raise__":cls,"message":msg} is RECORDED then raised as a real exception
  (BaseException trio included; persistence kinds use the REAL extracted
  classes so except-clause identity holds). KeyError messages carry the RAW
  key ("key_kind":"int" for int keys); str(KeyError(k))==repr(k) is applied at
  exc/ret projection. Specs may carry "cause" (exc.__cause__).
- append_batch attaches batch_progress to ANY escaping BaseException; the
  exc/ret projection carries it as {"outcomes", "uids"}.
- Fixture domain (ASCII): record specs are VALID post-__post_init__ (the
  claimed validators are outside the ported set); construction mechanics
  (required/unknown kwargs) are reproduced. JSON text cells are canonical
  strict JSON (no NaN/Infinity, lone surrogates, leading zeros, raw controls,
  invalid escapes, trailing commas; depth <= 8). int()/float() strings use
  ASCII whitespace, finite values. stored maps carry decimal-string keys for
  the int row ids. tool_calls/display_metadata/tool_call_uids cells are
  lists/objects or falsy/null. SessionId/MessageId are NewType (identity);
  records project to field-order dicts with enums as .value; tuples as lists.
"""
import ast
import copy
import json
import math
import os
import sys
import types
from dataclasses import dataclass, field, fields as dc_fields
from enum import Enum
from typing import Any, Optional, Iterator, Mapping, Sequence, NewType, TypeAlias, Literal
from contextlib import contextmanager

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "persistence", "sqlite", "conversation", "messages.py")
CONTRACT = os.path.join(QUARRY, "persistence", "conversation", "messages.py")
ERRORS = os.path.join(QUARRY, "persistence", "core", "errors.py")
IDENTITY = os.path.join(QUARRY, "persistence", "core", "identity.py")
CONVENTIONS = os.path.join(QUARRY, "persistence", "core", "conventions.py")
TURNS = os.path.join(QUARRY, "session_rules", "turns.py")
RULES = os.path.join(QUARRY, "persistence", "sqlite", "conversation", "_store", "rules.py")

import __future__
ANN_FLAG = __future__.annotations.compiler_flag

# The ported set (PoP targets). Everything else is seamed or silent.
FUNCS = [
    "_visibility", "_sidecar", "_message_dict", "_held_dict", "_pruned_original",
    "_transcript_row", "_message_record", "_stamp", "_row_outcomes", "_conversation",
    "_refusals", "_append_chunk", "append", "append_batch", "replace_transcript",
    "compact", "rewrite_pruned_rows", "set_reaction", "take_unseen_reactions",
    "read_transcript", "active_watermark", "conversation_revision", "read_around",
    "read_session_timeline", "read_prompt_page", "read_compression_cooldown_snapshot",
    "read_compression_cooldown", "restore_compression_cooldown", "latest_row_id",
    "message_storage_state", "plan_user_turn_rewind",
]
LEAF_FUNCS = {
    "_visibility", "_sidecar", "_message_dict", "_held_dict", "_pruned_original",
    "_transcript_row", "_message_record", "_stamp", "_row_outcomes", "_conversation",
    "_refusals", "_append_chunk",
}
METHOD_FUNCS = set(FUNCS) - LEAF_FUNCS

# Seamed scripted names (fixture io keys == C seam names == recorder names).
IO_NAMES = [
    "append_message", "get_messages", "replace_messages", "archive_and_compact",
    "rewrite_pruned_rows", "set_message_reaction", "take_unseen_reactions",
    "get_active_message_watermark", "conversation_revision", "get_messages_around",
    "latest_message_row_id", "get_compression_failure_cooldown_row",
    "restore_compression_failure_cooldown_row", "plan_user_turn_rewind",
    "get_session_timeline", "get_session_messages_around",
    "_append_messages_in_transaction", "_stamp_tool_call_uids", "_parse_tool_calls",
    "_active_logical_message_row", "transcript_row_snapshot", "_restore_row_identity",
    "message_uid_or_none", "classify_persistence_error", "from_openai_message",
    "conn.execute",
]
STRUCTURAL = [
    "refusals.enter", "refusals.exit",
    "_execute_transcript_write.enter", "_execute_transcript_write.exit",
]

CUR = {"in": {}, "trace": [], "cursors": {}, "func": ""}
_MISSING = object()
NS = {}


def rec(ev):
    CUR["trace"].append(ev)


def exc_spec(e):
    d = {"__raise__": type(e).__name__, "message": str(e)}
    bp = getattr(e, "batch_progress", None)
    if NS and isinstance(bp, NS.get("BatchAppendProgress", ())):
        d["batch_progress"] = {
            "outcomes": [rec_proj(o) for o in bp.outcomes],
            "uids": [rec_proj(u) for u in bp.uids],
        }
    return d


# The full builtin hierarchy the scripts may raise (real classes).
BUILTIN_EXC = {
    "BaseException": BaseException, "SystemExit": SystemExit,
    "KeyboardInterrupt": KeyboardInterrupt, "GeneratorExit": GeneratorExit,
    "Exception": Exception, "TypeError": TypeError,
    "StopAsyncIteration": StopAsyncIteration, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError, "MemoryError": MemoryError,
    "NameError": NameError, "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "Warning": Warning,
    "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
}

# The persistence family kinds the scripts may raise (REAL extracted classes;
# str(exc) must equal the spec message, so they are built via __new__ + args —
# the class identity is what the except clauses bind).
PERSISTENCE_KINDS = [
    "PersistenceError", "NotFound", "SessionRowMissing", "Conflict",
    "_SessionConflict", "CompressionClosed", "CompressionBusy",
    "CompressionInProgress", "TurnLeaseLost", "SessionWriteGuarded",
    "RewindRefused", "HeldRowRewritten", "PruneRowUnresolved", "PruneRowStale",
    "CooldownRestoreRefused", "CompressionPublishRefused", "SessionLedgered",
    "TransientStoreError", "StoreContention", "DiskFull", "PermanentStoreError",
    "CorruptStoreError", "StoreQuarantined", "MalformedSchema",
    "SearchIndexCorrupt", "DeclarativeDrift", "StoreUnavailable", "StoreReplaced",
    "DeletedWalGeneration", "ExportTooLarge", "CapabilityUnsupported",
    "AuthorityRefusal", "FenceStale", "UnknownExecution", "ProofRejected",
    "HandoffUnsettled", "RetiredSessionError", "ResumeTooLarge",
    "RewindTargetUnavailable",
]


def build_exc(spec):
    """A scripted raise needs a REAL exception instance; the BaseException
    trio must be the real builtins and the persistence kinds the real
    extracted classes so except-clause identity holds."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    if kind == "CancelledError":
        import asyncio
        e = asyncio.CancelledError(msg)
    elif kind in BUILTIN_EXC:
        cls = BUILTIN_EXC[kind]
        if kind == "KeyError" and spec.get("key_kind") == "int":
            e = cls(int(msg))
        else:
            e = cls(msg)
    elif kind in PERSISTENCE_KINDS:
        cls = NS[kind]
        e = cls.__new__(cls)
        e.args = (msg,)
    else:
        e = type(kind, (Exception,), {})(msg)
    cause = spec.get("cause")
    if isinstance(cause, dict) and "__raise__" in cause:
        e.__cause__ = build_exc(cause)
    return e


def rec_seam(name, args):
    """Universal scripted seam: queue/constant/__script__ semantics; records
    {ev, args, ret} then raises a scripted spec or returns a deep copy."""
    io = CUR["in"].get("io") or {}
    script = io.get(name, _MISSING)
    if script is _MISSING:
        result = {"__raise__": "RuntimeError", "message": "no script for " + name}
    elif isinstance(script, list):
        cur = CUR["cursors"].get(name, 0)
        if cur >= len(script):
            result = {"__raise__": "RuntimeError",
                      "message": "script exhausted for " + name}
        else:
            result = script[cur]
            CUR["cursors"][name] = cur + 1
    elif isinstance(script, dict) and script.get("__script__"):
        result = script.get("ret")
    else:
        result = script
    rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(result)})
    if isinstance(result, dict) and "__raise__" in result:
        raise build_exc(result)
    return copy.deepcopy(result)


class _RefusalsCM:
    """The seamed refusals() sibling: passthrough, structural enter/exit."""
    def __init__(self, sid):
        self.sid = sid

    def __enter__(self):
        rec({"ev": "refusals.enter", "args": {"session_id": self.sid}, "ret": None})
        return self

    def __exit__(self, et, e, tb):
        rec({"ev": "refusals.exit",
             "args": {"session_id": self.sid, "exc": None if e is None else exc_spec(e)},
             "ret": None})
        return False


def _refusals(holder=None, session_id=None):
    return _RefusalsCM(session_id)


def _store_verb(name):
    def f(*a, **k):
        args = {"args": list(a[1:]), "kwargs": dict(k)}
        return rec_seam(name, args)
    return f


def _apply_stamps(dicts, script):
    if not isinstance(script, dict):
        return
    stamps = script.get("stamps") or []
    for msg, stamp in zip(dicts, stamps):
        msg.update(stamp)


def _append_messages_in_transaction(holder, conn, session_id, messages, **guards):
    r = rec_seam("_append_messages_in_transaction",
                 {"args": [session_id, messages], "kwargs": dict(guards)})
    if r is None:
        return None
    _apply_stamps(messages, r)
    return r.get("inserted")


def _replace_messages(holder, session_id, messages, **kw):
    r = rec_seam("replace_messages", {"args": [session_id, messages], "kwargs": dict(kw)})
    if isinstance(r, dict):
        _apply_stamps(messages, r)
    return None


def _archive_and_compact(holder, session_id, dicts, **kw):
    r = rec_seam("archive_and_compact", {"args": [session_id, dicts], "kwargs": dict(kw)})
    if r is None:
        return None
    _apply_stamps(dicts, r)
    return r.get("active")


def _rewrite_pruned_rows(holder, session_id, pairs, **kw):
    r = rec_seam("rewrite_pruned_rows", {"args": [session_id, pairs], "kwargs": dict(kw)})
    if r is None:
        return None
    stamps = r.get("stamps") or []
    for pair, stamp in zip(pairs, stamps):
        pair[1].update(stamp)
    return r.get("rewritten")


def _stamp_tool_call_uids(msg, tool_calls, batch_index):
    r = rec_seam("_stamp_tool_call_uids",
                 {"msg": msg, "tool_calls": tool_calls, "index": batch_index})
    if r is not None:
        msg.update(r.get("msg") or {})
        batch_index.clear()
        batch_index.update(r.get("index") or {})
    return None


def _restore_row_identity(row, msg):
    r = rec_seam("_restore_row_identity", {"row": row, "msg": msg})
    if r is not None:
        msg.update(r.get("msg") or {})
    return None


def _parse_tool_calls(tool_calls):
    return rec_seam("_parse_tool_calls", {"tool_calls": tool_calls})


def _active_logical_message_row(conn, session_id, role, message_uid):
    return rec_seam("_active_logical_message_row",
                    {"session_id": session_id, "role": role, "message_uid": message_uid})


def transcript_row_snapshot(row):
    return rec_seam("transcript_row_snapshot", {"row": row})


def message_uid_or_none(msg):
    return rec_seam("message_uid_or_none", {"msg": msg})


def classify_persistence_error(exc):
    return rec_seam("classify_persistence_error",
                    {"cause": None if exc is None else exc_spec(exc)})


def _execute_transcript_write(holder, fn, messages, **kwargs):
    """Structural passthrough driver: the _do body runs between the events."""
    rec({"ev": "_execute_transcript_write.enter",
         "args": {"messages": copy.deepcopy(messages),
                  "patience_s": kwargs.get("patience_s")},
         "ret": None})
    try:
        ret = fn(object())
    except BaseException as e:
        rec({"ev": "_execute_transcript_write.exit",
             "args": {"exc": exc_spec(e)}, "ret": None})
        raise
    rec({"ev": "_execute_transcript_write.exit", "args": {"exc": None}, "ret": None})
    return ret


def _plan_user_turn_rewind(holder, session_id, user_ordinal, **kw):
    r = rec_seam("plan_user_turn_rewind",
                 {"args": [session_id, user_ordinal], "kwargs": dict(kw)})
    if not isinstance(r, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(r).__name__)
    return NS["RewindPlan"](**r)


CM_TUPLE_FIELDS = ("absorbed_message_uids", "data_use", "tool_calls", "absorbed_row_ids")


def mat_conversation_message(r):
    if not isinstance(r, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(r).__name__)
    kw = dict(r)
    for f in CM_TUPLE_FIELDS:
        if f in kw and isinstance(kw[f], list):
            kw[f] = tuple(kw[f])
    return NS["ConversationMessage"](**kw)


def _from_openai_seam(cls, message):
    r = rec_seam("from_openai_message", {"message": message})
    return mat_conversation_message(r)


class _Cursor:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row


class _Conn:
    def execute(self, sql, params=()):
        row = rec_seam("conn.execute", {"sql": sql, "params": list(params)})
        return _Cursor(row)


class _ReadCtx:
    def __enter__(self):
        return _Conn()

    def __exit__(self, *a):
        return False


def _reg_mod(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def _src(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def _exec_node(node, src, path, ns):
    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec",
                 flags=ANN_FLAG, dont_inherit=True), ns)


def _extract_class(path, classname, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name == classname:
            _exec_node(node, src, path, ns)
            return
    raise RuntimeError("class %s not found in %s" % (classname, path))


def _extract_assign(path, name, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == name for t in node.targets):
            _exec_node(node, src, path, ns)
            return
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) \
                and node.target.id == name:
            _exec_node(node, src, path, ns)
            return
    raise RuntimeError("assign %s not found in %s" % (name, path))


def _extract_func(path, name, ns):
    src = _src(path)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            _exec_node(node, src, path, ns)
            return
    raise RuntimeError("func %s not found in %s" % (name, path))

def extract():
    global NS
    modname = "sqlite_messages_flow_extracted"
    mod = _reg_mod(modname)
    ns = mod.__dict__
    ns["__name__"] = modname
    ns["__builtins__"] = __builtins__
    ns["json"] = json
    ns["types"] = types
    ns["contextmanager"] = contextmanager
    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["Enum"] = Enum
    ns["Any"] = Any
    ns["Optional"] = Optional
    ns["Iterator"] = Iterator
    ns["Mapping"] = Mapping
    ns["Sequence"] = Sequence
    ns["NewType"] = NewType
    ns["TypeAlias"] = TypeAlias
    ns["Literal"] = Literal

    # Identity NewTypes + the visibility enum.
    _extract_assign(IDENTITY, "SessionId", ns)
    _extract_assign(IDENTITY, "MessageId", ns)
    _extract_assign(IDENTITY, "JsonValue", ns)
    _extract_class(CONVENTIONS, "RowVisibility", ns)

    # The real error family, in dependency order (except-clause identity).
    _extract_func(ERRORS, "_one_line", ns)
    for nm in ("PersistenceError", "NotFound", "SessionRowMissing", "Conflict",
               "_SessionConflict", "CompressionClosed", "CompressionBusy",
               "CompressionInProgress", "TurnLeaseLost", "SessionWriteGuarded",
               "RewindRefused", "HeldRowRewritten", "PruneRowUnresolved",
               "PruneRowStale", "CooldownRestoreRefused", "CompressionPublishRefused",
               "SessionLedgered", "TransientStoreError", "StoreContention",
               "DiskFull", "PermanentStoreError", "CorruptStoreError",
               "StoreQuarantined", "MalformedSchema", "SearchIndexCorrupt",
               "DeclarativeDrift", "StoreUnavailable", "StoreReplaced",
               "DeletedWalGeneration", "ExportTooLarge", "CapabilityUnsupported",
               "AuthorityRefusal", "StaleFenceKind", "FenceStale",
               "UnknownExecution", "ProofRejected", "HandoffUnsettled",
               "RetiredSessionError"):
        _extract_class(ERRORS, nm, ns)
    _extract_assign(ERRORS, "_REWIND_REFUSAL_TEXT", ns)

    # Contract records / enums, in dependency order.
    for nm in ("TranscriptView", "ReplaceMode", "RowDisposition", "NewMessage",
               "RowStamp", "RowOutcome", "BatchAppendResult", "RowUids",
               "BatchAppendProgress", "HeldMessage", "HeldCoverage", "ReplaceResult",
               "CompactionResult", "PrunedRowChange", "PruneRewriteResult",
               "Watermark", "MessageRecord", "MessageStorageState", "Reaction",
               "UnseenReaction", "CompressionCooldown", "CooldownSnapshot",
               "ConversationRequest", "ConversationMessage", "ResumeConversations",
               "ConversationRevision", "ResumeTooLarge", "RewindTargetUnavailable",
               "AnchoredWindow", "AnchoredView", "PrUrlHit", "ConversationPeer",
               "DelegationDelivery", "MarkerPurgeReport", "RewindPlan",
               "UserTurnRewind", "TranscriptRead", "TranscriptRow", "TimelineEntry",
               "SessionTimeline", "PromptPage", "PagedMessage", "SegmentPage"):
        _extract_class(CONTRACT, nm, ns)

    # The turn markers + row-state keys the ported set reads.
    for nm in ("_DB_PERSISTED_MARKER", "MERGED_DURABLE_ROWS", "OWN_ROW",
               "RETIRED_DURABLE_ROWS", "RETIRED_ROW", "UNNAMED_DURABLE_ROWS"):
        _extract_assign(TURNS, nm, ns)
    for nm in ("DB_ROW_SNAPSHOT", "RESTORED_FROM_STORE"):
        _extract_assign(RULES, nm, ns)

    # Target module: constants, leaves, the repository class.
    src = _src(SRC)
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in (
                    "_REWIND_REFUSALS", "_MESSAGE_OPTIONALS", "_WIRE_FIELDS",
                    "_SIDECARS") for t in node.targets):
            _exec_node(node, src, SRC, ns)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in LEAF_FUNCS:
            _exec_node(node, src, SRC, ns)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteMessageRepository":
            _exec_node(node, src, SRC, ns)

    # Seamed collaborators (module globals the extracted bodies resolve).
    ns["refusals"] = _refusals
    ns["classify_persistence_error"] = classify_persistence_error
    ns["message_uid_or_none"] = message_uid_or_none
    ns["_parse_tool_calls"] = _parse_tool_calls
    ns["_stamp_tool_call_uids"] = _stamp_tool_call_uids
    ns["_append_messages_in_transaction"] = _append_messages_in_transaction
    ns["_execute_transcript_write"] = _execute_transcript_write
    ns["_active_logical_message_row"] = _active_logical_message_row
    ns["transcript_row_snapshot"] = transcript_row_snapshot
    ns["_restore_row_identity"] = _restore_row_identity
    ns["replace_messages"] = _replace_messages
    ns["archive_and_compact"] = _archive_and_compact
    ns["rewrite_pruned_rows"] = _rewrite_pruned_rows
    ns["plan_user_turn_rewind"] = _plan_user_turn_rewind
    ns["timeline"] = types.SimpleNamespace(
        get_session_timeline=_store_verb("get_session_timeline"),
        get_session_messages_around=_store_verb("get_session_messages_around"))
    for nm in IO_NAMES:
        if nm in ns or nm in ("conn.execute", "get_session_timeline",
                              "get_session_messages_around"):
            continue
        ns[nm] = _store_verb(nm)
    # The claimed from_openai_message sibling is a seam on the real class.
    ns["ConversationMessage"].from_openai_message = classmethod(_from_openai_seam)

    for nm in list(LEAF_FUNCS) + ["SQLiteMessageRepository", "_REWIND_REFUSALS",
                       "_MESSAGE_OPTIONALS", "_WIRE_FIELDS", "_SIDECARS",
                       "DB_ROW_SNAPSHOT", "RESTORED_FROM_STORE",
                       "_DB_PERSISTED_MARKER", "MERGED_DURABLE_ROWS", "OWN_ROW",
                       "RETIRED_DURABLE_ROWS", "RETIRED_ROW",
                       "UNNAMED_DURABLE_ROWS", "SessionId", "MessageId",
                       "RowVisibility"] + PERSISTENCE_KINDS:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    for nm in METHOD_FUNCS:
        if not hasattr(ns["SQLiteMessageRepository"], nm):
            raise RuntimeError("method missing: " + nm)
    NS = ns
    return ns


def setup_case(inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["cursors"] = {}
    holder = types.SimpleNamespace(
        _TRANSCRIPT_WRITE_PATIENCE_S=inp.get("patience_s", 30.0),
        _read_ctx=lambda: _ReadCtx(),
        _read_ctx_with_timeout=lambda t: _ReadCtx(),
        db_path="state.db")
    store = types.SimpleNamespace(holder=lambda reacquire=True: holder,
                                  store_path="state.db")
    return NS["SQLiteMessageRepository"](store), holder


def rec_proj(r):
    d = {}
    for f in dc_fields(r):
        v = getattr(r, f.name)
        if isinstance(v, Enum):
            v = v.value
        d[f.name] = v
    return d


NM_TUPLE_FIELDS = ("tool_calls", "absorbed_message_uids", "absorbed_row_ids", "data_use")
HM_TUPLE_FIELDS = ("tool_calls", "absorbed_row_ids", "retired_durable_rows", "tool_call_ids")


def mat_new_message(spec):
    if not isinstance(spec, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(spec).__name__)
    kw = dict(spec)
    for f in NM_TUPLE_FIELDS:
        if f in kw and isinstance(kw[f], list):
            kw[f] = tuple(kw[f])
    return NS["NewMessage"](**kw)


def mat_held(spec):
    if not isinstance(spec, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(spec).__name__)
    kw = dict(spec)
    for f in HM_TUPLE_FIELDS:
        if f in kw and isinstance(kw[f], list):
            kw[f] = tuple(kw[f])
    if "retired_durable_rows" in kw:
        kw["retired_durable_rows"] = tuple(mat_held(r) for r in kw["retired_durable_rows"])
    return NS["HeldMessage"](**kw)


def mat_change(spec):
    # C check order: PrunedRowChange mechanics, then before, original, replacement.
    if not isinstance(spec, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(spec).__name__)
    for k in spec:
        if k not in ("original", "replacement", "before"):
            raise TypeError("PrunedRowChange.__init__() got an unexpected keyword argument '%s'" % k)
    missing = [k for k in ("original", "replacement", "before") if k not in spec]
    if missing:
        if len(missing) == 1:
            names = "'%s'" % missing[0]
        elif len(missing) == 2:
            names = "'%s' and '%s'" % (missing[0], missing[1])
        else:
            names = ", ".join("'%s'" % k for k in missing[:-1]) + ", and '%s'" % missing[-1]
        raise TypeError("PrunedRowChange.__init__() missing %d required positional argument%s: %s"
                        % (len(missing), "" if len(missing) == 1 else "s", names))
    before = mat_new_message(spec["before"])
    original = mat_held(spec["original"])
    replacement = mat_new_message(spec["replacement"])
    return NS["PrunedRowChange"](original=original, replacement=replacement, before=before)


def mat_request(rq):
    if rq is None:
        return NS["TranscriptRead"]()
    if not isinstance(rq, dict):
        raise TypeError("argument of type '%s' is not a mapping" % type(rq).__name__)
    kw = dict(rq)
    if "view" in kw and kw["view"] is not None:
        kw["view"] = NS["TranscriptView"](kw["view"])
    return NS["TranscriptRead"](**kw)


def drive(inp):
    func = CUR["func"]
    repo, holder = setup_case(inp)
    sid = inp.get("session_id")
    try:
        if func == "_visibility":
            ret = NS["_visibility"](inp.get("active"), inp.get("compacted")).value
        elif func == "_sidecar":
            ret = NS["_sidecar"](inp.get("value"))
        elif func == "_message_dict":
            ret = NS["_message_dict"](mat_new_message(inp.get("message")))
        elif func == "_held_dict":
            ret = NS["_held_dict"](mat_held(inp.get("held")))
        elif func == "_pruned_original":
            ret = NS["_pruned_original"](mat_change(inp.get("change")))
        elif func == "_transcript_row":
            ret = rec_proj(NS["_transcript_row"](inp.get("row")))
        elif func == "_message_record":
            ret = rec_proj(NS["_message_record"](inp.get("row")))
        elif func == "_stamp":
            stored = {int(k): v for k, v in (inp.get("stored") or {}).items()}
            ret = rec_proj(NS["_stamp"](inp.get("msg"), stored))
        elif func == "_row_outcomes":
            ret = [rec_proj(o) for o in NS["_row_outcomes"](inp.get("rows"), inp.get("count"))]
        elif func == "_conversation":
            ret = [rec_proj(m) for m in NS["_conversation"](inp.get("messages"))]
        elif func == "_refusals":
            body = inp.get("body") or {}
            with NS["_refusals"](holder, sid):
                rspec = body.get("raise")
                if isinstance(rspec, dict) and "__raise__" in rspec:
                    raise build_exc(rspec)
                ret = body.get("ret")
        elif func == "_append_chunk":
            ret = NS["_append_chunk"](holder, sid, inp.get("messages"),
                                      **(inp.get("guards") or {}))
        elif func == "append":
            ret = repo.append(sid, mat_new_message(inp.get("message")),
                              turn_lease_holder=inp.get("turn_lease_holder"),
                              turn_lease_ttl_seconds=inp.get("turn_lease_ttl_seconds", 300.0))
        elif func == "append_batch":
            msgs = [mat_new_message(s) for s in inp.get("messages")]
            r = repo.append_batch(sid, msgs,
                                  turn_lease_holder=inp.get("turn_lease_holder"),
                                  chunk_rows=inp.get("chunk_rows"),
                                  turn_lease_ttl_seconds=inp.get("turn_lease_ttl_seconds", 300.0))
            ret = {"outcomes": [rec_proj(o) for o in r.outcomes]}
        elif func == "replace_transcript":
            mode = NS["ReplaceMode"](inp.get("mode"))
            msgs = [mat_new_message(s) for s in inp.get("messages")]
            r = repo.replace_transcript(sid, msgs, mode,
                                        reject_active_turn_lease=inp.get("reject_active_turn_lease", False))
            ret = {"rows": [rec_proj(s) for s in r.rows]}
        elif func == "compact":
            replacement = [mat_new_message(s) for s in inp.get("replacement")]
            wm = inp.get("watermark")
            watermark = None if wm is None else NS["Watermark"](**wm)
            carried = [mat_held(s) for s in (inp.get("carried") or ())]
            cov = inp.get("coverage")
            if cov is None:
                coverage = None
            else:
                if not isinstance(cov, dict):
                    raise TypeError("argument of type '%s' is not a mapping" % type(cov).__name__)
                kw = dict(cov)
                if "covered_ids" in kw:
                    kw["covered_ids"] = tuple(kw["covered_ids"] or ())
                if "unresolved" in kw:
                    kw["unresolved"] = tuple(mat_held(s) for s in (kw["unresolved"] or ()))
                coverage = NS["HeldCoverage"](**kw)
            hfc = inp.get("held_for_cas")
            held_for_cas = None if hfc is None else [mat_held(s) for s in hfc]
            r = repo.compact(sid, replacement, watermark=watermark,
                             carried_tail_count=inp.get("carried_tail_count", 0),
                             lock_holder=inp.get("lock_holder"),
                             model_config_patch=inp.get("model_config_patch"),
                             carried=carried, coverage=coverage, held_for_cas=held_for_cas)
            ret = {"active_count": r.active_count, "rows": [rec_proj(s) for s in r.rows]}
        elif func == "rewrite_pruned_rows":
            changes = [mat_change(s) for s in inp.get("changes")]
            r = repo.rewrite_pruned_rows(sid, changes,
                                         model_config_patch=inp.get("model_config_patch"),
                                         turn_lease_holder=inp.get("turn_lease_holder"),
                                         turn_lease_ttl_seconds=inp.get("turn_lease_ttl_seconds", 300.0))
            ret = {"rewritten": r.rewritten, "snapshots": list(r.snapshots)}
        elif func == "set_reaction":
            r = repo.set_reaction(sid, inp.get("message_id"), inp.get("emoji"),
                                  author=inp.get("author"))
            ret = None if r is None else [rec_proj(x) for x in r]
        elif func == "take_unseen_reactions":
            r = repo.take_unseen_reactions(sid, author=inp.get("author", "user"))
            ret = [rec_proj(x) for x in r]
        elif func == "read_transcript":
            r = repo.read_transcript(sid, mat_request(inp.get("request")))
            ret = [rec_proj(x) for x in r]
        elif func == "active_watermark":
            ret = rec_proj(repo.active_watermark(sid))
        elif func == "conversation_revision":
            r = repo.conversation_revision(sid, include_lineage=inp.get("include_lineage", False))
            ret = rec_proj(r)
        elif func == "read_around":
            r = repo.read_around(sid, inp.get("anchor"), window=inp.get("window", 5))
            ret = {"window": [rec_proj(x) for x in r.window],
                   "before": r.before, "after": r.after}
        elif func == "read_session_timeline":
            r = repo.read_session_timeline(sid, limit=inp.get("limit", 500),
                                           after_row_id=inp.get("after_row_id", 0))
            ret = {"entries": [rec_proj(e) for e in r.entries],
                   "limit": r.limit, "after_row_id": r.after_row_id,
                   "returned": r.returned, "total": r.total,
                   "has_more": r.has_more, "next_cursor": r.next_cursor}
        elif func == "read_prompt_page":
            r = repo.read_prompt_page(sid, inp.get("row_id"), limit=inp.get("limit", 120))
            if r is None:
                ret = None
            else:
                ret = {"messages": [rec_proj(x) for x in r.messages],
                       "row_id": r.row_id, "limit": r.limit, "returned": r.returned,
                       "offset": r.offset, "total": r.total,
                       "has_older": r.has_older, "has_newer": r.has_newer}
        elif func == "read_compression_cooldown_snapshot":
            ret = rec_proj(repo.read_compression_cooldown_snapshot(sid))
        elif func == "read_compression_cooldown":
            r = repo.read_compression_cooldown(sid, inp.get("at"))
            ret = None if r is None else rec_proj(r)
        elif func == "restore_compression_cooldown":
            snap = NS["CooldownSnapshot"](**inp.get("snapshot"))
            repo.restore_compression_cooldown(sid, snap)
            ret = None
        elif func == "latest_row_id":
            ret = repo.latest_row_id(sid, role=inp.get("role", "user"),
                                     offset=inp.get("offset", 0),
                                     require_text=inp.get("require_text", True))
        elif func == "message_storage_state":
            r = repo.message_storage_state(inp.get("message_id"))
            ret = None if r is None else rec_proj(r)
        elif func == "plan_user_turn_rewind":
            r = repo.plan_user_turn_rewind(sid, inp.get("user_ordinal"),
                                           warm_history=inp.get("warm_history"),
                                           require_retryable=inp.get("require_retryable", False),
                                           require_composite=inp.get("require_composite", False))
            ret = rec_proj(r)
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = exc_spec(e)
    return {"ret": ret, "trace": CUR["trace"]}


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        f = float(x)
        if math.isfinite(f) and f == int(f) and abs(f) < 1e15:
            return ("num", float(int(f)))
        return ("num", f)
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        try:
            want = drive(inp)
        except BaseException as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE sqlite_messages_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
