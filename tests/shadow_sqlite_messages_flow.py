#!/usr/bin/env python3
"""Shadow mirror for sqlite_messages_flow: a LITERAL Python transcription of
src/persistence/sqlite_messages_flow.c (accessor semantics, check order,
exception table, message text), diffed against the oracle's LIVE drive over
every fixture case. Code-only verification: no C compile/link involved.

Run: python tests/shadow_sqlite_messages_flow.py  -> "SHADOW sqlite_messages_flow:
N cases, M mismatches" (exit 1 on any mismatch or zero cases).
"""
import copy
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oracle_sqlite_messages_flow as O

MISSING = object()


def jget(obj, key):
    if isinstance(obj, dict) and key in obj:
        return obj[key]
    return MISSING


def jhas(obj, key):
    return isinstance(obj, dict) and key in obj


def cp(v):
    return None if v is MISSING else copy.deepcopy(v)


def truth(v):
    if v is MISSING or v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def is_null(v):
    return v is MISSING or v is None


def typename(v):
    if v is MISSING or v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "int" if float(v) == int(v) else "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "dict"
    return "object"


def py_str(v):
    if v is MISSING or v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        if float(v) == int(v):
            return "%d" % int(v)
        for prec in range(1, 18):
            s = "%.*g" % (prec, v)
            if float(s) == v:
                return s
        return repr(v)
    if isinstance(v, str):
        return v
    return "<%s>" % typename(v)


def py_repr(s):
    if s is None:
        s = ""
    has_sq = "'" in s
    has_dq = '"' in s
    quote = '"' if (has_sq and not has_dq) else "'"
    out = [quote]
    for ch in s:
        c = ord(ch)
        if ch == quote or ch == "\\":
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif c < 0x20 or c == 0x7F:
            out.append("\\x%02x" % c)
        else:
            out.append(ch)
    out.append(quote)
    return "".join(out)


class MExc(Exception):
    def __init__(self, spec):
        self.spec = spec
        super().__init__(spec.get("message", ""))


def raise_(cls, msg):
    return MExc({"__raise__": cls, "message": msg})


def raise_key(key):
    return MExc({"__raise__": "KeyError", "message": key})


def raise_key_int(n):
    return MExc({"__raise__": "KeyError", "message": "%d" % int(n), "key_kind": "int"})


def kind_of(spec):
    return spec.get("__raise__", "")


# ── the FULL exception parent table (transcribed from SM_EXC_TABLE) ──
EXC_TABLE = [
    ("BaseException", None, None),
    ("SystemExit", "BaseException", None),
    ("KeyboardInterrupt", "BaseException", None),
    ("GeneratorExit", "BaseException", None),
    ("CancelledError", "BaseException", None),
    ("BaseExceptionGroup", "BaseException", None),
    ("Exception", "BaseException", None),
    ("ExceptionGroup", "Exception", None),
    ("TypeError", "Exception", None),
    ("StopAsyncIteration", "Exception", None),
    ("StopIteration", "Exception", None),
    ("ArithmeticError", "Exception", None),
    ("FloatingPointError", "ArithmeticError", None),
    ("OverflowError", "ArithmeticError", None),
    ("ZeroDivisionError", "ArithmeticError", None),
    ("AssertionError", "Exception", None),
    ("AttributeError", "Exception", None),
    ("BufferError", "Exception", None),
    ("EOFError", "Exception", None),
    ("ImportError", "Exception", None),
    ("ModuleNotFoundError", "ImportError", None),
    ("LookupError", "Exception", None),
    ("IndexError", "LookupError", None),
    ("KeyError", "LookupError", None),
    ("MemoryError", "Exception", None),
    ("NameError", "Exception", None),
    ("UnboundLocalError", "NameError", None),
    ("OSError", "Exception", None),
    ("BlockingIOError", "OSError", None),
    ("ChildProcessError", "OSError", None),
    ("ConnectionError", "OSError", None),
    ("BrokenPipeError", "ConnectionError", None),
    ("ConnectionAbortedError", "ConnectionError", None),
    ("ConnectionRefusedError", "ConnectionError", None),
    ("ConnectionResetError", "ConnectionError", None),
    ("FileExistsError", "OSError", None),
    ("FileNotFoundError", "OSError", None),
    ("InterruptedError", "OSError", None),
    ("IsADirectoryError", "OSError", None),
    ("NotADirectoryError", "OSError", None),
    ("PermissionError", "OSError", None),
    ("ProcessLookupError", "OSError", None),
    ("TimeoutError", "OSError", None),
    ("ReferenceError", "Exception", None),
    ("RuntimeError", "Exception", None),
    ("NotImplementedError", "RuntimeError", None),
    ("RecursionError", "RuntimeError", None),
    ("SyntaxError", "Exception", None),
    ("IndentationError", "SyntaxError", None),
    ("TabError", "IndentationError", None),
    ("SystemError", "Exception", None),
    ("ValueError", "Exception", None),
    ("UnicodeError", "ValueError", None),
    ("UnicodeEncodeError", "UnicodeError", None),
    ("UnicodeDecodeError", "UnicodeError", None),
    ("UnicodeTranslateError", "UnicodeError", None),
    ("Warning", "Exception", None),
    ("DeprecationWarning", "Warning", None),
    ("PendingDeprecationWarning", "Warning", None),
    ("RuntimeWarning", "Warning", None),
    ("SyntaxWarning", "Warning", None),
    ("UserWarning", "Warning", None),
    ("FutureWarning", "Warning", None),
    ("ImportWarning", "Warning", None),
    ("UnicodeWarning", "Warning", None),
    ("BytesWarning", "Warning", None),
    ("EncodingWarning", "Warning", None),
    ("ResourceWarning", "Warning", None),
    ("PersistenceError", "Exception", None),
    ("NotFound", "PersistenceError", None),
    ("SessionRowMissing", "NotFound", None),
    ("Conflict", "PersistenceError", None),
    ("_SessionConflict", "Conflict", None),
    ("CompressionClosed", "Conflict", "RuntimeError"),
    ("CompressionBusy", "_SessionConflict", "RuntimeError"),
    ("CompressionInProgress", "CompressionBusy", None),
    ("TurnLeaseLost", "_SessionConflict", "RuntimeError"),
    ("SessionWriteGuarded", "_SessionConflict", "RuntimeError"),
    ("RewindRefused", "Conflict", "RuntimeError"),
    ("HeldRowRewritten", "_SessionConflict", "RuntimeError"),
    ("PruneRowUnresolved", "_SessionConflict", "RuntimeError"),
    ("PruneRowStale", "PruneRowUnresolved", None),
    ("CooldownRestoreRefused", "_SessionConflict", "RuntimeError"),
    ("CompressionPublishRefused", "_SessionConflict", "RuntimeError"),
    ("SessionLedgered", "_SessionConflict", "RuntimeError"),
    ("TransientStoreError", "PersistenceError", None),
    ("StoreContention", "TransientStoreError", None),
    ("DiskFull", "TransientStoreError", None),
    ("PermanentStoreError", "PersistenceError", None),
    ("CorruptStoreError", "PermanentStoreError", None),
    ("StoreQuarantined", "CorruptStoreError", None),
    ("MalformedSchema", "CorruptStoreError", None),
    ("SearchIndexCorrupt", "CorruptStoreError", None),
    ("DeclarativeDrift", "PermanentStoreError", "RuntimeError"),
    ("StoreUnavailable", "PersistenceError", None),
    ("StoreReplaced", "PersistenceError", "RuntimeError"),
    ("DeletedWalGeneration", "StoreReplaced", None),
    ("ExportTooLarge", "PersistenceError", "ValueError"),
    ("CapabilityUnsupported", "PersistenceError", None),
    ("AuthorityRefusal", "PersistenceError", "ValueError"),
    ("FenceStale", "AuthorityRefusal", None),
    ("UnknownExecution", "AuthorityRefusal", None),
    ("ProofRejected", "AuthorityRefusal", None),
    ("HandoffUnsettled", "AuthorityRefusal", None),
    ("RetiredSessionError", "AuthorityRefusal", None),
    ("ResumeTooLarge", "PersistenceError", "ValueError"),
    ("RewindTargetUnavailable", "PersistenceError", "ValueError"),
]
EXC_BY_NAME = {n: (p1, p2) for n, p1, p2 in EXC_TABLE}


def kind_is(kind, ancestor):
    if kind == ancestor:
        return True
    if kind not in EXC_BY_NAME:
        return False
    queue = [p for p in EXC_BY_NAME[kind] if p]
    i = 0
    while i < len(queue) and len(queue) < 16:
        cur = queue[i]
        if cur == ancestor:
            return True
        if cur in EXC_BY_NAME:
            for p in EXC_BY_NAME[cur]:
                if p and len(queue) < 16:
                    queue.append(p)
        i += 1
    return False


