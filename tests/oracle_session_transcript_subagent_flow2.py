#!/usr/bin/env python3
"""Oracle for the gateway/session_transcript_subagent.py FLOW port
(src/gateway/session_transcript_subagent_flow2.c, sts2_*).

Drives the LIVE module's 5 defs (unavailable, subagent_transcript, _child, the
nested roster, _read) AST-extracted from the quarry, with SCRIPTED seams fed the
same in.io scripts the C harness feeds its universal seam. Compares the ordered
seam-event trace + the final return.

The heavy quarry import chain is never imported: module-level globals
(authorize, authority_persistence, AuthorityRefusal, asyncio) are seeded into
the extraction namespace; in-function imports resolve against stub sys.modules.
Cross-function calls are seamed (subagent_transcript -> _read via to_thread;
_read -> _child; _child -> locked_projection), so each func's trace holds only
its immediate collaborators and the nested roster is driven on its own.

Fail-closed: missing defs / seam-name drift / 0 cases => exit 2. Summary:
'ORACLE session_transcript_subagent_flow2: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- None-valued arg keys are dropped on both sides; doubles travel as plain dicts
  in fixtures and are materialized to attribute holders (dict where .get() is
  used) so live getattr/projection reads agree.
- Scripted seams: plain value reused, bare array per-call queue (no repeat),
  {"__script__": true, "ret"/"raise"}; a bare {"__raise__","message"} dict also
  raises. Missing/exhausted script => fail-closed RuntimeError; absent io =>
  "no seam channel installed for <name>". Full builtin exception hierarchy is
  materialized for scripted kinds (KeyError excluded: str(KeyError(k)) == repr).
"""
import ast
import asyncio
import builtins as _bi
import json
import os
import re
import sys
import textwrap
import types
from pathlib import Path
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "gateway", "session_transcript_subagent.py")
_FLAGS = _fut.annotations.compiler_flag

PORTED = ["unavailable", "subagent_transcript", "_child", "_roster", "_read"]
FUNC_ALIAS = {"_roster": "roster"}  # fixture func name -> NS key

SEAM_NAMES = [
    "authorize", "to_thread",
    "owner_scope", "owner_scope.__enter__", "owner_scope.__exit__",
    "authority_persistence", "child", "delegated_child_lineage_ids",
    "read_transcript", "locked_projection", "require_current_route",
    "owner_objects", "physical_target", "compression_lineage",
    "stored_child_session",
]

# current per-case seam channel (set in drive; stubs read it at call time)
_CTX = None


# ── AST extraction helpers ──────────────────────────────────────────────────
def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(textwrap.dedent(seg(src, node)), path, "exec",
                 flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


# ── projection / materialization ────────────────────────────────────────────
class LineageDouble:
    """store.lineage: attribute that exposes delegated_child_lineage_ids()."""

    def __init__(self, raw, ctx):
        self.raw = raw
        self.ctx = ctx

    def delegated_child_lineage_ids(self, child):
        return self.ctx.call("delegated_child_lineage_ids",
                             {"lineage": denorm(self.raw), "child": denorm(child)})


def denorm(x):
    if isinstance(x, LineageDouble):
        return denorm(x.raw)
    if isinstance(x, types.SimpleNamespace):
        return {k: denorm(v) for k, v in vars(x).items()}
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(i) for i in x]
    return x


def mat(x):
    if isinstance(x, dict):
        return types.SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(i) for i in x]
    return x


def canon(v):
    def conv(x):
        if isinstance(x, LineageDouble):
            return conv(x.raw)
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
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization (FULL builtin hierarchy, py3.14) ─────────────
class AuthorityRefusal(Exception):
    pass


_BUILTIN_EXC = {}
for _n in dir(_bi):
    _o = getattr(_bi, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        _BUILTIN_EXC[_n] = _o
_BUILTIN_EXC["CancelledError"] = asyncio.CancelledError
_BUILTIN_EXC["AuthorityRefusal"] = AuthorityRefusal
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


# ── per-case script backend (mirrors C sts2_call exactly) ──────────────────
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io            # None == no seam channel installed
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in args.items() if v is not None}
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


# ── context-manager / store doubles ─────────────────────────────────────────
class _OwnerCM:
    def __init__(self, raw, ctx):
        self.raw = raw
        self.ctx = ctx

    def __enter__(self):
        return self.ctx.call("owner_scope.__enter__", {"cm": denorm(self.raw)})

    def __exit__(self, et, ev, tb):
        exc = proj_exc(ev) if ev is not None else None
        r = self.ctx.call("owner_scope.__exit__", {"cm": denorm(self.raw), "exc": exc})
        return bool(r)


