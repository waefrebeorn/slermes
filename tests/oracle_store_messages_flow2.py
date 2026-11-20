#!/usr/bin/env python3
"""Oracle for the unclaimed pure-leaf FLOW port store_messages_flow2
(src/persistence/store_messages_flow2.c).

Defining module: persistence/sqlite/conversation/_store/messages.py. Only
functions that a CRLF-aware PoP grep proved UNCLAIMED are driven here; the 13
functions already claimed by hermes_state_*.c / web_server_*.c /
session_search_tool.c are deliberately excluded. The ten driven leaves are:

    _reasoning_json_text, _encode_content, _encode_display_metadata,
    _decode_display_metadata, _reaction_list, _fold_summary_carrier,
    _active_clause, _exact_replayed_user_clone_key, _is_model_only_row,
    _is_explicit_fork_child_row

All ten are self-contained value transforms: their only collaborators are
seeded module constants and two tiny pure helpers (_json_or,
_sanitize_surrogates) plus json/logger. No DB/seam collaborators are reached,
so the recorded trace is empty on both sides and the universal sm2_call seam
is carried only for contract parity.

Live defs are AST-extracted (FunctionDef nodes) into a registered sys.modules
namespace under the future-annotations compiler flag; the heavy quarry import
chain is never imported. Constants/helpers are seeded with their real quarry
values (see header of the C port for provenance).

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE store_messages_flow2: N cases, M mismatches'.
--self-test drives every fixture through the LIVE defs; --shadow-test diffs a
literal Python mirror of the C port against the live drive (expect 0 diffs).
"""
import ast
import json
import math
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
MSG_SRC = os.path.join(Q, "persistence", "sqlite", "conversation", "_store", "messages.py")
_FLAGS = _fut.annotations.compiler_flag

# seeded constants (verified from the quarry modules named in the C port header)
CONTENT_PREFIX = "\x00json:"
REACTIONS_KEY = "reactions"
DISPLAY_ACTIVE_CLAUSE = " AND (active = 1 OR compacted = 1)"
MODEL_ONLY_KEY = "model_only"
PREFIX_TAG = "__prefixed__"          # transport tag for a "\x00json:..." string
_SURROGATE_RE = re.compile(r"[\ud800-\udfff]")


class _NoOpLogger:
    def warning(self, *a, **k):
        pass

    def debug(self, *a, **k):
        pass


LOGGER = _NoOpLogger()
_INVALID = object()  # _json_or sentinel (messages_sql._INVALID)


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


KINDS = [
    "_reasoning_json_text", "_encode_content", "_encode_display_metadata",
    "_decode_display_metadata", "_reaction_list", "_fold_summary_carrier",
    "_active_clause", "_exact_replayed_user_clone_key", "_is_model_only_row",
    "_is_explicit_fork_child_row",
]
MUT_KINDS = {"_fold_summary_carrier": ("first_by_uid",)}


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
            if x.get(PREFIX_TAG) is not None:
                return CONTENT_PREFIX + conv(x[PREFIX_TAG])
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
    "ConnectionRefusedError": ConnectionRefusedError, "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "EncodingWarning": EncodingWarning, "ResourceWarning": ResourceWarning,
    "ExceptionGroup": ExceptionGroup, "BaseExceptionGroup": BaseExceptionGroup,
    "GeneratorExit": GeneratorExit, "KeyboardInterrupt": KeyboardInterrupt,
    "SystemExit": SystemExit,
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


# ── per-case script backend (mirrors C sm2_call; no seams here) ──
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
        if v.get(PREFIX_TAG) is not None:
            return CONTENT_PREFIX + str(v[PREFIX_TAG])
        return {k: deep_mat(x, ctx) for k, x in v.items()}
    if isinstance(v, list):
        return [deep_mat(i, ctx) for i in v]
    return v


