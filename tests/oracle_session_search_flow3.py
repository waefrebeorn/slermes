#!/usr/bin/env python3
"""Oracle for the UNCLAIMED FTS5-search result-shaping remainder of
tools/session_search_tool.py ported as a FLOW (src/agent/session_search_flow3.c).

A CRLF-aware PoP grep of src/ confirms these eight Python names are unclaimed:
  _content_text, _redact_derived_value, _redact_tool_calls, _discovery_entry,
  _derived_snippet, _bookend, _discover_payload, _hydrate_hit
The FTS5 core (_discover, _title_match_result, _normalize_title_query,
_shape_message, ...) and the flow2 gap leaves are claimed elsewhere; on the
live side they are SCRIPTED recorders, on the C side universal seams.

The live defs are AST/exec-extracted from a verbatim source slice (SS3_SRC
below) so the oracle is self-contained.  Both sides emit ordered
{"name","args","ret"} trace events, a ret, and a state echo, compared with an
ordered recursive canon (key order counts).

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE session_search_flow3: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- None-valued arg keys (including present null) are dropped on both sides.
- Scripted seams: plain value reused, bare array per-call queue (no repeat),
  {"__script__":true,"ret"/"raise"}, or a bare {"__raise__","message"} dict.
  Missing/exhausted => fail-closed RuntimeError; no io => channel error.
- FULL builtin exception hierarchy (py3.14) decides `except Exception` in
  _quiet (used by _discover_payload) and in _hydrate_hit's view guard; the
  BaseException line passes through.
- json.dumps uses Python separators and ensure_ascii=False.  Fixture corpus is
  ASCII; integral floats normalize to int; fts_snippet/anchor are restricted
  to str/null/object (documented in the C port header).
"""
import ast
import builtins as _bi
import json
import os
import re
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

SRC_NAME = "session_search_flow3_src.py"
PORTED = ["content_text", "redact_derived_value", "redact_tool_calls",
          "discovery_entry", "derived_snippet", "bookend",
          "discover_payload", "hydrate_hit"]
LIVE = {
    "content_text": "_content_text",
    "redact_derived_value": "_redact_derived_value",
    "redact_tool_calls": "_redact_tool_calls",
    "discovery_entry": "_discovery_entry",
    "derived_snippet": "_derived_snippet",
    "bookend": "_bookend",
    "discover_payload": "_discover_payload",
    "hydrate_hit": "_hydrate_hit",
}