def make_store(raw, ctx):
    if isinstance(raw, dict):
        d = {k: mat(v) for k, v in raw.items()}
        if isinstance(raw.get("lineage"), dict):
            d["lineage"] = LineageDouble(raw["lineage"], ctx)
        return types.SimpleNamespace(**d)
    return raw


def mat_records(records):
    """locked_projection rows stay dicts (.get()); agent becomes attribute holder."""
    if isinstance(records, list):
        out = []
        for r in records:
            if isinstance(r, dict):
                d = dict(r)
                if isinstance(d.get("agent"), dict):
                    d["agent"] = types.SimpleNamespace(**d["agent"])
                out.append(d)
            else:
                out.append(r)
        return out
    return records


# ── stubs for module globals and in-function imports ───────────────────────
def authorize(connection, ref, params, permission):
    return _CTX.call("authorize", {"connection": denorm(connection), "ref": denorm(ref),
                                    "params": denorm(params), "permission": permission})


def authority_persistence(authority):
    return make_store(_CTX.call("authority_persistence",
                                {"authority": denorm(authority)}), _CTX)


def child_stub(authority, live, ref, subagent_id):
    return _CTX.call("child", {"authority": denorm(authority), "live": denorm(live),
                               "ref": denorm(ref), "subagent_id": denorm(subagent_id)})


def _require_current_route(authority, ref):
    return _CTX.call("require_current_route", {"authority": denorm(authority),
                                                "ref": denorm(ref)})


def _owner_objects(authority, ref, include_fallback_evicted=False):
    return _CTX.call("owner_objects", {"authority": denorm(authority), "ref": denorm(ref),
                                        "include_fallback_evicted": include_fallback_evicted})


def physical_target(authority, ref):
    return _CTX.call("physical_target", {"authority": denorm(authority), "ref": denorm(ref)})


def locked_projection(authority, live, ref, roster):
    rows = _CTX.call("locked_projection", {"authority": denorm(authority),
                                           "live": denorm(live), "ref": denorm(ref)})
    if isinstance(rows, list) and rows and isinstance(rows[0], (dict, list)):
        rows = [mat_records(rows[0])] + rows[1:]
    return rows


def _compression_lineage(store, target):
    return _CTX.call("compression_lineage", {"store": denorm(store),
                                              "target": denorm(target)})


def stored_child_session(store, subagent_id, lineage):
    return _CTX.call("stored_child_session", {"store": denorm(store),
                                               "subagent_id": denorm(subagent_id),
                                               "lineage": denorm(lineage)})


def owner_scope(authority):
    return _OwnerCM(_CTX.call("owner_scope", {"authority": denorm(authority)}), _CTX)


def read_transcript(store, child, lineage_ids, binding, held, limit, *, profile_home,
                    inline_images):
    return _CTX.call("read_transcript", {
        "store": denorm(store), "child": denorm(child), "lineage_ids": denorm(lineage_ids),
        "binding": denorm(binding), "held": denorm(held), "limit": denorm(limit),
        "profile_home": denorm(profile_home), "inline_images": denorm(inline_images)})


class _ASyncio:
    @staticmethod
    async def to_thread(fn, *a, **kw):
        # _read is seamed here: record the dispatch with its bound args and
        # answer from the script (a raise or the final transcript dict).
        keys = ("authority", "live", "ref", "binding", "held", "limit", "inline_images")
        call_args = {k: denorm(v) for k, v in zip(keys, a)}
        return _CTX.call("to_thread", call_args)


def install_modules():
    def pkg(name):
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
        return m

    g = pkg("gateway")
    t = pkg("tools")
    p = pkg("persistence")
    pc = pkg("persistence.core")

    anc = types.ModuleType("gateway.session_ancillary")
    anc._compression_lineage = _compression_lineage
    anc._owner_objects = _owner_objects
    anc._require_current_route = _require_current_route
    anc.locked_projection = locked_projection
    anc.physical_target = physical_target
    sys.modules["gateway.session_ancillary"] = anc
    g.session_ancillary = anc

    dts = types.ModuleType("tools.delegate_tool_sessions")
    dts.stored_child_session = stored_child_session
    sys.modules["tools.delegate_tool_sessions"] = dts
    t.delegate_tool_sessions = dts

    authmod = types.ModuleType("gateway.session_authorities")
    authmod.owner_scope = owner_scope
    sys.modules["gateway.session_authorities"] = authmod
    g.session_authorities = authmod

    sreads = types.ModuleType("gateway.session_stored_reads")
    sreads.read_transcript = read_transcript
    sys.modules["gateway.session_stored_reads"] = sreads
    g.session_stored_reads = sreads

    errs = types.ModuleType("persistence.core.errors")
    errs.AuthorityRefusal = AuthorityRefusal
    sys.modules["persistence.core.errors"] = errs
    pc.errors = errs


