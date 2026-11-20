#!/usr/bin/env python3
"""Generate tests/fixtures/session_recovery_flow2.jsonl for the
session_recovery_flow2 FLOW oracle (session_recovery.py remainder, 12 funcs).
>=5 cases per function, exercising decision branches, error paths, builtin and
sqlite3 exception folding, BaseException propagation, fail-closed missing
scripts, and post-hoc mutated carriers (result / verification).

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; an ARRAY script is a per-call queue whose elements are
the per-call RESULTS (so an array RESULT is always wrapped one level deeper:
Q(["id","value"]) scripts one call returning the column list); a single non-array
value is a constant; scripted null = None; {"__raise__": cls, "msg": ...} = raise.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_recovery_flow2.jsonl")

CASES = []


def R(cls, msg="boom", **extra):
    return {"__raise__": cls, "msg": msg, **extra}


def Q(*vals):
    """per-call queue (the harness/oracle treat a JSON array as a queue)."""
    return list(vals)


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── count_rows (fetchone row [n] -> int) ──
add("count_rows", {"table": "sessions"}, {"count_rows": Q([5])})
add("count_rows", {"table": "messages"}, {"count_rows": Q([0])})
add("count_rows", {"table": "state_meta"}, {"count_rows": Q([12.9])})       # int() truncates
add("count_rows", {"table": "sessions"}, {"count_rows": Q(["7"])})          # int(str)
add("count_rows", {"table": "sessions"}, {"count_rows": Q([None])})         # TypeError
add("count_rows", {"table": "sessions"}, {"count_rows": Q([[]])})           # int([]) TypeError
add("count_rows", {"table": "sessions"}, {"count_rows": R("DatabaseError", "db down")})
add("count_rows", {"table": "sessions"}, {"count_rows": R("SystemExit", "bye")})
add("count_rows", {"table": "sessions"}, {})                                # fail-closed

# ── quoted_columns (pure tuple projection) ──
add("quoted_columns", {"columns": ["id", "value"]})
add("quoted_columns", {"columns": []})
add("quoted_columns", {"columns": ["key"]})
add("quoted_columns", {"columns": ["a b", "c\"d"]})
add("quoted_columns", {"columns": [1, True, None]})
add("quoted_columns", {"columns": ["x", "y", "z"]})

# ── journal_mode (fetchone row -> str().lower()) ──
add("journal_mode", {}, {"journal_mode": Q(["WAL"])})
add("journal_mode", {}, {"journal_mode": Q(["DELETE"])})
add("journal_mode", {}, {"journal_mode": Q(["MeMoRy"])})
add("journal_mode", {}, {"journal_mode": Q([None])})                        # -> None
add("journal_mode", {}, {"journal_mode": Q([])})                            # falsy [] row -> None
add("journal_mode", {}, {"journal_mode": Q([42])})                          # str(42) -> "42"
add("journal_mode", {}, {"journal_mode": R("DatabaseError", "prag broken")})
add("journal_mode", {}, {})                                                 # fail-closed

# ── compatible_columns (table_columns x2; mutates result carrier) ──
add("compatible_columns", {"table": "sessions", "result": {"source_rows": 5}},
    {"table_columns": Q(["id", "value"], ["id", "value", "extra"])})
add("compatible_columns", {"table": "sessions", "result": {}},
    {"table_columns": Q([], ["id"])})
add("compatible_columns", {"table": "sessions", "result": {"x": 1}},
    {"table_columns": Q(["a"], ["b"])})
add("compatible_columns", {"table": "sessions", "result": {}},
    {"table_columns": Q(["b", "a"], ["a", "c", "b"])})
add("compatible_columns", {"table": "sessions", "result": {}},
    {"table_columns": Q(R("DatabaseError", "src schema"), ["id"])})
add("compatible_columns", {"table": "sessions", "result": {}},
    {"table_columns": Q(["id"], R("DatabaseError", "dst schema"))})
add("compatible_columns", {"table": "sessions", "result": {}}, {})          # fail-closed

# ── state_meta_result (pure dict builder) ──
add("state_meta_result", {"source_rows": 9, "extra": {}})
add("state_meta_result", {"source_rows": None, "extra": {"status": "missing"}})
add("state_meta_result", {"source_rows": 0,
                          "extra": {"mode": "rowid_range_salvage", "status": "failed",
                                    "error": "destination state_meta schema is incomplete"}})
add("state_meta_result", {"source_rows": 3, "extra": {"status": "complete", "copied_rows": 3}})
add("state_meta_result", {"source_rows": 1, "extra": {"custom": [1, 2]}})

# ── state_meta_precheck (table_columns x1..2, composes state_meta_result) ──
add("state_meta_precheck", {"source_rows": 4, "salvage": False},
    {"table_columns": Q(["key", "value"], ["key", "value"])})
add("state_meta_precheck", {"source_rows": 4, "salvage": False},
    {"table_columns": Q([])})                                                # missing (1 call)
add("state_meta_precheck", {"source_rows": 4, "salvage": True},
    {"table_columns": Q([])})                                                # missing (1 call)
add("state_meta_precheck", {"source_rows": 4, "salvage": True},
    {"table_columns": Q(["key"])})                                           # failed: found: key
add("state_meta_precheck", {"source_rows": 4, "salvage": True},
    {"table_columns": Q(["key", "foo"], ["key"])})                           # dest incomplete
add("state_meta_precheck", {"source_rows": 4, "salvage": True},
    {"table_columns": Q(["value", "key"], ["value", "key"])})                # set subset ok
add("state_meta_precheck", {"source_rows": 4, "salvage": False},
    {"table_columns": Q(R("DatabaseError", "src schema"))})
add("state_meta_precheck", {"source_rows": 4, "salvage": False}, {})         # fail-closed

# ── reconcile (count + conditional mutate) ──
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {"reconcile_count": Q([3]), "reconcile_mutate": Q([None])})
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {"reconcile_count": Q([0]), "reconcile_mutate": Q([None])})
add("reconcile", {"table": "sessions", "where": "b = 2", "mutation": "UPDATE sessions SET a = NULL"},
    {"reconcile_count": Q(["4"]), "reconcile_mutate": Q([None])})
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {"reconcile_count": R("DatabaseError", "count failed"), "reconcile_mutate": Q([None])})
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {"reconcile_count": Q([2]), "reconcile_mutate": R("SystemExit", "mutate aborted")})
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {"reconcile_count": Q([None])})                                          # TypeError
add("reconcile", {"table": "sessions", "where": "a = 1", "mutation": "DELETE FROM sessions"},
    {})                                                                      # fail-closed

# ── immediate_transaction (generator CM: begin/commit/rollback) ──
add("immediate_transaction", {"outcome": None},
    {"begin": None, "commit": None, "rollback": None})
add("immediate_transaction", {"outcome": R("ValueError", "body failed")},
    {"begin": None, "commit": None, "rollback": None})
add("immediate_transaction", {"outcome": R("SystemExit", "body exits")},
    {"begin": None, "commit": None, "rollback": None})
add("immediate_transaction", {"outcome": R("KeyboardInterrupt", "ctrl-c")},
    {"begin": None, "commit": None, "rollback": None})
add("immediate_transaction", {"outcome": None},
    {"begin": None, "commit": R("DatabaseError", "commit failed"), "rollback": None})
add("immediate_transaction", {"outcome": None},
    {"begin": R("DatabaseError", "begin failed"), "commit": None, "rollback": None})
add("immediate_transaction", {"outcome": None}, {})                         # fail-closed (begin)

# ── verify_fts_indexes (table_columns + insert/match; except DatabaseError) ──
add("verify_fts_indexes", {"verification": {"errors": []}},
    {"table_columns": Q(["id"], ["id"]), "fts_insert": None, "fts_match": None})
add("verify_fts_indexes", {"verification": {"errors": []}},
    {"table_columns": Q([], ["id"]), "fts_insert": None, "fts_match": None})
add("verify_fts_indexes", {"verification": {"errors": ["existing"]}},
    {"table_columns": Q(R("DatabaseError", "schema gone"), [])})
add("verify_fts_indexes", {"verification": {"errors": []}},
    {"table_columns": Q(["id"], []),
     "fts_insert": Q(R("DatabaseError", "fts broken"))})
add("verify_fts_indexes", {"verification": {"errors": []}},
    {"table_columns": Q(["id"], []),
     "fts_insert": Q([None]),
     "fts_match": Q(R("OperationalError", "match broken"))})
add("verify_fts_indexes", {"verification": {"errors": []}},
    {"table_columns": Q(["id"], []),
     "fts_insert": Q(R("ValueError", "not a db error"))})
add("verify_fts_indexes", {"verification": {"errors": []}}, {})              # fail-closed

# ── verify_structure (5 pragma seams, composes journal_mode) ──
add("verify_structure", {"verification": {"errors": []}},
    {"integrity_check": Q([["ok"]]), "foreign_key_check": Q([]),
     "journal_mode": Q(["wal"]), "schema_version": Q([33]),
     "fts_meta": Q([["fts_storage_version", "3"]])})
add("verify_structure", {"verification": {"errors": []}},
    {"integrity_check": Q([["ok"], ["corrupt"]]), "foreign_key_check": Q([["sessions", "m", "sid"]]),
     "journal_mode": Q(["delete"]), "schema_version": Q([31]),
     "fts_meta": Q([["fts_storage_version", "2"], ["fts_rebuild_progress", "1"]])})
add("verify_structure", {"verification": {"errors": []}},
    {"integrity_check": Q([["ok"]]), "foreign_key_check": Q([]),
     "journal_mode": Q([None]), "schema_version": Q([]),
     "fts_meta": Q([])})
add("verify_structure", {"verification": {"errors": []}},
    {"integrity_check": R("DatabaseError", "integrity failed"),
     "foreign_key_check": Q([]), "journal_mode": Q(["wal"]),
     "schema_version": Q([33]), "fts_meta": Q([["fts_storage_version", "3"]])})
add("verify_structure", {"verification": {"errors": ["pre"]}},
    {"integrity_check": Q([["ok"]]), "foreign_key_check": R("SystemExit", "fk aborts"),
     "journal_mode": Q(["wal"]), "schema_version": Q([33]),
     "fts_meta": Q([["fts_storage_version", "3"]])})
add("verify_structure", {"verification": {"errors": []}}, {})                # fail-closed

# ── sanitize_session_model_config (table_columns + transaction + reconcile) ──
add("sanitize_session_model_config", {},
    {"table_columns": Q(["id"])})                                            # no model_config -> 0
add("sanitize_session_model_config", {},
    {"table_columns": Q(["id", "model_config"]), "begin": None, "commit": None,
     "reconcile_count": Q([2]), "reconcile_mutate": Q([None])})
add("sanitize_session_model_config", {},
    {"table_columns": Q(["model_config"]), "begin": None, "commit": None,
     "reconcile_count": Q([0]), "reconcile_mutate": Q([None])})
add("sanitize_session_model_config", {},
    {"table_columns": Q(["model_config"]), "begin": None, "rollback": None,
     "reconcile_count": R("DatabaseError", "count failed")})
add("sanitize_session_model_config", {},
    {"table_columns": Q(["model_config"]), "begin": R("DatabaseError", "begin failed")})
add("sanitize_session_model_config", {},
    {"table_columns": R("SystemExit", "schema aborted")})
add("sanitize_session_model_config", {}, {})                                 # fail-closed

# ── recovery_report (source_fingerprint seam; mutates verification) ──
_FP = {"main": {"size": 10, "mtime_ns": 1}}
_BASE_INSP = {"journal_mode": "wal", "tables": {"sessions": {"rows": 2}},
              "errors": [], "warnings": ["w1"],
              "source_bundle": ["a.db", "a.db-wal"],
              "source_fingerprint": _FP}
add("recovery_report",
    {"source": "/h/a.db", "output": "/h/out.db", "inspection": _BASE_INSP,
     "disk_space": {"source_bundle_bytes": 10},
     "verification": {"errors": [], "healthy": True, "complete": True,
                      "loss_detected": False},
     "on_source_change": "healthy", "fields": {"mode": "full"}},
    {"source_fingerprint": {"main": {"size": 10, "mtime_ns": 1}}})
add("recovery_report",
    {"source": "/h/a.db", "output": "/h/out.db", "inspection": _BASE_INSP,
     "disk_space": {"source_bundle_bytes": 10},
     "verification": {"errors": [], "healthy": True, "complete": True,
                      "loss_detected": False},
     "on_source_change": "healthy", "fields": {}},
    {"source_fingerprint": {"main": {"size": 99, "mtime_ns": 2}}})            # changed
add("recovery_report",
    {"source": "/h/a.db", "output": "/h/out.db", "inspection": _BASE_INSP,
     "disk_space": {}, "verification": {"errors": [], "healthy": False,
     "complete": False, "loss_detected": True},
     "on_source_change": "complete", "fields": {"best_effort": True}},
    {"source_fingerprint": {"main": {"size": 10, "mtime_ns": 1}}})
add("recovery_report",
    {"source": "/h/a.db", "output": "/h/out.db", "inspection": _BASE_INSP,
     "disk_space": {}, "verification": {"errors": [], "healthy": True,
     "complete": False, "loss_detected": False},
     "on_source_change": "healthy", "fields": {"copied": {"sessions": 1}}},
    {"source_fingerprint": R("OSError", "stat failed")})
add("recovery_report",
    {"source": "/h/a.db", "output": "/h/out.db", "inspection": _BASE_INSP,
     "disk_space": {}, "verification": {"errors": [], "healthy": True,
     "complete": True, "loss_detected": False},
     "on_source_change": "healthy", "fields": {}}, {})                       # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
