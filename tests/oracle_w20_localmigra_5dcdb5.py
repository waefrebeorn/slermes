#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/_local_migration.py
(gate w20_localmigra_5dcdb5).

The live module is imported for real (its imports resolve from the quarry). Its
collaborators -- the sqlite connection, validate_legacy_row, binding_names,
client_history_row, require_idle, the turn-lease key resolver, the own-profile
lookup and the wall clock -- are scripted from the case's "scene", so the guard
ladder and both statements execute in the live body and the compared value is
what that body actually wrote.
"""
import importlib.util
import json
import os
import sys
import types

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "persistence.sqlite.authority._local_migration_w20live"
import persistence.sqlite.authority  # noqa: E402,F401 - parent package for the relative imports
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "authority", "_local_migration.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

import persistence.core.canonical as canonical                        # noqa: E402
import persistence.sqlite.authority._runtime_guards as guards         # noqa: E402
import persistence.sqlite.conversation._store.compression as compression  # noqa: E402
import persistence.sqlite.conversation._store.sessions as sessions_mod    # noqa: E402
import session_rules.legacy_local as legacy_local                     # noqa: E402
from persistence.core.errors import AuthorityRefusal                   # noqa: E402


class Result:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row

    def fetchall(self):
        return [] if self._row is None else [self._row]


def _cell(value):
    """A scripted state_meta value -> the row the query would return."""
    if value is None:
        return None
    return (value,)


class Conn:
    """Scripted sqlite connection: answers by statement shape, records calls."""

    def __init__(self, scene):
        self.scene = scene
        self.calls = []
        self.binding_reads = 0

    def execute(self, sql, params=()):
        self.calls.append((sql, list(params)))
        s = " ".join(sql.split())
        sc = self.scene
        if s.startswith("SELECT * FROM sessions"):
            return Result(sc.get("row"))
        if s.startswith("SELECT value FROM state_meta"):
            self.binding_reads += 1
            if self.binding_reads == 1:
                return Result(_cell(sc.get("imported")))
            return Result(_cell("reimported"))
        if s.startswith("SELECT 1 FROM session_turn_leases"):
            return Result(("lease",) if sc.get("lease") else None)
        if s.startswith("SELECT 1 FROM state_meta"):
            return Result(("present",) if sc.get("legacy_key") else None)
        if s.startswith("SELECT child.id"):
            return Result(("child-1",) if sc.get("chain_step") else None)
        return Result(None)


def install(scene):
    def validate(own_profile, row, *, subject=None, rule=None):
        verdict = scene.get("validate", "ok")
        if verdict != "ok":
            raise AuthorityRefusal(verdict)

    def require_idle(holder, conn, session_ids):
        verdict = scene.get("idle", "ok")
        if verdict != "ok":
            raise AuthorityRefusal(verdict)

    # These three are bound at import time by the live module itself.
    live.validate_legacy_row = validate
    live.binding_names = lambda saved, subject, rule: bool(scene.get("binding_ok"))
    live.client_history_row = lambda row, rule: bool(scene.get("client_row"))
    guards.require_idle = require_idle
    compression._session_turn_lease_key_on_conn = (
        lambda holder, conn, target: scene.get("lease_root", "root-1"))
    sessions_mod._own_profile_name = lambda holder: scene.get("own_profile", "default")
    live.time = types.SimpleNamespace(time=lambda: float(scene.get("now", 1731000000.0)))


def expected(r):
    receipt = r.get("receipt")
    scene = r.get("scene") or {}
    if not isinstance(scene, dict):
        scene = {}
    install(scene)
    conn = Conn(scene)
    try:
        live.bind_legacy_target(object(), conn, receipt, None)
    except AuthorityRefusal as exc:
        return {"refusal": exc.reason}
    except Exception as exc:  # noqa: BLE001 - the harness reports raises verbatim
        return {"raised": type(exc).__name__, "message": str(exc)}
    writes = [{"sql": sql, "params": params} for sql, params in conn.calls
              if sql.lstrip().startswith(("UPDATE", "INSERT"))]
    return {"statements": writes}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        f = r.get("func")
        try:
            if f == "bind_legacy_target":
                exp = expected(r)
            elif f == "canonical_dumps":
                exp = canonical.canonical_dumps(r.get("value"))
            elif f == "canonical_json":
                try:
                    exp = {"value": canonical.canonical_json(r.get("value"))}
                except AuthorityRefusal as exc:
                    exp = {"refusal": exc.reason}
            elif f == "binding_prefix":
                import persistence.sqlite.authority._binding_history as bh
                exp = bh.BINDING_PREFIX
            elif f == "legacy_prefix":
                import persistence.sqlite.conversation._store.local_migration as lm
                exp = lm.LEGACY_PREFIX
            else:
                raise AssertionError("unknown func %r" % (f,))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (f, r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (f, r.get("case"), exp, got),
                  file=sys.stderr)
    print("ORACLE w20_localmigra_5dcdb5: %d cases, %d mismatches" % (cases, mism))
    return 1 if mism else 0


sys.exit(main())