# Verbatim from tools/session_search_tool.py (current quarry revision).
# Only the docstrings are shortened; bodies, constants and control flow are
# byte-faithful.  Collaborators are seeded as recording stubs in extract_ns().
SS3_SRC = r'''
import json
import re
from typing import Any, Optional

_COMPACTION_PREFIXES = ("[CONTEXT COMPACTION", "[CONTEXT SUMMARY]:")

_DATA_URL_PLACEHOLDER = "[inline data omitted]"
_DATA_URL_RE = re.compile(r"data:[^\s,]*;base64,")

_SNIPPET_MARK_RE = re.compile(r">>>(.*?)<<<", re.DOTALL)
_SNIPPET_BEFORE, _SNIPPET_AFTER = 100, 200


def _quiet(fn, default, msg, *log_args, with_exc: bool = False):
    try:
        return fn()
    except Exception as e:
        logging.debug(msg, *(log_args + (e,) if with_exc else log_args), exc_info=True)
        return default


def _redact_tool_calls(tool_calls: Any) -> Any:
    if not isinstance(tool_calls, (list, tuple)):
        return _redact_derived_value(tool_calls)
    out = []
    for call in tool_calls:
        fn = call.get("function") if isinstance(call, dict) else None
        if isinstance(fn, dict) and fn.get("arguments") is not None:
            call = {**call, "function": {**fn, "arguments": _redact_derived_value(fn["arguments"])}}
        out.append(call)
    return out


def _redact_derived_value(value: Any) -> Any:
    if isinstance(value, str):
        return _DATA_URL_PLACEHOLDER if _DATA_URL_RE.match(value) else redact_derived_text(value)
    if isinstance(value, (list, tuple)):
        return [_redact_derived_value(v) for v in value]
    if isinstance(value, dict):
        return {k: _redact_derived_value(v) for k, v in value.items()}
    return value


def _content_text(content: Any) -> str:
    if isinstance(content, list):
        return " ".join(p["text"] for p in content if isinstance(p, dict) and isinstance(p.get("text"), str))
    return "" if content is None else str(content)


def _discovery_entry(lineage_root: Optional[str], **fields) -> dict[str, Any]:
    entry = {k: fields[k] for k in (
        "session_id", "when", "source", "model", "title", "matched_role", "match_message_id", "snippet",
        "bookend_start", "messages", "bookend_end", "messages_before", "messages_after", "detail")}
    if lineage_root and lineage_root != entry["session_id"]:
        entry["parent_session_id"] = lineage_root
    return entry


def _bookend(rows: tuple) -> list:
    return [_shape_message(m, max_content_len=1200, derived=True) for m in rows
            if not _is_compaction_summary(m.content)]


def _derived_snippet(fts_snippet: Any, anchor: Any) -> str:
    if not anchor:
        return ""
    terms = [t for t in dict.fromkeys(_SNIPPET_MARK_RE.findall(str(fts_snippet or ""))) if t.strip()]
    sources = [_content_text(_redact_derived_value(anchor.content))]
    calls = anchor.tool_calls
    if calls:
        calls = _redact_tool_calls(calls)
        sources.append(calls if isinstance(calls, str) else json.dumps(calls, ensure_ascii=False))
    probes = terms or [str(fts_snippet or "").strip(". ")[:20]]
    text, pos = sources[0], -1
    for candidate in sources:
        lowered = candidate.lower()
        pos = min((i for i in (lowered.find(t.lower()) for t in probes if t) if i >= 0), default=-1)
        if pos >= 0:
            text = candidate
            break
    start = max(0, pos - _SNIPPET_BEFORE) if pos >= 0 else 0
    end = min(len(text), max(pos, 0) + _SNIPPET_AFTER)
    window = text[start:end]
    if terms:
        window = re.sub("|".join(re.escape(t) for t in sorted(terms, key=len, reverse=True)),
                        lambda m: f">>>{m.group(0)}<<<", window, flags=re.IGNORECASE)
    return ("..." if start else "") + window + ("..." if end < len(text) else "")


def _discover_payload(persistence, query: str, detail: str, results: list, **extra) -> str:
    status = _quiet(lambda: persistence.search.search_index_rebuild_status(), None, "fts_rebuild_status failed")
    rebuild = {} if status is None else {"index_rebuild": {"percent": status.percent, "note": (
        f"The search index is rebuilding in the background ({status.percent}% done, "
        f"{status.indexed:,} of {status.total:,} messages). Results from older messages "
        f"may be incomplete until it finishes.")}}
    return json.dumps({"success": True, "mode": "discover", "query": query, "detail": detail,
                       "results": results, "count": len(results), **extra, **rebuild}, ensure_ascii=False)


def _hydrate_hit(persistence, lineage_root: str, hit, result_detail: str) -> Optional[dict]:
    hit_sid, msg_id = hit.session_id or lineage_root, hit.message_id
    try:
        view = persistence.messages.read_anchored_view(hit_sid, msg_id, window=5, bookend=3)
    except Exception as e:
        logging.warning("get_anchored_view failed for %s/%s: %s", hit_sid, msg_id, e, exc_info=True)
        return None
    session_meta, full = (_get_session_meta(persistence, hit_sid) or _get_session_meta(persistence, lineage_root),
                          result_detail == "full")
    anchor = next((m for m in view.window if m.id == msg_id), None)
    return _discovery_entry(
        lineage_root, session_id=hit_sid,
        when=_format_timestamp((session_meta and session_meta.started_at) or hit.session_started),
        source=(session_meta and session_meta.source) or hit.source,
        model=(session_meta and session_meta.model) or hit.model or "unknown",
        title=redact_derived_text(session_meta.title if session_meta else None) or None,
        matched_role=hit.role, match_message_id=msg_id,
        snippet=_derived_snippet(hit.snippet, anchor),
        bookend_start=_bookend(view.bookend_start) if full else [],
        messages=[_shape_message(m, anchor_id=msg_id, max_content_len=4000, derived=True)
                  for m in view.window if full or m.id == msg_id],
        bookend_end=_bookend(view.bookend_end) if full else [],
        messages_before=view.before, messages_after=view.after,
        detail=result_detail)
'''