def is_base_exc(kind):
    if kind not in EXC_BY_NAME:
        return False
    return not kind_is(kind, "Exception")


def is_persistence(kind):
    return kind_is(kind, "PersistenceError")


def is_runtime(kind):
    return kind_is(kind, "RuntimeError")


def exc_project(spec):
    r = {"__raise__": kind_of(spec)}
    msg = spec.get("message", "")
    if kind_of(spec) == "KeyError" and spec.get("key_kind", "") != "int":
        r["message"] = py_repr(msg)
    else:
        r["message"] = msg
    if "batch_progress" in spec:
        r["batch_progress"] = copy.deepcopy(spec["batch_progress"])
    return r


# ── seam machinery (mirrors the harness stub + sm_seam) ──
CUR = {"in": {}, "trace": [], "cursors": {}}


def rec(ev):
    CUR["trace"].append(ev)


def seam(name, args):
    io = CUR["in"].get("io") or {}
    script = io.get(name, MISSING)
    if script is MISSING:
        result = {"__raise__": "RuntimeError", "message": "no script for " + name}
    elif isinstance(script, list):
        cur = CUR["cursors"].get(name, 0)
        if cur >= len(script):
            result = {"__raise__": "RuntimeError",
                      "message": "script exhausted for " + name}
        else:
            result = script[cur]
            CUR["cursors"][name] = cur + 1
    elif isinstance(script, dict) and truth(jget(script, "__script__")):
        result = cp(jget(script, "ret"))
    else:
        result = copy.deepcopy(script)
    rec({"ev": name, "args": copy.deepcopy(args), "ret": copy.deepcopy(result)})
    if isinstance(result, dict) and "__raise__" in result:
        raise MExc(result)
    return copy.deepcopy(result)


def structural(name, args):
    rec({"ev": name, "args": copy.deepcopy(args), "ret": None})


def refusals_enter(sid):
    structural("refusals.enter", {"session_id": sid})


def refusals_exit(sid, spec):
    structural("refusals.exit",
               {"session_id": sid, "exc": None if spec is None else exc_project(spec)})


def etw_enter(messages, patience):
    structural("_execute_transcript_write.enter",
               {"messages": cp(messages), "patience_s": patience})


def etw_exit(spec):
    structural("_execute_transcript_write.exit",
               {"exc": None if spec is None else exc_project(spec)})


# ── common error shapes ──

def get_attr_err(v):
    return raise_("AttributeError", "'%s' object has no attribute 'get'" % typename(v))


def subscript_err(v):
    if v is MISSING or v is None:
        return raise_("TypeError", "'NoneType' object is not subscriptable")
    if isinstance(v, list):
        return raise_("TypeError", "list indices must be integers or slices, not str")
    if isinstance(v, str):
        return raise_("TypeError", "string indices must be integers")
    return raise_("TypeError", "'%s' object is not subscriptable" % typename(v))


def iter_err(v):
    return raise_("TypeError", "'%s' object is not iterable" % typename(v))


def missing_args(cls, names):
    n = len(names)
    parts = []
    for i, nm in enumerate(names):
        if i > 0:
            if n == 2:
                parts.append(" and ")
            elif i + 1 == n:
                parts.append(", and ")
            else:
                parts.append(", ")
        parts.append("'%s'" % nm)
    return raise_("TypeError", "%s.__init__() missing %d required positional argument%s: %s"
                  % (cls, n, "" if n == 1 else "s", "".join(parts)))


def enum_invalid(cls, v):
    if isinstance(v, str):
        return raise_("ValueError", "%s is not a valid %s" % (py_repr(v), cls))
    return raise_("ValueError", "%s is not a valid %s" % (py_str(v), cls))


def obj_update(dst, updates):
    if not isinstance(updates, dict):
        return
    for k, v in updates.items():
        dst[k] = copy.deepcopy(v)


def fields_check(spec, cls, fields, required):
    if is_null(spec) or not isinstance(spec, dict):
        raise raise_("TypeError", "argument of type '%s' is not a mapping" % typename(spec))
    for k in spec:
        if k not in fields:
            raise raise_("TypeError",
                         "%s.__init__() got an unexpected keyword argument '%s'" % (cls, k))
    missing = [k for k in required if k not in spec]
    if missing:
        raise missing_args(cls, missing)


# ── coercions (literal sm_py_int / sm_py_float) ──

WS = " \t\n\v\f\r"


def strip_ascii(s):
    return s.strip(WS)


def digits_ok(s):
    if not s or not s[0].isdigit() or not s[-1].isdigit():
        return False
    for i, c in enumerate(s):
        if c == "_":
            if i == 0 or i + 1 >= len(s) or not s[i - 1].isdigit() or not s[i + 1].isdigit():
                return False
        elif not c.isdigit():
            return False
    return True


def py_int(v):
    if is_null(v):
        raise raise_("TypeError",
                     "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'")
    if isinstance(v, bool):
        return 1 if v else 0
    if isinstance(v, (int, float)):
        return math.trunc(v)
    if isinstance(v, str):
        t = strip_ascii(v)
        p = t[1:] if t[:1] in ("+", "-") else t
        if not digits_ok(p):
            raise raise_("ValueError",
                         "invalid literal for int() with base 10: %s" % py_repr(v))
        d = int(p.replace("_", ""), 10)
        if t[:1] == "-":
            d = -d
        return d
    raise raise_("TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'"
                 % typename(v))


def py_float(v):
    if is_null(v):
        raise raise_("TypeError",
                     "float() argument must be a string or a real number, not 'NoneType'")
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, str):
        t = strip_ascii(v)
        p = t
        ok = True
        if p[:1] in ("+", "-"):
            p = p[1:]
        int_start = 0
        while int_start < len(p) and (p[int_start].isdigit() or p[int_start] == "_"):
            int_start += 1
        int_n = int_start
        has_frac = False
        frac_n = 0
        q = p[int_start:]
        if q[:1] == ".":
            has_frac = True
            q = q[1:]
            j = 0
            while j < len(q) and (q[j].isdigit() or q[j] == "_"):
                j += 1
            frac_n = j
            q = q[j:]
        if int_n == 0 and frac_n == 0:
            ok = False
        if ok and int_n > 0 and not digits_ok(p[:int_n]):
            ok = False
        if ok and has_frac and frac_n > 0:
            if not digits_ok(p[int_n + 1:int_n + 1 + frac_n]):
                ok = False
        if ok and q[:1] in ("e", "E"):
            q = q[1:]
            if q[:1] in ("+", "-"):
                q = q[1:]
            if not digits_ok(q):
                ok = False
            q = ""
        if ok and q != "":
            ok = False
        if not ok:
            raise raise_("ValueError", "could not convert string to float: %s" % py_repr(v))
        d = float(t.replace("_", ""))
        if not math.isfinite(d):
            raise raise_("ValueError", "could not convert string to float: %s" % py_repr(v))
        return d
    raise raise_("TypeError",
                 "float() argument must be a string or a real number, not '%s'" % typename(v))


def int_or_0(v):
    if not truth(v):
        return 0
    return py_int(v)


def float_or_0(v):
    if not truth(v):
        return 0.0
    return py_float(v)
# ── strict Python json.loads grammar (literal sm_json_loads) ──

