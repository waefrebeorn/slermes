#!/usr/bin/env python3
"""Differential oracle for
persistence/sqlite/conversation/_store/worker_lifecycle.py (w20_workerlife_dacc9a).

The live helper runs with a fake connection that answers the two tombstone
SELECTs from the fixture and records the INSERT, plus stub store helpers that
record which post-insert steps ran. Compared: the 19-value INSERT parameter
tuple (json.dumps included), the final profile name, the final model_config and
the step order -- or the AuthorityRefusal the guard raises.
"""
import importlib.util
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AuthorityRefusal(Exception):
    def __init__(self, reason, **kw):
        super().__init__(reason)
        self.reason = reason
        self.kw = kw


STEPS = []
RETIRED_PREFIX = "retired:"


for pkg in ("persistence", "persistence.core", "persistence.sqlite",
            "persistence.sqlite.conversation", "persistence.sqlite.conversation._store"):
    _mk(pkg)

_mk("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
_mk("persistence.sqlite.conversation._store.core",
    _delete_unreferenced_system_prompts=lambda conn: STEPS.append("delete_system_prompts"))
_mk("persistence.sqlite.conversation._store.sessions",
    _inherit_parent_session_metadata=lambda conn, sid: STEPS.append("inherit_parent"),
    _own_profile_name=lambda holder: holder.profile_name,
    _UPSERT_KEEP_EXISTING_SQL=" /* keep-existing */")
_mk("persistence.sqlite.conversation._store.retirement", RETIRED_PREFIX=RETIRED_PREFIX)
_mk("persistence.sqlite.conversation._store.compression",
    stamp_compression_ancestors=lambda conn, sid: STEPS.append("stamp"))

NAME = "persistence.sqlite.conversation._store.worker_lifecycle"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "conversation", "_store",
                       "worker_lifecycle.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class _Cursor:
    def __init__(self, value):
        self._value = value

    def fetchone(self):
        return self._value


class _Conn:
    def __init__(self, spec, log):
        self._row = (1,) if spec.get("row_exists") else None
        self._tomb = (1,) if spec.get("tombstone_exists") else None
        self._first = True
        self.log = log

    def execute(self, sql, params=()):
        self.log.append((sql, params))
        if sql.startswith("SELECT 1 FROM sessions"):
            return _Cursor(self._row)
        if sql.startswith("SELECT 1 FROM state_meta"):
            return _Cursor(self._tomb)
        return _Cursor(None)


def _expected(spec):
    spec = spec if isinstance(spec, dict) else {}
    STEPS.clear()
    log = []
    conn = _Conn(spec, log)
    holder = SimpleNamespace(profile_name=spec.get("own_profile_name"),
                             _store_system_prompt=lambda c, sp_: spec.get("prompt_hash"))
    live.time = SimpleNamespace(time=lambda: spec.get("now", 0.0))
    try:
        live.insert_session_row_in_transaction(
            holder, conn, spec.get("session_id"), spec.get("source"),
            model=spec.get("model"), model_config=spec.get("model_config"),
            system_prompt=spec.get("system_prompt"), user_id=spec.get("user_id"),
            session_key=spec.get("session_key"), chat_id=spec.get("chat_id"),
            chat_type=spec.get("chat_type"), thread_id=spec.get("thread_id"),
            parent_session_id=spec.get("parent_session_id"), cwd=spec.get("cwd"),
            profile_name=spec.get("profile_name"),
            git_repo_root=spec.get("git_repo_root"), origin_json=spec.get("origin_json"),
            display_name=spec.get("display_name"),
            transport_profile=spec.get("transport_profile"))
    except AuthorityRefusal as exc:
        return {"status": "raised", "type": "AuthorityRefusal", "message": str(exc)}
    inserts = [entry for entry in log if "INSERT INTO sessions" in entry[0]]
    tomb_queries = len(log) - len(inserts)
    params = list(inserts[0][1])
    return {
        "status": "ok",
        "value": {
            "params": params,
            "steps": list(STEPS),
            "profile_name": params[13],
            "model_config": json.loads(params[9]) if params[9] is not None else None,
            "tombstone_queries": tomb_queries,
        },
    }


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
    try:
        exp = _expected(r.get("spec"))
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_workerlife_dacc9a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