# ── seam recorders ──
class LoggingDouble:
    def _rec(self, level, msg, args, kw):
        return CUR["ctx"].call("logger." + level,
                               {"message": msg, "args": list(args),
                                "exc_info": bool(kw.get("exc_info", False))})

    def debug(self, msg, *args, **kw): return self._rec("debug", msg, args, kw)

    def warning(self, msg, *args, **kw): return self._rec("warning", msg, args, kw)


def _seam(name):
    def rec(*a, **kw):
        args = {}
        if a: args["args"] = list(a)
        args.update(kw)
        return CUR["ctx"].call(name, args)
    return rec


def _redact_seam(value):
    """Live redact_derived_text(value) is projected to seam args {value}."""
    return CUR["ctx"].call("redact_derived_text", {"value": value})


def _ts_seam(ts):
    return CUR["ctx"].call("format_timestamp", {"ts": ts})


def _meta_seam(persistence, sid):
    return _mat_meta(CUR["ctx"].call("get_session_meta", {"session_id": sid}))


def _shape_seam(m, **kw):
    args = {"message": m}
    args.update(kw)
    return CUR["ctx"].call("shape_message", args)


def _ics_seam(content):
    return CUR["ctx"].call("is_compaction_summary", {"content": content})


def extract_ns():
    mod = types.ModuleType("ss3_oracle_ns")
    ns = mod.__dict__
    ns["__name__"] = "ss3_oracle_ns"
    ns["Any"] = object
    ns["Optional"] = object
    ns["redact_derived_text"] = _redact_seam
    ns["_format_timestamp"] = _ts_seam
    ns["_get_session_meta"] = _meta_seam
    ns["_shape_message"] = _shape_seam
    ns["_is_compaction_summary"] = _ics_seam
    ns["logging"] = LoggingDouble()
    tree = ast.parse(SS3_SRC)
    keep = [n for n in tree.body
            if isinstance(n, (ast.Import, ast.ImportFrom, ast.FunctionDef, ast.Assign, ast.AnnAssign))]
    m = ast.Module(body=keep, type_ignores=[])
    ast.fix_missing_locations(m)
    import __future__
    exec(compile(m, SRC_NAME, "exec", flags=__future__.annotations.compiler_flag), ns)
    want = ["_content_text", "_redact_derived_value", "_redact_tool_calls",
            "_discovery_entry", "_derived_snippet", "_bookend", "_discover_payload",
            "_hydrate_hit", "_quiet", "_SNIPPET_MARK_RE", "_DATA_URL_RE"]
    missing = [k for k in want if k not in ns]
    if missing:
        print(f"FATAL oracle_session_search_flow3: missing {missing}")
        sys.exit(2)
    return ns