class JParse:
    def __init__(self, text):
        self.s = text
        self.i = 0
        self.n = len(text)

    def ws(self):
        while self.i < self.n and self.s[self.i] in " \t\n\r":
            self.i += 1

    def hex4(self):
        if self.i + 4 > self.n:
            return None
        v = 0
        for k in range(4):
            c = self.s[self.i + k]
            if "0" <= c <= "9":
                v = (v << 4) | (ord(c) - 48)
            elif "a" <= c <= "f":
                v = (v << 4) | (ord(c) - 87)
            elif "A" <= c <= "F":
                v = (v << 4) | (ord(c) - 55)
            else:
                return None
        return v

    def jstring(self):
        self.i += 1
        while self.i < self.n and self.s[self.i] != '"':
            c = self.s[self.i]
            if ord(c) < 0x20:
                return False
            if c == "\\":
                self.i += 1
                if self.i >= self.n:
                    return False
                e = self.s[self.i]
                if e == "u":
                    save = self.i
                    self.i += 1
                    cp = self.hex4()
                    if cp is None:
                        return False
                    if 0xD800 <= cp <= 0xDFFF:
                        return False
                    self.i = save + 4
                elif e not in ('"', "\\", "/", "b", "f", "n", "r", "t"):
                    return False
            self.i += 1
        if self.i >= self.n:
            return False
        self.i += 1
        return True

    def jnumber(self):
        s = self.i
        if self.i < self.n and self.s[self.i] == "-":
            self.i += 1
        if self.i >= self.n:
            return False
        if self.s[self.i] == "0":
            self.i += 1
            if self.i < self.n and self.s[self.i].isdigit():
                return False
        elif self.s[self.i].isdigit():
            while self.i < self.n and self.s[self.i].isdigit():
                self.i += 1
        else:
            return False
        if self.i < self.n and self.s[self.i] == ".":
            self.i += 1
            if self.i >= self.n or not self.s[self.i].isdigit():
                return False
            while self.i < self.n and self.s[self.i].isdigit():
                self.i += 1
        if self.i < self.n and self.s[self.i] in "eE":
            self.i += 1
            if self.i < self.n and self.s[self.i] in "+-":
                self.i += 1
            if self.i >= self.n or not self.s[self.i].isdigit():
                return False
            while self.i < self.n and self.s[self.i].isdigit():
                self.i += 1
        return self.i > s

    def jvalue(self, depth):
        if depth > 32:
            return False
        self.ws()
        if self.i >= self.n:
            return False
        c = self.s[self.i]
        if c == '"':
            return self.jstring()
        if c == "t":
            if self.s[self.i:self.i + 4] != "true":
                return False
            self.i += 4
            return True
        if c == "f":
            if self.s[self.i:self.i + 5] != "false":
                return False
            self.i += 5
            return True
        if c == "n":
            if self.s[self.i:self.i + 4] != "null":
                return False
            self.i += 4
            return True
        if c == "-" or c.isdigit():
            return self.jnumber()
        if c == "[":
            self.i += 1
            self.ws()
            if self.i < self.n and self.s[self.i] == "]":
                self.i += 1
                return True
            while True:
                if not self.jvalue(depth + 1):
                    return False
                self.ws()
                if self.i >= self.n:
                    return False
                if self.s[self.i] == "]":
                    self.i += 1
                    return True
                if self.s[self.i] != ",":
                    return False
                self.i += 1
        if c == "{":
            self.i += 1
            self.ws()
            if self.i < self.n and self.s[self.i] == "}":
                self.i += 1
                return True
            while True:
                self.ws()
                if self.i >= self.n or self.s[self.i] != '"':
                    return False
                if not self.jstring():
                    return False
                self.ws()
                if self.i >= self.n or self.s[self.i] != ":":
                    return False
                self.i += 1
                if not self.jvalue(depth + 1):
                    return False
                self.ws()
                if self.i >= self.n:
                    return False
                if self.s[self.i] == "}":
                    self.i += 1
                    return True
                if self.s[self.i] != ",":
                    return False
                self.i += 1
        return False


def json_loads(text):
    if text is None:
        return None
    p = JParse(text)
    if not p.jvalue(0):
        return None
    p.ws()
    if p.i != p.n:
        return None
    return json.loads(text)


# ── record construction tables ──
NM_FIELDS = ["role", "content", "timestamp", "tool_calls", "tool_name", "tool_call_id",
             "display_kind", "compressed_summary", "api_content", "display_metadata",
             "platform_message_id", "observed", "effect_disposition", "finish_reason",
             "reasoning", "reasoning_content", "reasoning_details", "codex_reasoning_items",
             "codex_message_items", "repair_target", "db_row_snapshot", "message_uid",
             "absorbed_message_uids", "tool_call_uids", "tool_call_uid", "token_count",
             "data_use", "merged_durable_rows", "unnamed_durable_rows", "absorbed_row_ids",
             "restored_from_store"]
NM_REQ = ["role", "content"]
HM_FIELDS = ["role", "content", "tool_call_id", "tool_calls", "timestamp", "row_id",
             "persisted", "db_row_snapshot", "message_uid", "absorbed_row_ids",
             "merged_durable_rows", "retired_durable_rows", "tool_call_ids", "own_row",
             "retired_row"]
HM_REQ = ["role", "content"]
PRC_FIELDS = ["original", "replacement", "before"]
MESSAGE_OPTIONALS = ["tool_name", "tool_call_id", "display_kind", "api_content",
                     "platform_message_id", "effect_disposition", "finish_reason",
                     "reasoning", "reasoning_content", "reasoning_details",
                     "codex_reasoning_items", "codex_message_items", "token_count",
                     "message_uid"]
WIRE_FIELDS = ["role", "content", "timestamp", "tool_call_id", "tool_calls", "tool_name",
               "effect_disposition", "token_count", "finish_reason", "reasoning",
               "reasoning_content", "reasoning_details", "codex_reasoning_items",
               "codex_message_items", "platform_message_id", "api_content", "display_kind",
               "display_metadata", "message_uid", "absorbed_message_uids", "tool_call_uids",
               "tool_call_uid"]
TR_FIELDS = ["id", "session_id", "role", "content", "timestamp", "visibility",
             "tool_call_id", "tool_calls", "tool_name", "effect_disposition",
             "token_count", "finish_reason", "reasoning", "reasoning_content",
             "reasoning_details", "codex_reasoning_items", "codex_message_items",
             "platform_message_id", "observed", "compressed_summary", "api_content",
             "display_kind", "display_metadata", "message_uid", "absorbed_message_uids",
             "tool_call_uids", "tool_call_uid"]
SIDECARS = ["reasoning_details", "codex_reasoning_items", "codex_message_items"]
MR_FIELDS = ["id", "role", "content", "timestamp", "tool_calls", "tool_name",
             "tool_call_id", "display_kind", "compressed_summary", "visibility",
             "api_content", "display_metadata", "platform_message_id", "observed",
             "effect_disposition", "finish_reason", "reasoning", "reasoning_content",
             "reasoning_details", "codex_reasoning_items", "codex_message_items",
             "message_uid"]
CM_FIELDS = ["role", "content", "db_row_snapshot", "row_id", "message_uid",
             "tool_call_uids", "tool_call_uid", "absorbed_message_uids", "api_content",
             "display_kind", "display_metadata", "data_use", "compressed_summary",
             "timestamp", "tool_call_id", "tool_name", "effect_disposition", "tool_calls",
             "message_id", "observed", "finish_reason", "reasoning", "reasoning_content",
             "reasoning_details", "codex_reasoning_items", "codex_message_items",
             "absorbed_row_ids", "merged_turn_prefix", "merged_durable_rows",
             "unnamed_durable_rows", "retired_durable_rows", "db_persisted",
             "_tool_call_uid_stored"]
CM_REQ = ["role", "content"]


def cm_default(name):
    if name in ("absorbed_message_uids", "data_use", "absorbed_row_ids"):
        return []
    if name in ("compressed_summary", "observed", "_tool_call_uid_stored"):
        return False
    if name == "db_persisted":
        return True
    return None


# ── ported leaves (literal transcriptions) ──

def m_visibility(active, compacted):
    if truth(active):
        return "active"
    return "compaction_archived" if truth(compacted) else "rewound"


def m_sidecar(value):
    if not isinstance(value, str):
        return cp(value)
    parsed = json_loads(value)
    if parsed is None:
        # NULL == ValueError catch: the text itself. (A literal JSON "null"
        # parses to None too — sm_json_loads returns a JSON_NULL node, NOT
        # NULL; mirror distinguishes via the validator.)
        p = JParse(value)
        if p.jvalue(0):
            p.ws()
            if p.i == p.n:
                return None  # valid JSON null
        return copy.deepcopy(value)
    return parsed


