#!/usr/bin/env python3
"""Oracle for the four session_search_tool.py "gap" leaves ported as a FLOW
(src/tools/session_search_gaps_flow2.c).

The live defs are AST/exec-extracted from a verbatim source slice of
tools/session_search_tool.py at quarry commit 39e480c051 — the revision where
_session_end_reason / _session_left_live_context / _is_fresh_reset_session /
_session_search_impl still lived in that module.  The current quarry split
them into session_search_tool_common.py / _discover.py, and the stub-era PoP
claims in src/tools/session_search_tool_gaps.c are constant-return shims, not
ports.  The source slice is embedded below so the oracle is self-contained.

Live collaborators are SCRIPTED recorders fed the same in.io the C universal
seam uses; both sides emit ordered {"name","args","ret"} trace events, a ret,
and a state echo.  Compares with an ordered recursive canon (key order counts).

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE session_search_gaps_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- None-valued arg keys (including present null) are dropped on both sides.
- db/profile_db/located travel as STRING markers; the oracle wraps a marker
  in DBDouble (get_session/close) for method calls and projects it back for
  seam args.  state.owned_dbs (when present) models the non-None _owned_dbs.
- Scripted seams: plain value reused, bare array per-call queue (no repeat),
  {"__script__":true,"ret"/"raise"}, or a bare {"__raise__","message"} dict.
  Missing/exhausted => fail-closed RuntimeError; no io => channel error.
- FULL builtin exception hierarchy (py3.14) decides `except Exception` in
  _session_end_reason and the profile-resolution guard in _session_search_impl;
  the BaseException line (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError/BaseExceptionGroup) passes through.
- json.loads accepts only str (exact CPython TypeError otherwise); seams always
  return well-formed JSON text.  json.dumps(found, ensure_ascii=False) uses
  Python separators.  Fixture corpus is ASCII; KeyError (repr-quoted str) and
  non-printable invalid int tokens are out of domain.
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

SRC_NAME = "session_search_gaps_flow2_src.py"
PORTED = ["session_end_reason", "session_left_live_context",
          "is_fresh_reset_session", "session_search_impl"]
# (harness func -> live def name)
LIVE = {
    "session_end_reason": "_session_end_reason",
    "session_left_live_context": "_session_left_live_context",
    "is_fresh_reset_session": "_is_fresh_reset_session",
    "session_search_impl": "_session_search_impl",
}

# Verbatim from tools/session_search_tool.py @ 39e480c051 (only docstrings
# shortened; bodies, constants and control flow identical).
GAPS_SRC = r'''
from __future__ import annotations
import json
from typing import Any, List, Optional

_FRESH_RESET_END_REASONS = frozenset({
    "session_reset", "session_switch", "idle", "daily", "suspended",
    "resume_pending_expired", "new_session",
})


def _session_end_reason(db, session_id: str) -> Optional[str]:
    """Return the session's end_reason, or None if missing/unended/error."""
    if not session_id:
        return None
    try:
        s = db.get_session(session_id)
        if not s:
            return None
        return s.get("end_reason") or None
    except Exception:
        return None


def _session_left_live_context(db, session_id: str) -> bool:
    """True when the session was ended (transcript not in live context)."""
    return _session_end_reason(db, session_id) is not None


def _is_fresh_reset_session(end_reason: Optional[str]) -> bool:
    """True when end_reason is a /new-style reset."""
    return end_reason in _FRESH_RESET_END_REASONS


def _session_search_impl(
    query: str = "",
    role_filter: str = None,
    limit: int = 3,
    db=None,
    current_session_id: str = None,
    session_id: str = None,
    around_message_id: int = None,
    window: int = 5,
    sort: str = None,
    profile: str = None,
    *,
    _owned_dbs: Optional[List[Any]] = None,
) -> str:
    """Single-shape tool. Mode inferred from which args are set."""
    if isinstance(session_id, str) and "/" in session_id:
        emb_profile, _, emb_id = session_id.partition("/")
        if emb_id:
            session_id = emb_id
            if emb_profile and (profile is None or not str(profile).strip()):
                profile = emb_profile

    if profile is not None and str(profile).strip():
        try:
            profile_db = _resolve_profile_db(profile)
        except Exception as e:
            return tool_error(f"profile '{profile}': {e}", success=False)
        if profile_db is not None:
            db = profile_db
            if _owned_dbs is not None:
                _owned_dbs.append(profile_db)
            current_session_id = None

    if (isinstance(session_id, str) and session_id.strip()) and around_message_id is not None:
        return _scroll(
            db=db, session_id=session_id, around_message_id=around_message_id,
            window=window, current_session_id=current_session_id,
        )

    if isinstance(session_id, str) and session_id.strip():
        sid = session_id.strip()
        result = _read_session(db, sid, link_profile=profile)
        if json.loads(result).get("success"):
            return result
        located, owner = _locate_session_db(sid)
        if located is not None:
            try:
                found = json.loads(_read_session(located, sid, link_profile=owner))
            finally:
                located.close()
            if found.get("success"):
                found["profile"] = owner
                return json.dumps(found, ensure_ascii=False)
        return result

    if not isinstance(limit, int):
        try:
            limit = int(limit)
        except (TypeError, ValueError):
            limit = 3
    limit = max(1, min(limit, 10))

    if not query or not isinstance(query, str) or not query.strip():
        return _list_recent_sessions(db, limit, current_session_id, link_profile=profile)

    role_list: Optional[List[str]] = None
    if isinstance(role_filter, str) and role_filter.strip():
        role_list = [r.strip() for r in role_filter.split(",") if r.strip()]

    sort_norm: Optional[str] = None
    if isinstance(sort, str):
        candidate = sort.strip().lower()
        if candidate in ("newest", "oldest"):
            sort_norm = candidate

    return _discover(
        db=db, query=query.strip(), role_filter=role_list, limit=limit,
        sort=sort_norm, current_session_id=current_session_id, link_profile=profile,
    )
