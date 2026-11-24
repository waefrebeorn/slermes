#!/usr/bin/env python3
"""Differential oracle for gateway/run_notifications_ownership.py (gate w19_runnotific_f48e0a)."""
import ast, asyncio, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_runnotific_f48e0a"
SRC = Q / "gateway" / "run_notifications_ownership.py"
_ANN = __future__.annotations.compiler_flag

import logging

# --- stubbed collaborator modules -------------------------------------------------
if "gateway" not in sys.modules:
    _pkg = types.ModuleType("gateway")
    _pkg.__path__ = []
    sys.modules["gateway"] = _pkg


class Platform:
    def __init__(self, v):
        self.value = v

    def __eq__(self, o):
        return isinstance(o, Platform) and o.value == self.value

    def __ne__(self, o):
        return not self.__eq__(o)

    def __hash__(self):
        return hash(self.value)


Platform.LOCAL = Platform("local")

_gc = types.ModuleType("gateway.config")
_gc.Platform = Platform
sys.modules["gateway.config"] = _gc

_gr = types.ModuleType("gateway.run")
_gr._USER_BOUNDARY_END_REASONS = ("session_reset", "user_exit", "session_switch", "new_session")
sys.modules["gateway.run"] = _gr

_gsess = types.ModuleType("gateway.session")
_gsess.SessionEntry = object
sys.modules["gateway.session"] = _gsess

import persistence as _persistence


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("ownership_live")
    _mod.__path__ = []
    sys.modules["ownership_live"] = _mod
    g = _mod.__dict__
    g.update({"__name__": "ownership_live", "asyncio": asyncio, "logging": logging,
              "Any": object, "Optional": object, "Sequence": list,
              "cast": lambda t, x: x, "SessionEntry": object,
              "STORE_FAILURES": _persistence.STORE_FAILURES,
              "logger": logging.getLogger("oracle")})
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "_local_completion_card":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name == "GatewayNotificationOwnershipMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
            except Exception:
                continue
    return g


G = extract()
MIXIN = G["GatewayNotificationOwnershipMixin"]


class Row:
    def __init__(self, d):
        self._src = d
        self.session_id = d.get("session_id")
        self.ended_at = bool(d.get("ended_at"))
        self.end_reason = d.get("end_reason")
        self.source = d.get("source")
        self.model_config = d.get("model_config", {})


def _raise_set(v):
    if v is True:
        return ("*",)
    return tuple(v or ())


class FakeSessions:
    def __init__(self, rows, raises):
        self.rows = {r.get("session_id"): Row(r) for r in rows}
        self.raises = _raise_set(raises)

    async def get(self, sid):
        if "*" in self.raises or sid in self.raises:
            raise RuntimeError("store failure")
        return self.rows.get(sid)


class FakeLineage:
    def __init__(self, tips, raises):
        self.tips = tips or {}
        self.raises = _raise_set(raises)

    async def tip(self, sid):
        if "*" in self.raises or sid in self.raises:
            # _closed_local_target only catches STORE_FAILURES, so raise one of those.
            raise _persistence.STORE_FAILURES[0]("tip failure")
        return self.tips.get(sid)


class Store:
    def __init__(self, r):
        self.aio = types.SimpleNamespace(
            sessions=FakeSessions(r.get("sessions") or [], r.get("get_raises") or []),
            lineage=FakeLineage(r.get("tips") or {}, r.get("tip_raises") or []))


class Runner:
    def __init__(self, store):
        self.persistence = store


def entry_of(r):
    e = r.get("entry")
    if e is None:
        return None
    plat = e.get("platform")
    p = Platform.LOCAL if plat == "local" else Platform(plat)
    return types.SimpleNamespace(session_id=e.get("session_id"), platform=p, session_key=e.get("session_key"))


def run_case(r):
    f = r.get("func")
    store = None if r.get("store_none") else Store(r)
    runner = Runner(store)
    if f == "lookup_owner":
        verdict, sess, row = asyncio.run(MIXIN._lookup_completion_owner(
            runner, r.get("session_id") or "", r.get("current_session_id") or ""))
        return {"verdict": verdict, "session": sess,
                "row": row._src if row is not None else None}
    if f == "compression_target":
        verdict, target = asyncio.run(MIXIN._resolve_compression_lineage_target(
            runner, runner.persistence.aio, entry_of(r), r.get("pinned_session_id") or ""))
        return {"verdict": verdict, "target": target}
    if f == "closed_local_target":
        pinned_row = r.get("pinned_row")
        row = Row(pinned_row) if pinned_row is not None else None
        return asyncio.run(MIXIN._closed_local_target(
            runner, runner.persistence.aio, entry_of(r), r.get("pinned_session_id") or "", row))
    return None


def main():
    cases = mism = 0
    known = {"lookup_owner", "compression_target", "closed_local_target"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