def m_message_dict(message):
    fields_check(message, "NewMessage", NM_FIELDS, NM_REQ)
    msg = {"role": cp(jget(message, "role")), "content": cp(jget(message, "content"))}
    ts = jget(message, "timestamp")
    if not is_null(ts):
        msg["timestamp"] = copy.deepcopy(ts)
    tcs = jget(message, "tool_calls")
    if truth(tcs):
        msg["tool_calls"] = copy.deepcopy(tcs)
    for name in MESSAGE_OPTIONALS:
        v = jget(message, name)
        if not is_null(v):
            msg[name] = copy.deepcopy(v)
    dm = jget(message, "display_metadata")
    if not is_null(dm):
        msg["display_metadata"] = copy.deepcopy(dm)
    if truth(jget(message, "compressed_summary")):
        msg["_compressed_summary"] = True
    if truth(jget(message, "observed")):
        msg["observed"] = True
    rt = jget(message, "repair_target")
    if not is_null(rt):
        msg["_row_id"] = py_int(rt)
    snap = jget(message, "db_row_snapshot")
    if not is_null(snap):
        msg["_db_row_snapshot"] = copy.deepcopy(snap)
    amu = jget(message, "absorbed_message_uids")
    if truth(amu):
        msg["_absorbed_message_uids"] = copy.deepcopy(amu)
    tcu = jget(message, "tool_call_uids")
    if not is_null(tcu):
        msg["_tool_call_uids"] = copy.deepcopy(tcu)
    tcu1 = jget(message, "tool_call_uid")
    if not is_null(tcu1):
        msg["_tool_call_uid"] = copy.deepcopy(tcu1)
    du = jget(message, "data_use")
    if truth(du):
        msg["data_use"] = copy.deepcopy(du)
    mdr = jget(message, "merged_durable_rows")
    if truth(mdr):
        msg["_merged_durable_rows"] = copy.deepcopy(mdr)
    udr = jget(message, "unnamed_durable_rows")
    if truth(udr):
        msg["_unnamed_durable_rows"] = copy.deepcopy(udr)
    ari = jget(message, "absorbed_row_ids")
    if truth(ari):
        if not isinstance(ari, list):
            raise iter_err(ari)
        msg["_absorbed_row_ids"] = [py_int(i) for i in ari]
    if truth(jget(message, "restored_from_store")):
        msg["_restored_from_store"] = True
    return msg


def m_held_dict(held):
    fields_check(held, "HeldMessage", HM_FIELDS, HM_REQ)
    msg = {"role": cp(jget(held, "role")), "content": cp(jget(held, "content"))}
    tcid = jget(held, "tool_call_id")
    if not is_null(tcid):
        msg["tool_call_id"] = copy.deepcopy(tcid)
    tcs = jget(held, "tool_calls")
    if truth(tcs):
        msg["tool_calls"] = copy.deepcopy(tcs)
    ts = jget(held, "timestamp")
    if not is_null(ts):
        msg["timestamp"] = copy.deepcopy(ts)
    rid = jget(held, "row_id")
    if not is_null(rid):
        msg["_row_id"] = py_int(rid)
    if truth(jget(held, "persisted")):
        msg["_db_persisted"] = True
    snap = jget(held, "db_row_snapshot")
    if not is_null(snap):
        msg["_db_row_snapshot"] = copy.deepcopy(snap)
    uid = jget(held, "message_uid")
    if not is_null(uid):
        msg["message_uid"] = copy.deepcopy(uid)
    ari = jget(held, "absorbed_row_ids")
    if truth(ari):
        if not isinstance(ari, list):
            raise iter_err(ari)
        msg["_absorbed_row_ids"] = [py_int(i) for i in ari]
    mdr = jget(held, "merged_durable_rows")
    if truth(mdr):
        msg["_merged_durable_rows"] = copy.deepcopy(mdr)
    rdr = jget(held, "retired_durable_rows")
    if truth(rdr):
        if not isinstance(rdr, list):
            raise iter_err(rdr)
        msg["_retired_durable_rows"] = [m_held_dict(r) for r in rdr]
    tcids = jget(held, "tool_call_ids")
    if truth(tcids):
        msg["tool_call_ids"] = copy.deepcopy(tcids)
    if truth(jget(held, "own_row")):
        msg["_own_row"] = True
    if truth(jget(held, "retired_row")):
        msg["_retired_row"] = True
    return msg


def m_pruned_original(change):
    fields_check(change, "PrunedRowChange", PRC_FIELDS, PRC_FIELDS)
    before_full = m_message_dict(jget(change, "before"))
    before = {k: copy.deepcopy(v) for k, v in before_full.items()
              if k not in ("_row_id", "_db_row_snapshot")}
    held = m_held_dict(jget(change, "original"))
    obj_update(before, held)
    return before


def m_transcript_row(row):
    if not isinstance(row, dict):
        raise get_attr_err(row)
    fields = {name: cp(jget(row, name)) for name in WIRE_FIELDS}
    if not jhas(row, "id"):
        raise raise_key("id")
    idv = py_int(jget(row, "id"))
    if not jhas(row, "session_id"):
        raise raise_key("session_id")
    out = {"id": idv, "session_id": cp(jget(row, "session_id")),
           "role": fields["role"], "content": fields["content"],
           "timestamp": fields["timestamp"],
           "visibility": m_visibility(jget(row, "active"), jget(row, "compacted"))}
    for name in TR_FIELDS[6:18]:
        out[name] = fields[name]
    out["observed"] = truth(jget(row, "observed"))
    cs = jget(row, "_compressed_summary")
    out["compressed_summary"] = isinstance(cs, bool) and cs
    for name in TR_FIELDS[20:]:
        out[name] = fields[name]
    return out


def m_message_record(row):
    if not isinstance(row, dict):
        raise get_attr_err(row)
    meta_in = jget(row, "display_metadata")
    if isinstance(meta_in, dict):
        metadata = {k: copy.deepcopy(v) for k, v in meta_in.items() if k != "reactions"}
        if not metadata:
            metadata = None
    else:
        metadata = cp(meta_in)
    if not jhas(row, "id"):
        raise raise_key("id")
    idv = py_int(jget(row, "id"))
    if not jhas(row, "role"):
        raise raise_key("role")
    if not jhas(row, "timestamp"):
        raise raise_key("timestamp")
    ts = py_float(jget(row, "timestamp"))
    tcs = jget(row, "tool_calls")
    if not truth(tcs):
        tool_calls = []
    elif isinstance(tcs, list):
        tool_calls = copy.deepcopy(tcs)
    else:
        raise iter_err(tcs)
    out = {"id": idv, "role": cp(jget(row, "role")), "content": cp(jget(row, "content")),
           "timestamp": ts, "tool_calls": tool_calls,
           "tool_name": cp(jget(row, "tool_name")),
           "tool_call_id": cp(jget(row, "tool_call_id")),
           "display_kind": cp(jget(row, "display_kind")),
           "compressed_summary": truth(jget(row, "_compressed_summary")),
           "visibility": m_visibility(jget(row, "active"), jget(row, "compacted")),
           "api_content": cp(jget(row, "api_content")),
           "display_metadata": metadata,
           "platform_message_id": cp(jget(row, "platform_message_id")),
           "observed": truth(jget(row, "observed")),
           "effect_disposition": cp(jget(row, "effect_disposition")),
           "finish_reason": cp(jget(row, "finish_reason")),
           "reasoning": cp(jget(row, "reasoning")),
           "reasoning_content": cp(jget(row, "reasoning_content"))}
    for name in SIDECARS:
        out[name] = m_sidecar(jget(row, name))
    out["message_uid"] = cp(jget(row, "message_uid"))
    return out


def m_stamp(msg, stored):
    if not isinstance(msg, dict):
        raise subscript_err(msg)
    if not jhas(msg, "_row_id"):
        raise raise_key("_row_id")
    row_id = py_int(jget(msg, "_row_id"))
    keybuf = "%d" % int(row_id)
    ts = jget(msg, "timestamp")
    if is_null(ts):
        if not isinstance(stored, dict):
            raise subscript_err(stored)
        srow = jget(stored, keybuf)
        if srow is MISSING:
            raise raise_key_int(row_id)
        if not isinstance(srow, dict):
            raise subscript_err(srow)
        if not jhas(srow, "timestamp"):
            raise raise_key("timestamp")
        ts = jget(srow, "timestamp")
    timestamp = py_float(ts)
    uid = jget(msg, "message_uid")
    if truth(uid):
        uid_out = copy.deepcopy(uid)
    else:
        srow = jget(stored, keybuf) if isinstance(stored, dict) else MISSING
        suid = MISSING
        if srow is not MISSING:
            if not isinstance(srow, dict):
                raise get_attr_err(srow)
            suid = jget(srow, "message_uid")
        uid_out = copy.deepcopy(suid) if truth(suid) else ""
    return {"id": row_id, "timestamp": timestamp,
            "db_row_snapshot": cp(jget(msg, "_db_row_snapshot")),
            "message_uid": uid_out,
            "tool_call_uids": cp(jget(msg, "_tool_call_uids")),
            "tool_call_uid": cp(jget(msg, "_tool_call_uid"))}