'''


def _seam(name):
    def rec(*a, **kw):
        ctx = CUR["ctx"]
        args = {}
        if a:
            args["args"] = list(a)
        args.update(kw)
        return ctx.call(name, args)
    return rec


def _seam_kw(name, pos=(), optional=()):
    """Build a recorder mapping positional params to live keyword names."""
    def rec(*a, **kw):
        args = {}
        for k, v in zip(pos, a):
            args[k] = v
        args.update(kw)
        for k in optional:
            args.setdefault(k, None)
        return CUR["ctx"].call(name, args)
    return rec


def extract_ns():
    mod = types.ModuleType("ssg2_oracle_ns")
    ns = mod.__dict__
    ns["json"] = json
    ns["tool_error"] = _seam_kw("tool_error", ("message",), ("success",))
    ns["_resolve_profile_db"] = _seam_kw("_resolve_profile_db", ("profile",))
    ns["_scroll"] = _seam("_scroll")
    ns["_read_session"] = _seam_kw("_read_session", ("db", "session_id"), ("link_profile",))

    def _locate_stub(sid):
        # Live returns (SessionDB, owner); the first element has .close().
        ctx = CUR["ctx"]
        r = ctx.call("_locate_session_db", {"session_id": sid})
        if isinstance(r, list) and len(r) >= 2:
            return [DBDouble(r[0], ctx), r[1]]
        return r

    ns["_locate_session_db"] = _locate_stub
    ns["_list_recent_sessions"] = _seam_kw(
        "_list_recent_sessions", ("db", "limit", "current_session_id"), ("link_profile",))
    ns["_discover"] = _seam("_discover")
    tree = ast.parse(GAPS_SRC)
    keep = [n for n in tree.body
            if isinstance(n, (ast.ImportFrom, ast.FunctionDef, ast.Assign))]
    m = ast.Module(body=keep, type_ignores=[])
    ast.fix_missing_locations(m)
    exec(compile(m, SRC_NAME, "exec", flags=0, dont_inherit=False), ns)
    missing = [k for k in ["_session_end_reason", "_session_left_live_context",
                           "_is_fresh_reset_session", "_session_search_impl",
                           "_FRESH_RESET_END_REASONS"] if k not in ns]
    if missing:
        print(f"FATAL oracle_session_search_gaps_flow2: missing {missing}")
        sys.exit(2)
    return ns


NS = extract_ns()
CUR = {"ctx": None, "state": None}


# ── canonical projection ──
class DBDouble:
    """Opaque DB marker with the two methods live code touches."""
    def __init__(self, marker, ctx):
        self.marker = marker
        self.ctx = ctx

    def get_session(self, session_id):
        return self.ctx.call("db.get_session", {"session_id": session_id})

    def close(self):
        return self.ctx.call("session_db.close", {"db": self})


def _proj(v):
    if isinstance(v, DBDouble):
        return v.marker
    return v


def canon(v):
    def conv(x):
        if isinstance(x, DBDouble):
            return x.marker
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
        args = {k: _proj(v) for k, v in args.items() if v is not None}
        if self.io is None:
            self._record_raise(name, args,
                               RuntimeError(f"no seam channel installed for {name}"))
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args,
                                   RuntimeError(f"script exhausted for {name}"))
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


def _mat_db(v, ctx):
    return DBDouble(v, ctx) if isinstance(v, str) else v


def drive(func, inp):
    args = dict(inp.get("args") or {})
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    CUR["ctx"] = ctx
    CUR["state"] = state
    f = NS[LIVE[func]]
    try:
        if func == "session_end_reason" or func == "session_left_live_context":
            ret = f(_mat_db(args.get("db"), ctx), args.get("session_id"))
        elif func == "is_fresh_reset_session":
            ret = f(args.get("end_reason"))
        else:
            db = _mat_db(args.get("db"), ctx)
            owned = (state.get("owned_dbs")
                     if isinstance(state, dict) and "owned_dbs" in state else None)
            ret = f(
                query=args.get("query"),
                role_filter=args.get("role_filter"),
                limit=args.get("limit", 3),
                db=db,
                current_session_id=args.get("current_session_id"),
                session_id=args.get("session_id"),
                around_message_id=args.get("around_message_id"),
                window=args.get("window", 5),
                sort=args.get("sort"),
                profile=args.get("profile"),
                _owned_dbs=owned,
            )
    except BaseException as exc:
        ret = proj_exc(exc)
    finally:
        CUR["ctx"] = None
        CUR["state"] = None
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state)}


# ── three-way seam-name cross-check ──
SEAM_NAMES = ["db.get_session", "session_db.close", "_resolve_profile_db",
              "tool_error", "_scroll", "_read_session", "_locate_session_db",
              "_list_recent_sessions", "_discover"]


def cross_check():
    csrc = (_REPO / "src" / "tools" / "session_search_gaps_flow2.c").read_text(encoding="utf-8")
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_search_gaps_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'_seam(?:_kw)?\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\("(db\.[^"]+|session_db\.[^"]+)"', osrc))
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_session_search_gaps_flow2: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_search_gaps_flow2.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_session_search_gaps_flow2: fixture io keys without seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(PORTED)
        if badf:
            print(f"FATAL oracle_session_search_gaps_flow2: fixture funcs not ported: {sorted(badf)}")
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
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:500]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:500]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:500]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:500]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:250]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:250]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
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
    print(f"ORACLE session_search_gaps_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
