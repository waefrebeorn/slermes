#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/_state/repair.py (gate w28_repair_1f5388_p2).

The classification and statement-construction layer, exercised by driving the
LIVE module with a recording connection: every statement the repair strategies
and the health probe execute is captured as upstream issued it, so the SQL text
and the bound parameters compare byte-for-byte.  The other seams (iterdir,
unlink, copy2, the wall clock) are likewise injected.

  * ``integrity_damage_is_structural`` -- which integrity_check lines the FTS
    ladder cannot heal;
  * ``_db_opens_cleanly`` -- the problem summary and the probe's statement plan;
  * ``_strategy_rebuild_fts`` / ``_strategy_reindex`` / ``_strategy_dedup_schema``
    / ``_strategy_drop_fts_vacuum`` -- the exact statement streams;
  * ``_bump_schema_cookie``, ``_db_fingerprint``'s key format;
  * the ledger/backup naming, the collision loop, prune selection and the
    attempt arithmetic.
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_repair_1f5388_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite._state import repair as live  # noqa: E402
from persistence.sqlite import _schema as schema_mod  # noqa: E402

WANTED = {
    "integrity_structural", "probe_reason", "strategy_statements", "probe_plan",
    "schema_version_sql", "fingerprint_text", "ledger_name", "scratch_name",
    "staging_name", "backup_name", "existing_backups", "prune_victims",
    "attempts_exhausted", "next_attempts",
}


def split_path(p):
    for i in range(len(p) - 1, -1, -1):
        if p[i] in "/\\":
            return p[: i + 1], p[i + 1:]
    return "", p


class FakePath:
    """The slice of pathlib.Path repair.py touches, driven by fixtures."""

    def __init__(self, path, sizes=None, entries=None, texts=None, deleted=None):
        self.path = path
        self.sizes = dict(sizes or {})
        self.entries = list(entries or [])
        self.texts = texts if texts is not None else {}
        self.deleted = deleted if deleted is not None else []

    def __str__(self):
        return self.path

    def __fspath__(self):
        return self.path

    @property
    def name(self):
        return split_path(self.path)[1]

    @property
    def parent(self):
        return FakePath(split_path(self.path)[0].rstrip("/\\") or ".",
                        entries=self.entries, texts=self.texts, deleted=self.deleted)

    def with_name(self, new):
        return FakePath(split_path(self.path)[0] + new, self.sizes, self.entries,
                        self.texts, self.deleted)

    def stat(self):
        if self.path not in self.sizes:
            raise FileNotFoundError(2, "No such file", self.path)
        return type("Stat", (), {"st_size": self.sizes[self.path]})()

    def exists(self):
        return self.path in self.sizes

    def iterdir(self):
        return [FakePath(n, self.sizes, self.entries, self.texts, self.deleted) for n in self.entries]

    def glob(self, pattern):
        return []

    def unlink(self, missing_ok=False):
        self.deleted.append(self.path)
        return None

    def write_text(self, text, encoding="utf-8"):
        self.texts[self.path] = text
        return len(text)


class Patch:
    def __init__(self, obj, attr, value):
        self.obj, self.attr, self.value, self.saved = obj, attr, value, None

    def __enter__(self):
        self.saved = getattr(self.obj, self.attr)
        setattr(self.obj, self.attr, self.value)
        return self

    def __exit__(self, *exc):
        setattr(self.obj, self.attr, self.saved)
        return False


class FakeCursor:
    def __init__(self, conn):
        self.conn = conn

    def fetchall(self):
        return self.conn.last_rows

    def fetchone(self):
        return self.conn.last_one


class FakeConn:
    """Recording stand-in for a repair connection: answers what the probe and
    the strategy bodies read, records everything they execute."""

    def __init__(self, integrity_rows=None, dupes=None, schema_version=42, match_rows=(1,)):
        self.calls = []
        self.in_transaction = False
        self.integrity_rows = list(integrity_rows or [("ok",)])
        self.dupes = list(dupes or [])
        self.schema_version = schema_version
        self.match_rows = match_rows
        self.last_rows = []
        self.last_one = None

    def cursor(self):
        return FakeCursor(self)

    def commit(self):
        return None

    def execute(self, sql, params=()):
        self.calls.append({"sql": sql, "params": list(params) if params else []})
        if sql.startswith("PRAGMA integrity_check"):
            self.last_rows = self.integrity_rows
        elif sql.startswith("SELECT type, name"):
            self.last_rows = self.dupes
        elif sql == "PRAGMA schema_version":
            self.last_one = (self.schema_version,)
            self.last_rows = [self.last_one]
        elif sql.startswith("SELECT"):
            self.last_one = self.match_rows
            self.last_rows = [self.match_rows]
        else:
            self.last_one = None
            self.last_rows = []
        return FakeCursor(self)

    def close(self):
        return None


def integrity_structural(r):
    rows = [tuple(x) for x in (r.get("master_rows") or [])]
    return bool(live.integrity_damage_is_structural(r.get("lines") or [], rows))


def _probe(conn):
    with Patch(live, "_connect_repair_durable", lambda p: conn), \
            Patch(schema_mod, "retired_cjk_index_present", lambda cur: False):
        return live._db_opens_cleanly(FakePath("state.db"))


def probe_reason(r):
    conn = FakeConn(integrity_rows=[tuple(x) for x in (r.get("rows") or [])])
    return _probe(conn)