def m_row_outcomes(rows, count):
    if not isinstance(rows, list):
        raise iter_err(rows)
    ids = []
    for row in rows:
        if not isinstance(row, dict):
            raise subscript_err(row)
        if not jhas(row, "_row_id"):
            raise raise_key("_row_id")
        d = py_int(jget(row, "_row_id"))
        if d not in ids:
            ids.append(d)
    ids.sort(reverse=True)
    stop = len(ids)
    if not is_null(count):
        if isinstance(count, bool):
            c = 1 if count else 0
        elif isinstance(count, (int, float)) and float(count) == math.trunc(count):
            c = count
        else:
            raise raise_("TypeError",
                         "slice indices must be integers or None or have an __index__ method")
        if c >= 0:
            stop = min(int(c), len(ids))
        else:
            s = len(ids) + c
            stop = int(s) if s > 0 else 0
    out = []
    for row in rows:
        row_id = py_int(jget(row, "_row_id"))
        canonical = jget(row, "_canonical_row")
        fresh = row_id in ids[:stop]
        if fresh:
            disposition = "appended"
        elif not is_null(canonical):
            if not isinstance(canonical, dict):
                raise get_attr_err(canonical)
            disposition = "repaired" if truth(jget(canonical, "_metadata_only")) else "adopted"
        else:
            disposition = "repaired"
        if not jhas(row, "timestamp"):
            raise raise_key("timestamp")
        ts = py_float(jget(row, "timestamp"))
        out.append({"id": row_id, "disposition": disposition, "timestamp": ts,
                    "db_row_snapshot": cp(jget(row, "_db_row_snapshot")),
                    "canonical_row": cp(canonical),
                    "message_uid": cp(jget(row, "message_uid")),
                    "tool_call_uids": cp(jget(row, "_tool_call_uids")),
                    "tool_call_uid": cp(jget(row, "_tool_call_uid"))})
    return out


def m_conversation(messages):
    if not isinstance(messages, list):
        raise iter_err(messages)
    out = []
    for m in messages:
        recd = seam("from_openai_message", {"message": cp(m)})
        fields_check(recd, "ConversationMessage", CM_FIELDS, CM_REQ)
        proj = {}
        for name in CM_FIELDS:
            v = jget(recd, name)
            proj[name] = copy.deepcopy(v) if v is not MISSING else cm_default(name)
        out.append(proj)
    return out

# ── _refusals conversions + flows ──

REWIND_REFUSALS = {
    "session history changed before the rewind could be persisted": "history_changed",
    "active transcript changed before the rewind could be persisted": "history_changed",
    "rewind target has no durable row identity": "target_unidentified",
    "rewind target changed before it could be persisted": "target_changed",
    "rewind did not retain its compaction handoff": "handoff_lost",
}


def refusals_transform(sid, spec):
    kind = kind_of(spec)
    if is_persistence(kind):
        if sid is not None:
            cause = spec.get("cause")
            a = {"cause": exc_project(cause) if cause is not None else None}
            bucket = seam("classify_persistence_error", a)
            if isinstance(bucket, str) and bucket == "session_row_missing":
                spec.clear()
                spec.update({"__raise__": "SessionRowMissing",
                             "message": "FOREIGN KEY constraint failed: session %s has no row"
                                        % py_repr(sid)})
        return
    if is_runtime(kind):
        text = spec.get("message", "")
        if sid is not None and text in REWIND_REFUSALS:
            spec.clear()
            spec.update({"__raise__": "RewindRefused", "message": text})


def refusals_wrap(sid, body_fn):
    refusals_enter(sid)
    try:
        ret = body_fn()
    except MExc as e:
        refusals_exit(sid, e.spec)
        refusals_transform(sid, e.spec)
        raise
    refusals_exit(sid, None)
    return ret


def sid_of(inp):
    v = jget(inp, "session_id")
    return v if isinstance(v, str) else None


def m_refusals(inp):
    sid = sid_of(inp)
    refusals_enter(sid)
    body = jget(inp, "body")
    rspec = jget(body, "raise") if body is not MISSING else MISSING
    if isinstance(rspec, dict) and "__raise__" in rspec:
        spec = copy.deepcopy(rspec)
        refusals_exit(sid, spec)
        refusals_transform(sid, spec)
        raise MExc(spec)
    refusals_exit(sid, None)
    return cp(jget(body, "ret")) if body is not MISSING else None


def apply_stamps(dicts, script):
    if not isinstance(script, dict):
        return
    stamps = jget(script, "stamps")
    if not isinstance(stamps, list):
        return
    for msg, stamp in zip(dicts, stamps):
        obj_update(msg, stamp)


def m_append_chunk(sid, messages, guards, patience):
    if not truth(messages):
        return 0
    if not isinstance(messages, list):
        raise iter_err(messages)
    etw_enter(messages, patience)
    inserted = None
    try:
        script = seam("_append_messages_in_transaction",
                      {"args": [sid, copy.deepcopy(messages)], "kwargs": cp(guards)})
        if script is not None and not isinstance(script, dict):
            raise get_attr_err(script)
        if script is not None:
            apply_stamps(messages, script)
            inserted = cp(jget(script, "inserted"))
        for msg in messages:
            if not truth(jget(msg, "_restored_from_store")) \
                    or not truth(jget(msg, "_db_persisted")) or jhas(msg, "_row_id"):
                continue
            if not jhas(msg, "role"):
                raise raise_key("role")
            uid = seam("message_uid_or_none", {"msg": copy.deepcopy(msg)})
            row = seam("_active_logical_message_row",
                       {"session_id": sid, "role": cp(jget(msg, "role")),
                        "message_uid": cp(uid)})
            if not isinstance(row, dict):
                raise subscript_err(row)
            if not jhas(row, "id"):
                raise raise_key("id")
            rid = py_int(jget(row, "id"))
            if not jhas(row, "timestamp"):
                raise raise_key("timestamp")
            ts = copy.deepcopy(jget(row, "timestamp"))
            snapshot = seam("transcript_row_snapshot", {"row": copy.deepcopy(row)})
            msg["_row_id"] = rid
            msg["timestamp"] = ts
            msg["_db_row_snapshot"] = cp(snapshot)
            iscript = seam("_restore_row_identity",
                           {"row": copy.deepcopy(row), "msg": copy.deepcopy(msg)})
            if iscript is not None and not isinstance(iscript, dict):
                raise get_attr_err(iscript)
            if iscript is not None:
                obj_update(msg, jget(iscript, "msg"))
    except MExc as e:
        etw_exit(e.spec)
        raise
    etw_exit(None)
    return inserted


def m_append(inp):
    sid = sid_of(inp)
    spec = jget(inp, "message")
    msg = m_message_dict(spec)
    ttl = jget(inp, "turn_lease_ttl_seconds")

    def body():
        kw = {
            "content": cp(jget(spec, "content")),
            "tool_name": cp(jget(spec, "tool_name")),
            "tool_calls": cp(jget(msg, "tool_calls")),
            "tool_call_id": cp(jget(spec, "tool_call_id")),
            "token_count": cp(jget(spec, "token_count")),
            "finish_reason": cp(jget(spec, "finish_reason")),
            "reasoning": cp(jget(spec, "reasoning")),
            "reasoning_content": cp(jget(spec, "reasoning_content")),
            "reasoning_details": cp(jget(spec, "reasoning_details")),
            "codex_reasoning_items": cp(jget(spec, "codex_reasoning_items")),
            "codex_message_items": cp(jget(spec, "codex_message_items")),
            "platform_message_id": cp(jget(spec, "platform_message_id")),
            "observed": cp(jget(spec, "observed")) if jget(spec, "observed") is not MISSING else False,
            "effect_disposition": cp(jget(spec, "effect_disposition")),
            "_compressed_summary": cp(jget(spec, "compressed_summary")) if jget(spec, "compressed_summary") is not MISSING else False,
            "timestamp": cp(jget(spec, "timestamp")),
            "api_content": cp(jget(spec, "api_content")),
            "display_kind": cp(jget(spec, "display_kind")),
            "display_metadata": cp(jget(msg, "display_metadata")),
            "turn_lease_holder": cp(jget(inp, "turn_lease_holder")),
            "turn_lease_ttl_seconds": copy.deepcopy(ttl) if ttl is not MISSING else 300.0,
            "message_uid": cp(jget(spec, "message_uid")),
        }
        return seam("append_message", {"args": [cp(jget(inp, "session_id")),
                                                cp(jget(spec, "role"))], "kwargs": kw})
    return refusals_wrap(sid, body)


def row_uids(msg):
    uid = jget(msg, "message_uid")
    return {"message_uid": copy.deepcopy(uid) if truth(uid) else None,
            "tool_call_uids": cp(jget(msg, "_tool_call_uids")),
            "tool_call_uid": cp(jget(msg, "_tool_call_uid"))}