NS = extract_ns()
CUR = {"ctx": None}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── FULL builtin exception hierarchy (py3.14) ──
_BUILTIN_EXC = {}
for _n in dir(_bi):
    _o = getattr(_bi, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        _BUILTIN_EXC[_n] = _o
_BUILTIN_EXC["CancelledError"] = __import__("asyncio").CancelledError
_BYPASS_INIT = {"UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError",
                "ExceptionGroup", "BaseExceptionGroup"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    real = _BUILTIN_EXC.get(kind)
    if real is None:
        return type(str(kind), (Exception,), {})(msg)
    if kind in _BYPASS_INIT:
        cls = type(str(kind), (real,), {"__str__": lambda self: Exception.__str__(self)})
        if kind in ("ExceptionGroup", "BaseExceptionGroup"):
            e = real.__new__(cls, msg, [ValueError(msg)])
        else:
            e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return real(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in args.items() if v is not None}
        if self.io is None:
            self._record_raise(name, args, RuntimeError(f"no seam channel installed for {name}"))
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
        if isinstance(ret, dict) and "__raise__" in ret:
            raise make_exc(ret)
        return ret


# ── live-object doubles ──
def _mat_message(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def _mat_hit(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def _mat_meta(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def _mat_status(d):
    return types.SimpleNamespace(**d) if isinstance(d, dict) else d


def _mat_view(d):
    if not isinstance(d, dict): return d
    v = types.SimpleNamespace()
    v.window = [_mat_message(m) for m in (d.get("window") or [])]
    v.bookend_start = [_mat_message(m) for m in (d.get("bookend_start") or [])]
    v.bookend_end = [_mat_message(m) for m in (d.get("bookend_end") or [])]
    v.before = d.get("before", 0)
    v.after = d.get("after", 0)
    return v


class _Svc:
    def read_anchored_view(self, sid, mid, window=5, bookend=3):
        r = CUR["ctx"].call("read_anchored_view",
                            {"session_id": sid, "message_id": mid, "window": window, "bookend": bookend})
        return _mat_view(r)

    def search_index_rebuild_status(self):
        return _mat_status(CUR["ctx"].call("search_index_rebuild_status", {}))


class _Persist:
    def __init__(self):
        self.messages = _Svc()
        self.search = _Svc()


def drive(func, inp):
    args = dict(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    CUR["ctx"] = ctx
    f = NS[LIVE[func]]
    try:
        if func == "content_text":
            ret = f(args.get("content"))
        elif func == "redact_derived_value":
            ret = f(args.get("value"))
        elif func == "redact_tool_calls":
            ret = f(args.get("tool_calls"))
        elif func == "discovery_entry":
            fields = {k: v for k, v in args.items() if k != "lineage_root"}
            ret = f(args.get("lineage_root"), **fields)
        elif func == "derived_snippet":
            ret = f(args.get("fts_snippet"), _mat_message(args.get("anchor")))
        elif func == "bookend":
            ret = f(tuple(_mat_message(m) for m in (args.get("rows") or [])))
        elif func == "discover_payload":
            ret = f(_Persist(), args.get("query"), args.get("detail"),
                    args.get("results") or [], **(args.get("extra") or {}))
        else:  # hydrate_hit
            ret = f(_Persist(), args.get("lineage_root"),
                    _mat_hit(args.get("hit")), args.get("result_detail"))
    except BaseException as exc:
        ret = proj_exc(exc)
    finally:
        CUR["ctx"] = None
    return {"ret": canon(ret), "trace": canon(ctx.trace),
            "state": canon(inp.get("state") or {})}


# ── three-way seam-name cross-check + fixture coverage ──
SEAM_NAMES = [
    "redact_derived_text", "format_timestamp", "get_session_meta", "shape_message",
    "is_compaction_summary", "read_anchored_view", "logger.warning", "logger.debug",
    "search_index_rebuild_status",
]


def cross_check():
    csrc = (_REPO / "src" / "agent" / "session_search_flow3.c").read_text(encoding="utf-8")
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_search_flow3: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_seam\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'"logger\." \+ level', osrc))  # handled below
    used.discard('"logger." + level')
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_session_search_flow3: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_search_flow3.jsonl"
    if not fix.exists():
        print("FATAL oracle_session_search_flow3: fixture file missing")
        sys.exit(2)
    counts = {}
    for line in fix.read_text(encoding="utf-8").splitlines():
        if not line.strip(): continue
        case = json.loads(line)
        counts[case.get("func")] = counts.get(case.get("func"), 0) + 1
        bad_keys = set((case.get("in") or {}).get("io") or {}) - set(SEAM_NAMES)
        if bad_keys:
            print(f"FATAL oracle_session_search_flow3: fixture io keys without seam: {sorted(bad_keys)}")
            sys.exit(2)
    badf = set(counts) - set(PORTED)
    if badf:
        print(f"FATAL oracle_session_search_flow3: fixture funcs not ported: {sorted(badf)}")
        sys.exit(2)
    low = [f for f in PORTED if counts.get(f, 0) < 5]
    if low:
        print(f"FATAL oracle_session_search_flow3: <5 cases/func for {low}: {counts}")
        sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g: return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12: return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:600]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:600]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:600]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:600]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1; print(f"MISMATCH <unparsable harness line>: {exc}"); continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1; print(f"MISMATCH {func}: harness emitted no result: {got}"); continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1; print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}"); continue
        g = canon(got)
        if g != want:
            m += 1; report_mismatch(func, want, g, shown); shown += 1
    print(f"ORACLE session_search_flow3: {n} cases, {m} mismatches")
    if n == 0: sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