install_modules()


# ── namespace extraction ────────────────────────────────────────────────────
def extract_ns():
    mod = new_module("sts2_oracle_subagent")
    ns = mod.__dict__
    ns.update({"asyncio": _ASyncio(), "AuthorityRefusal": AuthorityRefusal,
               "authorize": authorize, "authority_persistence": authority_persistence})
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    seed_constants(src, ns, SRC)

    # nested roster (inside _child): compile standalone with (authority, ref) params.
    child = next((n for n in ast.parse(src).body
                  if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == "_child"), None)
    roster = next((n for n in ast.walk(child) if isinstance(n, ast.FunctionDef) and n.name == "roster"), None) if child else None
    if roster is None:
        print("FATAL oracle_session_transcript_subagent_flow2: nested roster not found")
        sys.exit(2)
    rsrc = seg(src, roster).replace("def roster():", "def _roster(authority, ref):", 1)
    if "def _roster(authority, ref):" not in rsrc:
        print("FATAL oracle_session_transcript_subagent_flow2: roster signature drift")
        sys.exit(2)
    rns = dict(ns)
    rns.update({"_require_current_route": _require_current_route,
                "_owner_objects": _owner_objects, "physical_target": physical_target})
    exec(textwrap.dedent(rsrc), rns, rns)

    required = PORTED
    missing = [k for k in required if k not in ns and k not in rns]
    if missing:
        print(f"FATAL oracle_session_transcript_subagent_flow2: missing {missing}")
        sys.exit(2)
    ns["_roster"] = rns["_roster"]
    return ns


NS = extract_ns()
REAL_CHILD = NS["_child"]
NS["_child"] = child_stub  # _read composes through the seam, not the live body


# ── drive one case ──────────────────────────────────────────────────────────
def drive(func, inp):
    global _CTX
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    _CTX = ctx

    raw_conn = args.get("connection") or {}
    raw_auth = args.get("authority") or raw_conn.get("authority") or {}
    authority = types.SimpleNamespace(
        sessions=dict(raw_auth.get("sessions") or {}),
        profile_id=raw_auth.get("profile_id"))
    connection = types.SimpleNamespace(authority=authority)
    ref = mat(args.get("ref"))
    params = mat(args.get("params"))
    live = mat(args.get("live"))
    binding = mat(args.get("binding"))
    held = mat(args.get("held"))
    limit = mat(args.get("limit"))
    inline_images = mat(args.get("inline_images"))
    subagent_id = mat(args.get("subagent_id"))

    try:
        if func == "unavailable":
            ret = NS["unavailable"](args.get("session_id"), args.get("subagent_id"),
                                    args.get("running"))
        elif func == "subagent_transcript":
            coro = NS["subagent_transcript"](connection, ref, params, binding, held,
                                             limit, inline_images)
            ret = asyncio.run(coro)
        elif func == "_child":
            ret = REAL_CHILD(authority, live, ref, subagent_id)
        elif func == "roster":
            ret = NS["_roster"](authority, ref)
        elif func == "_read":
            ret = NS["_read"](authority, live, ref, binding, held, limit, inline_images)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace)}


# ── seam / fixture cross-check ──────────────────────────────────────────────
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "session_transcript_subagent_flow2.c",
                encoding="utf-8").read()
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_session_transcript_subagent_flow2: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "session_transcript_subagent_flow2.jsonl"
    fixfuncs = set()
    if fix.exists():
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            io = (case.get("in") or {}).get("io") or {}
            bad = [k for k in io if k not in SEAM_NAMES]
            if bad:
                print(f"FATAL oracle_session_transcript_subagent_flow2: fixture io without seam: {sorted(set(bad))}")
                sys.exit(2)
            fixfuncs.add(case.get("func"))
    known = {FUNC_ALIAS.get(p, p) for p in PORTED}
    badf = fixfuncs - known
    if badf:
        print(f"FATAL oracle_session_transcript_subagent_flow2: fixture funcs not ported: {sorted(badf)}")
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
    print(f"ORACLE session_transcript_subagent_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()
