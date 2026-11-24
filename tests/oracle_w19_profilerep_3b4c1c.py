#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/profile_repair.py (gate w19_profilerep_3b4c1c).

The live class methods are executed against a real in-memory SQLite store filled from the
fixture's rows, so the compared value is what upstream computes: the finders, the topic-row
filter, the chat-profile evidence set and the move-import decision.
"""
import ast
import contextlib
import importlib.util
import json
import sqlite3
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
Q = REPO.parent / "hermes-agent"
SRC = Q / "persistence" / "sqlite" / "admin" / "profile_repair.py"
GATE = "w19_profilerep_3b4c1c"
_ANN = __future__.annotations.compiler_flag

WANT_FUNCS = {"_stored_profile", "_table_columns", "_insert_row", "_ledgered_session_ids"}
WANT_METHODS = {"find_crossed_profile_sessions", "find_profile_less_telegram_topic_rows",
                "key_profiles_for_chat", "import_moved_session"}


def load_keys():
    spec = importlib.util.spec_from_file_location("w19_pr_keys", Q / "persistence" / "core" / "keys.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class _StoreHolder:
    @staticmethod
    def _store_system_prompt(conn, prompt):
        return None if prompt is None else "h:" + str(prompt)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    keys = load_keys()
    g = {
        "__name__": "pr_live",
        "session_key_profile": keys.session_key_profile,
        "MAX_TITLE_LENGTH": 100,
        "_MESSAGE_MOVE_SKIP": frozenset({"id", "display_order"}),
        "StoreHolder": _StoreHolder,
        "Optional": object, "Any": object, "Iterable": object,
    }
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
            _run(node, g)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteProfileRepairAdmin":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in WANT_METHODS:
                    _run(sub, g)
    return g


def _run(node, g):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, str(SRC), "exec", flags=_ANN, dont_inherit=True), g)


G = extract()

# id / parent_session_id stay affinity-free: the store keeps whatever JSON scalar the fixture
# gave them, which is what the C side reads straight off the wire.
SESSIONS = ("CREATE TABLE sessions (id PRIMARY KEY, session_key TEXT, profile_name TEXT, "
            "parent_session_id, message_count INTEGER, started_at REAL, source TEXT, "
            "chat_id TEXT, title TEXT, system_prompt_hash TEXT, tool_names TEXT)")


class Holder:
    def __init__(self, conn):
        self.conn = conn

    def _read_all(self, sql, params=()):
        cur = self.conn.execute(sql, params)
        cols = [d[0] for d in cur.description]
        return [dict(zip(cols, r)) for r in cur.fetchall()]

    def _read_one(self, sql, params=()):
        return self.conn.execute(sql, params).fetchone()

    def _read_retrying_ioerr(self, fn):
        return fn(self.conn)

    def _execute_write(self, fn):
        out = fn(self.conn)
        self.conn.commit()
        return out


@contextlib.contextmanager
def _ctx(conn):
    yield Holder(conn)


class FakeSelf:
    read_only = False

    def __init__(self, conn):
        self._conn = conn

    def _absent(self):
        return False

    def _reader(self):
        return _ctx(self._conn)

    def _writer(self, _name):
        return _ctx(self._conn)


def fresh_db():
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    conn.execute(SESSIONS)
    return conn


def insert_sessions(conn, rows):
    for row in rows:
        cols = [k for k in row if k != "parent_session_key"]
        conn.execute("INSERT INTO sessions (%s) VALUES (%s)" % (", ".join(cols), ",".join("?" * len(cols))),
                     [row[c] for c in cols])
    conn.commit()


def case_stored_profile(r):
    return G["_stored_profile"](r.get("value"))


def case_find_crossed(r):
    conn = fresh_db()
    conn.execute("CREATE TABLE worker_executions (session_id TEXT)")
    for sid in r.get("ledgered") or []:
        conn.execute("INSERT INTO worker_executions (session_id) VALUES (?)", (sid,))
    insert_sessions(conn, r.get("rows") or [])
    return G["find_crossed_profile_sessions"](FakeSelf(conn), r.get("owner", ""))


def case_less_topic_rows(r):
    conn = fresh_db()
    has_profile = bool(r.get("has_profile"))
    cols = "chat_id INTEGER, thread_id INTEGER, session_key TEXT"
    if has_profile:
        cols = "chat_id INTEGER, thread_id INTEGER, session_key TEXT, profile_name TEXT"
    conn.execute("CREATE TABLE telegram_dm_topic_bindings (%s)" % cols)
    for row in r.get("rows") or []:
        if has_profile:
            conn.execute("INSERT INTO telegram_dm_topic_bindings (chat_id, thread_id, session_key, profile_name) "
                         "VALUES (?, ?, ?, ?)",
                         (row["chat_id"], row["thread_id"], row["session_key"], "default"))
        else:
            conn.execute("INSERT INTO telegram_dm_topic_bindings (chat_id, thread_id, session_key) VALUES (?, ?, ?)",
                         (row["chat_id"], row["thread_id"], row["session_key"]))
    conn.commit()
    return G["find_profile_less_telegram_topic_rows"](FakeSelf(conn))


def case_key_profiles(r):
    conn = fresh_db()
    for key in r.get("session_keys") or []:
        conn.execute("INSERT INTO sessions (id, session_key, source, chat_id) VALUES (?, ?, ?, ?)",
                     (str(key), key, "telegram", "c1"))
    conn.commit()
    return sorted(G["key_profiles_for_chat"](FakeSelf(conn), "telegram", "c1"))


def case_import_moved(r):
    conn = fresh_db()
    session = dict(r.get("session") or {})
    sid = session.get("id")
    if r.get("id_present"):
        insert_sessions(conn, [session])
    if r.get("parent_present"):
        insert_sessions(conn, [{"id": "p1", "session_key": "agent:alice:x:1"}])
    if r.get("title_present") and session.get("title") is not None:
        insert_sessions(conn, [{"id": "resident", "session_key": "agent:alice:x:2", "title": session["title"]}])
    payload = {"session": session}
    status = G["import_moved_session"](FakeSelf(conn), payload, profile_name=r.get("profile_name", "default"))
    row = conn.execute("SELECT title, profile_name, parent_session_id FROM sessions WHERE id = ?",
                       (sid,)).fetchone()
    return {"status": status,
            "title": row["title"] if row else None,
            "profile_name": row["profile_name"] if row else None,
            "parent_session_id": row["parent_session_id"] if row else None}


CASES = {
    "stored_profile": case_stored_profile,
    "find_crossed": case_find_crossed,
    "less_topic_rows": case_less_topic_rows,
    "key_profiles": case_key_profiles,
    "import_moved": case_import_moved,
}


def norm(v):
    """JSON round-trip so tuple/list and bool/int shapes compare as the wire format does."""
    return json.loads(json.dumps(v, default=str))


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
        f = r.get("func")
        if f not in CASES:
            continue
        cases += 1
        try:
            py = norm(CASES[f](r))
        except BaseException as exc:  # noqa: BLE001 - a live raise is a mismatch, not a crash
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
