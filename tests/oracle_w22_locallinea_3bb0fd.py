#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/local_lineage.py
(gate w22_locallinea_3bb0fd).

Seams stubbed, decisions live:
  sqlite  -- FakeConn answers exactly the five SELECT shapes the source issues
             (session end_reason, child parent/model_config, _CHAIN_STEP_SQL's
             `SELECT child.id`, the routing row, `SELECT * FROM sessions`,
             state_meta value/1) from the fixture's `db` document, and records
             the UPDATEs _commit_local_target issues.
  local_session_id / legacy_lineage_root -- the two imported helpers are
             replaced by the fixture's `local_id` / `root`.

The live `validate_local_lineage`, `_routed_entry` and `advance_local_target`
run unchanged; only the rows and the two helpers are injected.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "local_lineage.py"
GATE = "w22_locallinea_3bb0fd"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("local_lineage_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["local_lineage_live"] = live
_spec.loader.exec_module(live)

from persistence.authority import bindings as _bindings            # noqa: E402
from persistence.core.errors import AuthorityRefusal                # noqa: E402
from persistence.sqlite.conversation._store import local_migration as _migration  # noqa: E402

WANTED = {"validate", "routed", "advance", "policy_prefix", "legacy_prefix"}


class Row(dict):
    def __getitem__(self, key):
        if isinstance(key, int):
            return list(dict.values(self))[key]
        return dict.__getitem__(self, key)


class Cursor:
    def __init__(self, row):
        self._row = row

    def fetchone(self):
        return self._row


class Conn:
    """The sqlite seam: fixture rows in, UPDATEs recorded out."""

    def __init__(self, db):
        db = db if isinstance(db, dict) else {}
        self.sessions = {}
        rows = db.get("sessions")
        for row in rows if isinstance(rows, list) else []:
            if isinstance(row, dict) and "id" in row:
                self.sessions[row["id"]] = dict(row)
        self.chain = db.get("chain") if isinstance(db.get("chain"), dict) else {}
        self.routing = db.get("routing") if isinstance(db.get("routing"), dict) else {}
        self.meta = db.get("state_meta") if isinstance(db.get("state_meta"), dict) else {}
        legacy = db.get("legacy_ids")
        self.legacy = {_migration.LEGACY_PREFIX + x for x in legacy if isinstance(x, str)} \
            if isinstance(legacy, list) else set()
        self.updates = []

    def execute(self, sql, params=()):
        s = " ".join(str(sql).split())
        p0 = params[0] if params else None
        if "SELECT end_reason FROM sessions" in s:
            row = self.sessions.get(p0)
            return Cursor(Row({"end_reason": row.get("end_reason")}) if row is not None else None)
        if "SELECT parent_session_id,model_config FROM sessions" in s:
            row = self.sessions.get(p0)
            if row is None:
                return Cursor(None)
            return Cursor(Row({"parent_session_id": row.get("parent_session_id"),
                               "model_config": row.get("model_config")}))
        if "SELECT child.id" in s:
            child = self.chain.get(p0)
            return Cursor(Row({"id": child}) if child else None)
        if "SELECT entry_json FROM gateway_routing" in s:
            text = self.routing.get(p0)
            return Cursor(Row({0: text}) if text is not None else None)
        if "SELECT * FROM sessions" in s:
            row = self.sessions.get(p0)
            return Cursor(Row(row) if row is not None else None)
        if "SELECT value FROM state_meta" in s:
            if p0 not in self.meta:
                return Cursor(None)
            return Cursor(Row({0: self.meta[p0]}))
        if "SELECT 1 FROM state_meta" in s:
            return Cursor(Row({0: 1})) if p0 in self.legacy else Cursor(None)
        if s.startswith("UPDATE gateway_routing"):
            self.updates.append(("gateway_routing", params))
            return Cursor(None)
        if s.startswith("UPDATE state_meta"):
            self.updates.append(("state_meta", params))
            return Cursor(None)
        if s.startswith("UPDATE sessions"):
            self.updates.append(("sessions", params))
            return Cursor(None)
        raise AssertionError("unstubbed SQL: %s" % s[:80])


def with_seams(local_id, root):
    orig_id, orig_root = _bindings.local_session_id, _migration.legacy_lineage_root
    _bindings.local_session_id = lambda *a, **k: local_id
    _migration.legacy_lineage_root = lambda *a, **k: root
    return orig_id, orig_root


def restore_seams(saved):
    _bindings.local_session_id, _migration.legacy_lineage_root = saved


def classify(exc):
    if isinstance(exc, AuthorityRefusal):
        return {"status": "refused", "reason": getattr(exc, "reason", None) or str(exc)}
    return {"status": "raised", "type": type(exc).__name__}


def validate(r):
    conn = Conn(r.get("db"))
    saved = with_seams(r.get("local_id") or "", r.get("root") or "")
    try:
        tip = live.validate_local_lineage(conn, r.get("receipt"))
        return {"status": "ok", "tip": tip}
    except Exception as exc:  # noqa: BLE001
        return classify(exc)
    finally:
        restore_seams(saved)


def routed(r):
    conn = Conn(r.get("db"))
    try:
        value = live._routed_entry(conn, r.get("route"), r.get("tip"))
        return {"status": "ok", "value": value}
    except Exception as exc:  # noqa: BLE001
        return classify(exc)


def advance(r):
    conn = Conn(r.get("db"))
    saved = with_seams(r.get("local_id") or "", r.get("root") or "")
    try:
        live.advance_local_target(conn, r.get("parent_session_id"), r.get("child_session_id"),
                                 entry=r.get("entry"))
    except Exception as exc:  # noqa: BLE001
        return classify(exc)
    finally:
        restore_seams(saved)
    if not conn.updates:
        return {"action": "noop"}
    routed_json = receipt_json = None
    for table, params in conn.updates:
        if table == "gateway_routing":
            routed_json = params[0]
        elif table == "state_meta":
            receipt_json = params[0]
    return {"action": "commit",
            "receipt": json.loads(receipt_json) if receipt_json else None,
            "routed": json.loads(routed_json) if routed_json else None}


def expected(r):
    f = r.get("func")
    if f == "validate":
        return validate(r)
    if f == "routed":
        return routed(r)
    if f == "advance":
        return advance(r)
    if f == "policy_prefix":
        return live.POLICY_PREFIX
    if f == "legacy_prefix":
        return _migration.LEGACY_PREFIX
    raise AssertionError("unknown func %r" % (f,))


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