def m_append_batch(inp):
    sid = sid_of(inp)
    specs = jget(inp, "messages")
    if not isinstance(specs, list):
        raise iter_err(specs)
    dicts = [m_message_dict(s) for s in specs]
    patience = inp.get("patience_s", 30.0)
    ttl = jget(inp, "turn_lease_ttl_seconds")
    guards = {"turn_lease_holder": cp(jget(inp, "turn_lease_holder")),
              "turn_lease_ttl_seconds": copy.deepcopy(ttl) if ttl is not MISSING else 300.0}
    cr = jget(inp, "chunk_rows")
    split = False
    step = 0
    if not is_null(cr):
        if isinstance(cr, bool):
            crv = 1 if cr else 0
        elif isinstance(cr, (int, float)):
            crv = cr
        else:
            raise raise_("TypeError",
                         "'>' not supported between instances of 'int' and '%s'" % typename(cr))
        split = len(dicts) > crv
    if split:
        index = {}
        for msg in dicts:
            parsed = seam("_parse_tool_calls", {"tool_calls": cp(jget(msg, "tool_calls"))})
            script = seam("_stamp_tool_call_uids",
                          {"msg": copy.deepcopy(msg), "tool_calls": cp(parsed),
                           "index": copy.deepcopy(index)})
            if script is not None and not isinstance(script, dict):
                raise get_attr_err(script)
            if script is not None:
                obj_update(msg, jget(script, "msg"))
                ni = jget(script, "index")
                index = copy.deepcopy(ni) if isinstance(ni, dict) else {}
        stepv = (1 if cr else 0) if isinstance(cr, bool) else cr
        if float(stepv) != math.trunc(stepv):
            raise raise_("TypeError", "'float' object cannot be interpreted as an integer")
        if stepv == 0:
            raise raise_("ValueError", "range() arg 3 must not be zero")
        if stepv > 0:
            step = int(stepv)
            chunks = [dicts[start:start + step] for start in range(0, len(dicts), step)]
        else:
            chunks = []
    else:
        chunks = [dicts]
    committed = []

    def body():
        for chunk in chunks:
            inserted = m_append_chunk(sid, chunk, guards, patience)
            outcomes = m_row_outcomes(chunk, inserted)
            committed.extend(outcomes)
    try:
        refusals_wrap(sid, body)
    except MExc as e:
        e.spec["batch_progress"] = {
            "outcomes": copy.deepcopy(committed),
            "uids": [row_uids(d) for d in dicts],
        }
        raise
    return {"outcomes": committed}


def m_replace_transcript(inp):
    mode = jget(inp, "mode")
    if not isinstance(mode, str) or mode not in (
            "destructive", "preserve_existing_archive", "archive_dropped"):
        raise enum_invalid("ReplaceMode", mode)
    sid = sid_of(inp)
    specs = jget(inp, "messages")
    if not isinstance(specs, list):
        raise iter_err(specs)
    dicts = []
    for s in specs:
        d = m_message_dict(s)
        d.pop("_db_row_snapshot", None)
        dicts.append(d)

    def body():
        kw = {"active_only": mode == "preserve_existing_archive",
              "archive_dropped": mode == "archive_dropped",
              "reject_active_turn_lease": inp.get("reject_active_turn_lease", False)}
        script = seam("replace_messages",
                      {"args": [cp(jget(inp, "session_id")), copy.deepcopy(dicts)],
                       "kwargs": kw})
        if script is not None and not isinstance(script, dict):
            raise get_attr_err(script)
        apply_stamps(dicts, script)
        kept = any(is_null(jget(m, "timestamp")) or not truth(jget(m, "message_uid"))
                   for m in dicts)
        stored = {}
        if kept:
            rows = seam("get_messages",
                        {"args": [cp(jget(inp, "session_id"))], "kwargs": {}})
            if not isinstance(rows, list):
                raise iter_err(rows)
            for r in rows:
                if not isinstance(r, dict):
                    raise subscript_err(r)
                if not jhas(r, "id"):
                    raise raise_key("id")
                rid = py_int(jget(r, "id"))
                stored["%d" % int(rid)] = copy.deepcopy(r)
        return stored
    stored = refusals_wrap(sid, body)
    rows_out = [m_stamp(m, stored) for m in dicts]
    return {"rows": rows_out}


WM_FIELDS = ["last_active"]
HC_FIELDS = ["covered_ids", "unresolved"]


def held_list(specs):
    out = []
    if is_null(specs):
        return out
    if not isinstance(specs, list):
        raise iter_err(specs)
    for s in specs:
        out.append(m_held_dict(s))
    return out


def held_list_check(specs):
    if is_null(specs):
        return
    if not isinstance(specs, list):
        raise iter_err(specs)
    for s in specs:
        fields_check(s, "HeldMessage", HM_FIELDS, HM_REQ)


def m_compact(inp):
    sid = sid_of(inp)
    specs = jget(inp, "replacement")
    if not isinstance(specs, list):
        raise iter_err(specs)
    dicts = [m_message_dict(s) for s in specs]
    wm = jget(inp, "watermark")
    if not is_null(wm):
        fields_check(wm, "Watermark", WM_FIELDS, WM_FIELDS)
    carried = jget(inp, "carried")
    held_list_check(carried)
    coverage = jget(inp, "coverage")
    if not is_null(coverage):
        fields_check(coverage, "HeldCoverage", HC_FIELDS, [])
        held_list_check(jget(coverage, "unresolved"))
    hfc = jget(inp, "held_for_cas")
    if not is_null(hfc):
        held_list_check(hfc)

    def body():
        kw = {}
        mcp = jget(inp, "model_config_patch")
        kw["model_config_patch"] = None if is_null(mcp) else copy.deepcopy(mcp)
        if is_null(wm):
            kw["watermark"] = None
        else:
            kw["watermark"] = int_or_0(jget(wm, "last_active"))
        kw["lock_holder"] = cp(jget(inp, "lock_holder"))
        tc = jget(inp, "carried_tail_count")
        kw["tail_count"] = copy.deepcopy(tc) if tc is not MISSING else 0
        carried_dicts = held_list(carried)
        kw["carried_messages"] = carried_dicts if carried_dicts else None
        if is_null(coverage):
            kw["covered_ids"] = None
            kw["unresolved_held"] = None
        else:
            cids = jget(coverage, "covered_ids")
            ids = []
            if not is_null(cids):
                if not isinstance(cids, list):
                    raise iter_err(cids)
                ids = [py_int(i) for i in cids]
            kw["covered_ids"] = ids
            kw["unresolved_held"] = held_list(jget(coverage, "unresolved"))
        if is_null(hfc):
            kw["held_for_cas"] = None
        else:
            kw["held_for_cas"] = held_list(hfc)
        script = seam("archive_and_compact",
                      {"args": [cp(jget(inp, "session_id")), copy.deepcopy(dicts)],
                       "kwargs": kw})
        active = None
        if script is not None and not isinstance(script, dict):
            raise get_attr_err(script)
        if script is not None:
            apply_stamps(dicts, script)
            active = cp(jget(script, "active"))
        return active
    active = refusals_wrap(sid, body)
    rows_out = [m_stamp(m, {}) for m in dicts]
    return {"active_count": active, "rows": rows_out}


def m_rewrite_pruned_rows(inp):
    sid = sid_of(inp)
    changes = jget(inp, "changes")
    if not isinstance(changes, list):
        raise iter_err(changes)
    pairs = []
    for c in changes:
        original = m_pruned_original(c)
        replacement = m_message_dict(jget(c, "replacement") if isinstance(c, dict) else MISSING)
        replacement.pop("_db_row_snapshot", None)
        pairs.append([original, replacement])

    def body():
        kw = {}
        mcp = jget(inp, "model_config_patch")
        kw["model_config_patch"] = None if is_null(mcp) else copy.deepcopy(mcp)
        kw["turn_lease_holder"] = cp(jget(inp, "turn_lease_holder"))
        ttl = jget(inp, "turn_lease_ttl_seconds")
        kw["turn_lease_ttl_seconds"] = copy.deepcopy(ttl) if ttl is not MISSING else 300.0
        script = seam("rewrite_pruned_rows",
                      {"args": [cp(jget(inp, "session_id")), copy.deepcopy(pairs)],
                       "kwargs": kw})
        rewritten = None
        if script is not None and not isinstance(script, dict):
            raise get_attr_err(script)
        if script is not None:
            stamps = jget(script, "stamps")
            if isinstance(stamps, list):
                for pair, stamp in zip(pairs, stamps):
                    obj_update(pair[1], stamp)
            rewritten = cp(jget(script, "rewritten"))
        return rewritten
    rewritten = refusals_wrap(sid, body)
    snapshots = [cp(jget(p[1], "_db_row_snapshot")) for p in pairs]
    return {"rewritten": rewritten, "snapshots": snapshots}


