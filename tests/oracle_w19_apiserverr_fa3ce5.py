#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_response_store.py (gate w19_apiserverr_fa3ce5).

Extracts the live methods via ast.parse and runs them against a scripted fake connection, so the
SQL-bearing bodies execute their real decisions while the C side ports those same decisions.
"""
import ast
import json
import sys
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "gateway" / "platforms" / "api_server_response_store.py"
GATE = "w19_apiserverr_fa3ce5"
METHODS = {"_identity_bound", "put", "delete", "request_key_matches"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_apiserverr_live", "__builtins__": __builtins__, "time": time,
         "json": json}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) in ("MAX_STORED_RESPONSES", "IDENTITY_RETENTION_FACTOR")
                for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        if isinstance(node, ast.FunctionDef) and node.name == "_locked":
            g["_locked"] = _identity_decorator
        if isinstance(node, ast.ClassDef) and node.name == "ResponseStore":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in METHODS:
                    sub.decorator_list = []          # drop @_locked: the fake needs no lock
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), str(SRC), "exec"), g)
    return g


def _identity_decorator(method):
    return method


G = extract()


class Cursor:
    def __init__(self, rowcount=0):
        self.rowcount = rowcount

    def fetchone(self):
        return None


class FakeConn:
    """Records the statements a method issues; answers the counts the method asks for."""

    def __init__(self, row_deleted=0, fetch_row=None):
        self.statements = []
        self.row_deleted = row_deleted
        self.fetch_row = fetch_row
        self.committed = 0

    def execute(self, sql, params=()):
        self.statements.append(sql)
        if "FROM responses" in sql and sql.lstrip().startswith("DELETE"):
            return Cursor(self.row_deleted)
        return Cursor(0)

    def executemany(self, sql, params):
        self.statements.append(sql)
        return Cursor(0)

    def commit(self):
        self.committed += 1


class FakeStore:
    """The attributes/methods the ported bodies touch."""

    def __init__(self, conn, forgotten=0, max_identities=None, max_size=None):
        self._conn = conn
        self._forgotten = forgotten
        self._max_identities = max_identities
        self._max_size = max_size
        self.settled_calls = 0

    def _mark_settled(self, selector, params):
        self._conn.statements.append("SETTLE " + selector)

    def _evict_and_commit(self):
        pass

    def _settle_terminal_admissions(self, at=None):
        self.settled_calls += 1

    def _forget_identities(self, selector, params):
        return self._forgotten


def run_case(r):
    f = r.get("func")
    if f == "identity_bound":
        store = FakeStore(None, max_identities=r.get("max_identities"),
                          max_size=r.get("max_size", 100))
        return G["_identity_bound"](store)
    if f == "put_plan":
        conn = FakeConn()
        store = FakeStore(conn)
        G["put"](store, r.get("response_id", ""), r.get("data") or {})
        first = conn.statements[0] if conn.statements else ""
        verb = first.split(" INTO")[0]
        return {"verb": verb, "settles": any(s.startswith("SETTLE") for s in conn.statements)}
    if f == "delete_result":
        conn = FakeConn(row_deleted=int(r.get("row_deleted") or 0))
        store = FakeStore(conn, forgotten=int(r.get("forgotten") or 0))
        return bool(G["delete"](store, r.get("response_id", "")))
    if f == "request_key_matches":
        row = None
        if r.get("has_row"):
            row = (r.get("stored_fp"),)
        conn = FakeConn()
        conn.execute = lambda sql, params=(), _row=row: _CursorWithRow(_row)
        store = FakeStore(conn)
        return bool(G["request_key_matches"](store, r.get("key", "k"), r.get("fingerprint")))
    raise KeyError(f)


class _CursorWithRow:
    def __init__(self, row):
        self.rowcount = 0
        self._row = row

    def fetchone(self):
        return self._row


FUNCS = {"identity_bound", "put_plan", "delete_result", "request_key_matches"}


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
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:  # noqa: BLE001
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