def transport(v):
    if isinstance(v, float):
        return {"__float__": True, "v": v}
    if isinstance(v, tuple):
        return {"__tuple__": True, "v": [transport(i) for i in v]}
    if isinstance(v, str) and v.startswith(CONTENT_PREFIX):
        return {PREFIX_TAG: v[len(CONTENT_PREFIX):]}
    if isinstance(v, dict):
        return {k: transport(x) for k, x in v.items()}
    if isinstance(v, list):
        return [transport(i) for i in v]
    return v


SEAM_NAMES = []


# ── seeded helpers (faithful copies of the quarry helpers) ──
def _json_or(raw, fallback, warning):
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, TypeError):
        LOGGER.warning(warning)
        return fallback


def _sanitize_surrogates(text):
    if text.isascii():
        return text
    return _SURROGATE_RE.sub("\ufffd", text)


# ── messages.py leaf namespace (live defs + seeded constants/helpers) ──
def extract_messages():
    mod = new_module("sm2_oracle_messages")
    ns = mod.__dict__
    ns.update({
        "Any": Any, "Optional": Optional, "json": json, "re": re,
        "logger": LOGGER, "_INVALID": _INVALID,
        "_CONTENT_JSON_PREFIX": CONTENT_PREFIX,
        "REACTIONS_METADATA_KEY": REACTIONS_KEY,
        "_DISPLAY_ACTIVE_CLAUSE": DISPLAY_ACTIVE_CLAUSE,
        "MODEL_ONLY_DISPLAY_METADATA_KEY": MODEL_ONLY_KEY,
        "_json_or": _json_or, "_sanitize_surrogates": _sanitize_surrogates,
        "_SURROGATE_RE": _SURROGATE_RE,
    })
    src = open(MSG_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, MSG_SRC)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_store_messages_flow2: messages defs missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_messages()


# ── per-case drive (LIVE defs) ──
def drive(func, inp, ctx=None):
    args = inp.get("args") or {}
    if ctx is None:
        ctx = Ctx(inp.get("io"))
    mut = {}

    def mat(k, default=None):
        return deep_mat(args.get(k, default), ctx)

    try:
        if func == "_reasoning_json_text":
            ret = NS["_reasoning_json_text"](mat("value"))
        elif func == "_encode_content":
            ret = NS["_encode_content"](mat("content"))
        elif func == "_encode_display_metadata":
            ret = NS["_encode_display_metadata"](mat("display_metadata"))
        elif func == "_decode_display_metadata":
            ret = NS["_decode_display_metadata"](mat("raw"))
        elif func == "_reaction_list":
            ret = NS["_reaction_list"](None, mat("meta"))
        elif func == "_fold_summary_carrier":
            mut["first_by_uid"] = mat("first_by_uid", {})
            ret = NS["_fold_summary_carrier"](mat("row"), mat("key"),
                                              mut["first_by_uid"])
        elif func == "_active_clause":
            ret = NS["_active_clause"](mat("include_inactive", False),
                                       mat("include_compacted", False))
        elif func == "_exact_replayed_user_clone_key":
            ret = NS["_exact_replayed_user_clone_key"](mat("timestamp"), mat("content"))
        elif func == "_is_model_only_row":
            ret = NS["_is_model_only_row"](None, mat("row"))
        elif func == "_is_explicit_fork_child_row":
            ret = NS["_is_explicit_fork_child_row"](None, mat("session"),
                                                    include_reset=mat("include_reset", False))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    state = {k: transport(v) for k, v in mut.items()}
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ─────────────────────────────────────────────────────────────────────────────
# SHADOW MIRROR: literal Python transcription of the C sm2_* functions.
# Operates on transported args and emits transported values; diffed vs the
# live drive() over every fixture. Zero diffs proves the C transcription.
# ─────────────────────────────────────────────────────────────────────────────
def _is_dict(v):
    return isinstance(v, dict) and not (
        {"__float__", "__tuple__", PREFIX_TAG} & set(v))


def _is_str(v):
    return isinstance(v, str)


def _is_bool(v):
    return isinstance(v, bool)


def _is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def _typename(v):
    if v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, int):
        return "int"
    if isinstance(v, float):
        return "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return "float"
        if v.get("__tuple__") is True:
            return "tuple"
        if PREFIX_TAG in v:
            return "str"
        return "dict"
    return "object"