def m_set_reaction(inp):
    sid = sid_of(inp)

    def body():
        return seam("set_message_reaction",
                    {"args": [cp(jget(inp, "session_id")), cp(jget(inp, "message_id")),
                              cp(jget(inp, "emoji"))],
                     "kwargs": {"author": cp(jget(inp, "author"))}})
    reactions = refusals_wrap(sid, body)
    if is_null(reactions):
        return None
    if not isinstance(reactions, list):
        raise iter_err(reactions)
    out = []
    for r in reactions:
        if not isinstance(r, dict):
            raise get_attr_err(r)
        at = float_or_0(jget(r, "at"))
        out.append({"emoji": cp(jget(r, "emoji")), "author": cp(jget(r, "author")),
                    "at": at, "seen": truth(jget(r, "seen"))})
    return out


def m_take_unseen_reactions(inp):
    sid = sid_of(inp)
    author = jget(inp, "author")

    def body():
        return seam("take_unseen_reactions",
                    {"args": [cp(jget(inp, "session_id"))],
                     "kwargs": {"author": copy.deepcopy(author) if author is not MISSING else "user"}})
    pending = refusals_wrap(sid, body)
    if not isinstance(pending, list):
        raise iter_err(pending)
    out = []
    keys = ["row_id", "role", "emoji", "text"]
    for p in pending:
        if not isinstance(p, dict):
            raise subscript_err(p)
        e = {}
        for k in keys:
            if not jhas(p, k):
                raise raise_key(k)
            e[k] = cp(jget(p, k))
        out.append(e)
    return out


TREQ_FIELDS = ["view", "limit", "offset", "latest", "after_id", "include_lineage"]


def m_read_transcript(inp):
    req = jget(inp, "request")
    if not is_null(req):
        fields_check(req, "TranscriptRead", TREQ_FIELDS, [])
    view = jget(req, "view") if req is not MISSING else MISSING
    if not is_null(view) and not isinstance(view, str):
        raise enum_invalid("TranscriptView", view)
    view_s = "active" if is_null(view) else view
    if view_s not in ("active", "display", "all"):
        raise enum_invalid("TranscriptView", view)
    sid = sid_of(inp)

    def body():
        kw = {"include_inactive": view_s == "all", "include_compacted": view_s == "display",
              "limit": cp(jget(req, "limit")) if req is not MISSING else None}
        offset = jget(req, "offset") if req is not MISSING else MISSING
        kw["offset"] = copy.deepcopy(offset) if offset is not MISSING else 0
        latest = jget(req, "latest") if req is not MISSING else MISSING
        kw["latest"] = copy.deepcopy(latest) if latest is not MISSING else False
        after = jget(req, "after_id") if req is not MISSING else MISSING
        kw["after_id"] = None if is_null(after) else py_int(after)
        lineage = jget(req, "include_lineage") if req is not MISSING else MISSING
        kw["include_ancestors"] = copy.deepcopy(lineage) if lineage is not MISSING else False
        return seam("get_messages", {"args": [cp(jget(inp, "session_id"))], "kwargs": kw})
    rows = refusals_wrap(sid, body)
    if not isinstance(rows, list):
        raise iter_err(rows)
    return [m_transcript_row(r) for r in rows]


def m_active_watermark(inp):
    sid = sid_of(inp)

    def body():
        return seam("get_active_message_watermark",
                    {"args": [cp(jget(inp, "session_id"))], "kwargs": {}})
    last = refusals_wrap(sid, body)
    return {"last_active": last if truth(last) else None}


def m_conversation_revision(inp):
    sid = sid_of(inp)

    def body():
        r = seam("conversation_revision",
                 {"args": [cp(jget(inp, "session_id"))],
                  "kwargs": {"include_ancestors": inp.get("include_lineage", False)}})
        # the unpack fires INSIDE the with block
        if not isinstance(r, list):
            raise raise_("TypeError", "cannot unpack non-iterable %s object" % typename(r))
        if len(r) != 4:
            if len(r) < 4:
                raise raise_("ValueError",
                             "not enough values to unpack (expected 4, got %d)" % len(r))
            raise raise_("ValueError",
                         "too many values to unpack (expected 4, got %d)" % len(r))
        return r
    r = refusals_wrap(sid, body)
    ids = r[0]
    if not isinstance(ids, list):
        raise iter_err(ids)
    return {"lineage_ids": copy.deepcopy(ids), "active_count": cp(r[1]),
            "max_active_id": cp(r[2]), "transcript_revision": cp(r[3])}


def m_read_around(inp):
    sid = sid_of(inp)
    window = jget(inp, "window")

    def body():
        return seam("get_messages_around",
                    {"args": [cp(jget(inp, "session_id")), cp(jget(inp, "anchor"))],
                     "kwargs": {"window": copy.deepcopy(window) if window is not MISSING else 5}})
    around = refusals_wrap(sid, body)
    if not isinstance(around, dict):
        raise subscript_err(around)
    if not jhas(around, "window"):
        raise raise_key("window")
    win = jget(around, "window")
    if not isinstance(win, list):
        raise iter_err(win)
    records = [m_message_record(r) for r in win]
    if not jhas(around, "messages_before"):
        raise raise_key("messages_before")
    if not jhas(around, "messages_after"):
        raise raise_key("messages_after")
    return {"window": records, "before": cp(jget(around, "messages_before")),
            "after": cp(jget(around, "messages_after"))}


def m_read_session_timeline(inp):
    sid = sid_of(inp)
    limit = jget(inp, "limit")
    after = jget(inp, "after_row_id")

    def body():
        return seam("get_session_timeline",
                    {"args": [cp(jget(inp, "session_id"))],
                     "kwargs": {"limit": copy.deepcopy(limit) if limit is not MISSING else 500,
                                "after_row_id": copy.deepcopy(after) if after is not MISSING else 0}})
    result = refusals_wrap(sid, body)
    if not isinstance(result, dict):
        raise subscript_err(result)
    if not jhas(result, "pagination"):
        raise raise_key("pagination")
    page = jget(result, "pagination")
    if not jhas(result, "entries"):
        raise raise_key("entries")
    entries = jget(result, "entries")
    if not isinstance(entries, list):
        raise iter_err(entries)
    entries_out = []
    ekeys = ["row_id", "preview", "timestamp"]
    for e in entries:
        if not isinstance(e, dict):
            raise subscript_err(e)
        eo = {}
        for k in ekeys:
            if not jhas(e, k):
                raise raise_key(k)
            eo[k] = cp(jget(e, k))
        entries_out.append(eo)
    if not isinstance(page, dict):
        raise subscript_err(page)
    pkeys = ["limit", "after_row_id", "returned", "total", "has_more", "next_cursor"]
    out = {"entries": entries_out}
    for k in pkeys:
        if not jhas(page, k):
            raise raise_key(k)
        out[k] = cp(jget(page, k))
    return out


def m_read_prompt_page(inp):
    sid = sid_of(inp)
    limit = jget(inp, "limit")

    def body():
        return seam("get_session_messages_around",
                    {"args": [cp(jget(inp, "session_id")), cp(jget(inp, "row_id"))],
                     "kwargs": {"limit": copy.deepcopy(limit) if limit is not MISSING else 120}})
    result = refusals_wrap(sid, body)
    if is_null(result):
        return None
    if not isinstance(result, dict):
        raise subscript_err(result)
    if not jhas(result, "pagination"):
        raise raise_key("pagination")
    page = jget(result, "pagination")
    if not jhas(result, "messages"):
        raise raise_key("messages")
    msgs = jget(result, "messages")
    if not isinstance(msgs, list):
        raise iter_err(msgs)
    rows_out = [m_transcript_row(r) for r in msgs]
    if not isinstance(page, dict):
        raise subscript_err(page)
    pkeys = ["row_id", "limit", "returned", "offset", "total", "has_older", "has_newer"]
    out = {"messages": rows_out}
    for k in pkeys:
        if not jhas(page, k):
            raise raise_key(k)
        out[k] = cp(jget(page, k))
    return out


def m_cooldown_snapshot(inp):
    sid = sid_of(inp)

    def body():
        return seam("get_compression_failure_cooldown_row",
                    {"args": [cp(jget(inp, "session_id"))], "kwargs": {}})
    row = refusals_wrap(sid, body)
    if not isinstance(row, dict):
        raise subscript_err(row)
    if not truth(jget(row, "session_exists")):
        return {"present": False, "until": None, "error": None}
    if not jhas(row, "cooldown_until"):
        raise raise_key("cooldown_until")
    if not jhas(row, "error"):
        raise raise_key("error")
    return {"present": True, "until": cp(jget(row, "cooldown_until")),
            "error": cp(jget(row, "error"))}