def strategy_statements(r):
    name = r.get("name")
    dupes = [tuple(x) for x in (r.get("dupes") or [])]
    conn = FakeConn(dupes=dupes, schema_version=r.get("schema_version", 42))
    home = FakePath(".")
    table = {n: body for n, body, _s, _f in live._REPAIR_STRATEGIES}
    body = table.get(name)
    if body is None:
        return None
    with Patch(live, "_reapply_durability_barriers", lambda c: True):
        body(conn, home)
    return conn.calls


def probe_plan(_r):
    conn = FakeConn()
    _probe(conn)
    return [c["sql"] for c in conn.calls]


def schema_version_sql(r):
    conn = FakeConn(schema_version=r.get("current", 42))
    live._bump_schema_cookie(conn)
    for call in conn.calls:
        if call["sql"].startswith("PRAGMA schema_version="):
            return call["sql"]
    return None


def fingerprint_text(r):
    return "%d:%s" % (int(r.get("size") or 0), (r.get("digest_hex") or "")[:32])


def ledger_name(r):
    return live._repair_ledger_path(FakePath(r.get("db_name") or "state.db")).name


def scratch_name(r):
    db = FakePath(r.get("db_name") or "state.db")
    return db.with_name(db.name + ".repair-scratch").name


def staging_name(r):
    db = FakePath(r.get("db_name") or "state.db")
    return db.with_name(db.name + ".backup-staging-" + str(r.get("stamp") or "")).name


class _FakeNow:
    def __init__(self, stamp):
        self.stamp = stamp

    def strftime(self, fmt):
        return self.stamp


class _FakeDatetime:
    def __init__(self, stamp):
        self.stamp = stamp

    def now(self):
        return _FakeNow(self.stamp)


def backup_name(r):
    db_name = r.get("db_name") or "state.db"
    stamp = str(r.get("stamp") or "")
    existing = list(r.get("existing") or [])
    captured = {}

    def publish(db_path, staging, backup):
        captured["staging"] = str(staging)
        captured["backup"] = str(backup)

    old_cwd = os.getcwd()
    with tempfile.TemporaryDirectory() as tmp:
        os.chdir(tmp)
        try:
            sizes = {n: 1 for n in existing}
            sizes[db_name] = 4096
            db = FakePath(db_name, sizes=sizes)
            fake_dt = type("dt", (), {"datetime": _FakeDatetime(stamp)})
            with Patch(live, "datetime", fake_dt), \
                    Patch(live, "_existing_malformed_backups", lambda p: []), \
                    Patch(live, "_backup_content_identity", lambda p: None), \
                    Patch(live, "_backup_free_space_error", lambda p: None), \
                    Patch(live, "_publish_backup_bundle", publish), \
                    Patch(schema_mod, "retired_cjk_index_present", lambda cur: False):
                try:
                    import hermes_cli.sqlite_safe_read as safe
                    saved = safe.has_live_connection
                    safe.has_live_connection = lambda p: False
                except ImportError:
                    saved = None
                try:
                    path, error = live._backup_db_file(db)
                finally:
                    if saved is not None:
                        safe.has_live_connection = saved
            if path is None:
                return {"backup": None, "error": error}
            return path.name
        finally:
            os.chdir(old_cwd)


def existing_backups(r):
    db = FakePath(r.get("db_name") or "state.db", entries=list(r.get("names") or []))
    return [p.name for p in live._existing_malformed_backups(db)]


def prune_victims(r):
    deleted = []
    db = FakePath(r.get("db_name") or "state.db", entries=list(r.get("names") or []),
                  deleted=deleted)
    live._prune_malformed_backups(db, keep=int(r.get("keep", live._MAX_MALFORMED_BACKUPS)))
    return deleted


def attempts_exhausted(r):
    ledger = {"fingerprint": r.get("recorded"), "failed_attempts": r.get("failed_attempts")}
    db = FakePath("state.db", sizes={"state.db": r.get("db_size") or 0})
    with Patch(live, "_read_repair_ledger", lambda p: dict(ledger)), \
            Patch(live, "_db_fingerprint", lambda p: r.get("fp")):
        return bool(live._persistent_repair_attempts_exhausted(db))


def next_attempts(r):
    ledger = {"fingerprint": r.get("recorded"), "failed_attempts": r.get("failed_attempts")}
    texts = {}
    ledger_path = FakePath("state.db.repair-attempts.json", texts=texts)
    db = FakePath("state.db")
    with Patch(live, "_read_repair_ledger", lambda p: dict(ledger)), \
            Patch(live, "_db_fingerprint", lambda p: r.get("fp")), \
            Patch(live, "_repair_ledger_path", lambda p: ledger_path):
        live._record_repair_outcome(db, repaired=False)
    raw = texts.get(ledger_path.path)
    if raw is None:
        return None
    return float(json.loads(raw)["failed_attempts"])


DISPATCH = {
    "integrity_structural": integrity_structural,
    "probe_reason": probe_reason,
    "strategy_statements": strategy_statements,
    "probe_plan": probe_plan,
    "schema_version_sql": schema_version_sql,
    "fingerprint_text": fingerprint_text,
    "ledger_name": ledger_name,
    "scratch_name": scratch_name,
    "staging_name": staging_name,
    "backup_name": backup_name,
    "existing_backups": existing_backups,
    "prune_victims": prune_victims,
    "attempts_exhausted": attempts_exhausted,
    "next_attempts": next_attempts,
}


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