def _prefix(s):
    return {PREFIX_TAG: s}


def _untrans(v):
    if isinstance(v, dict):
        if v.get("__float__") is True:
            return float(v.get("v", 0))
        if v.get("__tuple__") is True:
            return tuple(_untrans(i) for i in v.get("v", []))
        if PREFIX_TAG in v:
            return CONTENT_PREFIX + str(v[PREFIX_TAG])
        return {k: _untrans(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_untrans(i) for i in v]
    return v


def _dumps(v, ensure_ascii=True, sort_keys=False, separators=None):
    return json.dumps(_untrans(v), ensure_ascii=ensure_ascii,
                      sort_keys=sort_keys, separators=separators)


def _loads_or_invalid(raw):
    if not isinstance(raw, str):
        return _INVALID
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, TypeError):
        return _INVALID


def _s_decode_display_metadata(raw):
    if raw is None:
        return None
    meta = raw
    for _ in range(2):
        if isinstance(meta, str):
            meta = _loads_or_invalid(meta)
    if meta is _INVALID:
        return None
    if not isinstance(meta, dict):
        return None
    return meta


def _raise(t, m):
    return {"__raise__": t, "message": m}


def shadow(func, args):
    g = args.get

    if func == "_reasoning_json_text":
        v = g("value")
        if not v:
            return None
        if _is_str(v):
            return v
        return transport(_dumps(v))

    if func == "_encode_content":
        c = g("content")
        if _is_str(c):
            c = _sanitize_surrogates(c)
            if not c.startswith(CONTENT_PREFIX):
                return c
        elif c is None or _is_bool(c) or _is_num(c):
            return c
        try:
            return _prefix(_dumps(c))
        except (TypeError, ValueError):
            return _sanitize_surrogates(str(_untrans(c)))

    if func == "_encode_display_metadata":
        dm = g("display_metadata")
        if not dm:
            return None
        if _is_str(dm):
            parsed = _loads_or_invalid(dm)
            if parsed is _INVALID:
                return None
            if not isinstance(parsed, dict):
                return None
            dm = parsed
        elif not isinstance(dm, dict):
            return None
        return transport(_dumps(dm))

    if func == "_decode_display_metadata":
        return transport(_s_decode_display_metadata(g("raw")))

    if func == "_reaction_list":
        meta = g("meta")
        base = meta if meta else {}
        if not isinstance(base, dict):
            return _raise("AttributeError",
                          f"'{_typename(base)}' object has no attribute 'get'")
        reactions = base.get(REACTIONS_KEY)
        if isinstance(reactions, list):
            return [r for r in reactions if isinstance(r, dict) and not (
                {"__float__", "__tuple__", PREFIX_TAG} & set(r))]
        return []

    if func == "_fold_summary_carrier":
        row = g("row")
        key = g("key")
        fab = g("first_by_uid", {})
        if not isinstance(row, dict) or "message_uid" not in row:
            return _raise("KeyError", "'message_uid'")
        uid = row["message_uid"]
        if not uid:
            return key
        if "_compressed_summary" not in row:
            return _raise("KeyError", "'_compressed_summary'")
        if row["_compressed_summary"]:
            return fab.get(uid, key)
        fab.setdefault(uid, key)
        return key

    if func == "_active_clause":
        if g("include_inactive", False):
            return ""
        if g("include_compacted", False):
            return DISPLAY_ACTIVE_CLAUSE
        return " AND active = 1"

    if func == "_exact_replayed_user_clone_key":
        ts = g("timestamp")
        c = g("content")
        if ts is None or c in (None, "", []):
            return None
        try:
            dumped = _dumps(c, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
            return {"__tuple__": True, "v": [ts, transport(dumped)]}
        except (TypeError, ValueError):
            return None

    if func == "_is_model_only_row":
        row = g("row")
        if not isinstance(row, dict) or "display_metadata" not in row:
            return _raise("KeyError", "'display_metadata'")
        meta = _s_decode_display_metadata(row["display_metadata"])
        base = meta if meta else {}
        return bool(base.get(MODEL_ONLY_KEY)) if isinstance(base, dict) else False

    if func == "_is_explicit_fork_child_row":
        session = g("session")
        if not isinstance(session, dict):
            return _raise("AttributeError",
                          f"'{_typename(session)}' object has no attribute 'get'")
        if session.get("source") == "tool":
            return True
        cfg = session.get("model_config")
        if isinstance(cfg, str):
            try:
                cfg = json.loads(cfg)
            except json.JSONDecodeError:
                return False
        if not isinstance(cfg, dict):
            return False
        markers = [cfg.get("_branched_from"), cfg.get("_delegate_from")]
        if g("include_reset", False):
            markers.append(cfg.get("_reset_from"))
        pid = session.get("parent_session_id")
        if pid:
            return any(pid == m for m in markers)
        return any(m is not None for m in markers)

    raise RuntimeError(f"unknown shadow func {func}")


# ── fixture / cross-check ──
def fixpath():
    return _REPO / "tests" / "fixtures" / "store_messages_flow2.jsonl"


def each_fixture():
    p = fixpath()
    if not p.exists():
        return []
    out = []
    for line in open(p, encoding="utf-8", errors="surrogatepass"):
        line = line.strip()
        if line:
            out.append(json.loads(line))
    return out


def cross_check():
    csrc = open(_REPO / "src" / "persistence" / "store_messages_flow2.c",
                encoding="utf-8").read()
    pop_missing = [k for k in KINDS
                   if f"sm2_{k.lstrip('_')} " not in csrc and f"sm2_{k.lstrip('_')}(" not in csrc]
    if pop_missing:
        print(f"FATAL oracle_store_messages_flow2: PoP funcs absent from C port: {pop_missing}")
        sys.exit(2)
    fix = fixpath()
    if fix.exists():
        counts = {}
        bad_io = set()
        for case in each_fixture():
            f = case.get("func", "")
            counts[f] = counts.get(f, 0) + 1
            bad_io |= set((case.get("in") or {}).get("io") or {})
        if bad_io - set(SEAM_NAMES):
            print(f"FATAL oracle_store_messages_flow2: fixture io keys without a seam: {sorted(bad_io)}")
            sys.exit(2)
        unknown = set(counts) - set(KINDS)
        if unknown:
            print(f"FATAL oracle_store_messages_flow2: fixture funcs without a drive: {sorted(unknown)}")
            sys.exit(2)
        untested = set(KINDS) - set(counts)
        if untested:
            print(f"FATAL oracle_store_messages_flow2: drive funcs without a fixture case: {sorted(untested)}")
            sys.exit(2)
        thin = {f: c for f, c in counts.items() if c < 5}
        if thin:
            print(f"FATAL oracle_store_messages_flow2: funcs with <5 fixture cases: {thin}")
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
    data = sys.stdin.buffer.read().decode("utf-8", errors="surrogatepass")
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
    print(f"ORACLE store_messages_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    n = bad = 0
    counts = {}
    for case in each_fixture():
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    print(f"SELFTEST store_messages_flow2: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


def shadow_test():
    n = bad = 0
    for case in each_fixture():
        n += 1
        func = case["func"]
        args = (case.get("in") or {}).get("args") or {}
        want = drive(func, case.get("in") or {})
        import copy
        shadow_args = copy.deepcopy(args)
        raw = shadow(func, shadow_args)
        got = {"ret": canon(raw), "trace": [], "state": {}}
        if MUT_KINDS.get(func):
            got["state"] = canon({k: shadow_args.get(k) for k in MUT_KINDS[func]})
        g = canon(got)
        if g != want:
            bad += 1
            print(f"SHADOW DIFF {func} case {n}:")
            print(f"  live={json.dumps(want, ensure_ascii=False)[:400]}")
            print(f"  shad={json.dumps(g, ensure_ascii=False)[:400]}")
    print(f"SHADOW store_messages_flow2: {n} cases, {bad} shadow diffs")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow-test":
        sys.exit(shadow_test())
    main()