def m_read_compression_cooldown(inp):
    snapshot = m_cooldown_snapshot(inp)
    until = jget(snapshot, "until")
    if is_null(until):
        return None
    u = py_float(until)
    at = jget(inp, "at")
    if not isinstance(at, (int, float)) or isinstance(at, bool):
        raise raise_("TypeError",
                     "'<=' not supported between instances of 'float' and '%s'" % typename(at))
    if u <= at:
        return None
    return {"until": u, "error": cp(jget(snapshot, "error"))}


CS_FIELDS = ["present", "until", "error"]
CS_REQ = ["present"]


def m_restore_compression_cooldown(inp):
    snap = jget(inp, "snapshot")
    fields_check(snap, "CooldownSnapshot", CS_FIELDS, CS_REQ)
    sid = sid_of(inp)
    row = {"session_exists": cp(jget(snap, "present")),
           "cooldown_until": cp(jget(snap, "until")),
           "error": cp(jget(snap, "error"))}

    def body():
        return seam("restore_compression_failure_cooldown_row",
                    {"args": [cp(jget(inp, "session_id")), row], "kwargs": {}})
    try:
        refusals_wrap(sid, body)
    except MExc as e:
        if is_runtime(kind_of(e.spec)) and not is_persistence(kind_of(e.spec)):
            e.spec = {"__raise__": "CooldownRestoreRefused",
                      "message": e.spec.get("message", "")}
        raise
    return None


def m_latest_row_id(inp):
    sid = sid_of(inp)
    role = jget(inp, "role")
    offset = jget(inp, "offset")
    rt = jget(inp, "require_text")

    def body():
        return seam("latest_message_row_id",
                    {"args": [cp(jget(inp, "session_id"))],
                     "kwargs": {"role": copy.deepcopy(role) if role is not MISSING else "user",
                                "offset": copy.deepcopy(offset) if offset is not MISSING else 0,
                                "require_text": copy.deepcopy(rt) if rt is not MISSING else True}})
    return refusals_wrap(sid, body)


def m_message_storage_state(inp):
    mid = jget(inp, "message_id")
    if not truth(mid):
        return None

    def body():
        return seam("conn.execute",
                    {"sql": "SELECT session_id, active, compacted FROM messages WHERE id = ?",
                     "params": [copy.deepcopy(mid)]})
    row = refusals_wrap(None, body)
    if is_null(row):
        return None
    if not isinstance(row, dict):
        raise subscript_err(row)
    if not jhas(row, "session_id"):
        raise raise_key("session_id")
    if not jhas(row, "active"):
        raise raise_key("active")
    if not jhas(row, "compacted"):
        raise raise_key("compacted")
    return {"session_id": cp(jget(row, "session_id")),
            "visibility": m_visibility(jget(row, "active"), jget(row, "compacted"))}


RP_FIELDS = ["target_row_id", "expected_active_ids", "expected_target_content", "scaffold",
             "prefix", "durable_prefix", "live_view", "live_text", "durable_user",
             "user_ordinal"]


def m_plan_user_turn_rewind(inp):
    sid = sid_of(inp)
    wh = jget(inp, "warm_history")
    warm = None
    if not is_null(wh):
        if not isinstance(wh, list):
            raise iter_err(wh)
        warm = []
        for m in wh:
            if not isinstance(m, dict):
                raise iter_err(m)
            warm.append(copy.deepcopy(m))

    def body():
        plan = seam("plan_user_turn_rewind",
                    {"args": [cp(jget(inp, "session_id")), cp(jget(inp, "user_ordinal"))],
                     "kwargs": {"warm_history": warm,
                                "require_retryable": inp.get("require_retryable", False),
                                "require_composite": inp.get("require_composite", False)}})
        # the RewindPlan materialization fires INSIDE the with block
        fields_check(plan, "RewindPlan", RP_FIELDS, RP_FIELDS)
        return plan
    plan = refusals_wrap(sid, body)
    out = {"target_row_id": cp(jget(plan, "target_row_id"))}
    eai = jget(plan, "expected_active_ids")
    if not isinstance(eai, list):
        raise iter_err(eai)
    out["expected_active_ids"] = copy.deepcopy(eai)
    out["expected_target_content"] = cp(jget(plan, "expected_target_content"))
    out["scaffold"] = cp(jget(plan, "scaffold"))
    for k in ("prefix", "durable_prefix"):
        v = jget(plan, k)
        if not isinstance(v, list):
            raise iter_err(v)
        out[k] = copy.deepcopy(v)
    out["live_view"] = cp(jget(plan, "live_view"))
    out["live_text"] = cp(jget(plan, "live_text"))
    du = jget(plan, "durable_user")
    if not isinstance(du, list):
        raise iter_err(du)
    out["durable_user"] = copy.deepcopy(du)
    out["user_ordinal"] = cp(jget(plan, "user_ordinal"))
    return out


def dispatch(func, inp):
    if func == "_visibility":
        return m_visibility(jget(inp, "active"), jget(inp, "compacted"))
    if func == "_sidecar":
        return m_sidecar(jget(inp, "value"))
    if func == "_message_dict":
        return m_message_dict(jget(inp, "message"))
    if func == "_held_dict":
        return m_held_dict(jget(inp, "held"))
    if func == "_pruned_original":
        return m_pruned_original(jget(inp, "change"))
    if func == "_transcript_row":
        return m_transcript_row(jget(inp, "row"))
    if func == "_message_record":
        return m_message_record(jget(inp, "row"))
    if func == "_stamp":
        stored = jget(inp, "stored")
        return m_stamp(jget(inp, "msg"), {} if is_null(stored) else stored)
    if func == "_row_outcomes":
        return m_row_outcomes(jget(inp, "rows"), jget(inp, "count"))
    if func == "_conversation":
        return m_conversation(jget(inp, "messages"))
    if func == "_refusals":
        return m_refusals(inp)
    if func == "_append_chunk":
        guards = jget(inp, "guards")
        return m_append_chunk(sid_of(inp), jget(inp, "messages"),
                              {} if is_null(guards) else guards,
                              inp.get("patience_s", 30.0))
    if func == "append":
        return m_append(inp)
    if func == "append_batch":
        return m_append_batch(inp)
    if func == "replace_transcript":
        return m_replace_transcript(inp)
    if func == "compact":
        return m_compact(inp)
    if func == "rewrite_pruned_rows":
        return m_rewrite_pruned_rows(inp)
    if func == "set_reaction":
        return m_set_reaction(inp)
    if func == "take_unseen_reactions":
        return m_take_unseen_reactions(inp)
    if func == "read_transcript":
        return m_read_transcript(inp)
    if func == "active_watermark":
        return m_active_watermark(inp)
    if func == "conversation_revision":
        return m_conversation_revision(inp)
    if func == "read_around":
        return m_read_around(inp)
    if func == "read_session_timeline":
        return m_read_session_timeline(inp)
    if func == "read_prompt_page":
        return m_read_prompt_page(inp)
    if func == "read_compression_cooldown_snapshot":
        return m_cooldown_snapshot(inp)
    if func == "read_compression_cooldown":
        return m_read_compression_cooldown(inp)
    if func == "restore_compression_cooldown":
        return m_restore_compression_cooldown(inp)
    if func == "latest_row_id":
        return m_latest_row_id(inp)
    if func == "message_storage_state":
        return m_message_storage_state(inp)
    if func == "plan_user_turn_rewind":
        return m_plan_user_turn_rewind(inp)
    raise raise_("ValueError", "unknown func: %s" % func)


def mirror_drive(func, inp):
    CUR["in"] = inp
    CUR["trace"] = []
    CUR["cursors"] = {}
    try:
        ret = dispatch(func, inp)
    except MExc as e:
        ret = exc_project(e.spec)
    return {"ret": ret, "trace": CUR["trace"]}


def main():
    O.extract()
    fix = os.path.join(HERE, "fixtures", "sqlite_messages_flow.jsonl")
    cases = 0
    mism = 0
    with open(fix, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            c = json.loads(line)
            func, inp = c["func"], c["in"]
            cases += 1
            O.CUR["func"] = func
            # the live drive MUTATES the input dicts (stamps): fresh copies
            want = O.drive(copy.deepcopy(inp))
            try:
                got = mirror_drive(func, copy.deepcopy(inp))
            except BaseException as e:
                mism += 1
                print("MIRROR-ERROR %s: %r" % (func, e))
                continue
            if O.canon(want) != O.canon(got):
                mism += 1
                print("SHADOW-MISMATCH %s (case %d)" % (func, cases))
                print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False, default=str)[:600])
                print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False, default=str)[:600])
    print("SHADOW sqlite_messages_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
