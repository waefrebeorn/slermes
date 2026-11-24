#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/usage_analytics.py (gate w19_usageanaly_6ec515)."""
import ast
import json
import sqlite3
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "conversation" / "usage_analytics.py"
GATE = "w19_usageanaly_6ec515"
FUNCS = {"scoped", "local_surface", "query_sql", "query_params", "insight_wait"}
BASES = {"_GET_SESSIONS", "_GET_TOOL_CALLS", "_GET_SKILL_CALLS", "_GET_TOOL_NAMES",
         "_GET_MESSAGE_STATS", "_GET_MODEL_USAGE"}


class _Cur:
    def __init__(self, rows):
        self._rows = rows

    def fetchone(self):
        return self._rows[0] if self._rows else None

    def fetchall(self):
        return list(self._rows)

    def __iter__(self):
        return iter(self._rows)


class FakeConn:
    def __init__(self, has_index, local, record=None):
        self.has_index = has_index
        self.local = local
        self.record = record

    def execute(self, sql, params=None):
        if self.record is not None:
            self.record.append((sql, params))
        if "sqlite_master" in sql:
            return _Cur([(1,)] if self.has_index else [])
        if sql.strip().startswith("PRAGMA table_info(sessions)"):
            cols = [(0, "id"), (1, "source")]
            cols.append((2, "created_source") if self.local else (2, "created_at"))
            return _Cur(cols)
        return _Cur([])


def load_live():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "ua_live", "sqlite3": sqlite3, "Optional": object, "Any": object}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", None) == "_LOCAL_SURFACE"
                                                for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name == "_InsightQueries":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.FunctionDef) and node.name == "_scoped":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteUsageAnalytics":
            for item in node.body:
                if isinstance(item, ast.FunctionDef) and item.name == "_insight_wait":
                    exec(compile(ast.Module(body=[item], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = load_live()


class Holder:
    def __init__(self, read_only, busy):
        self.read_only = read_only
        self._read_busy_timeout_s = busy


def run_case(r):
    f = r.get("func")
    if f == "scoped":
        pair = G["_scoped"](r.get("before") or "", r.get("after") or "", alias=r.get("alias", "s."))
        return {"all": pair[0], "with_source": pair[1]}
    if f == "local_surface":
        return G["_LOCAL_SURFACE"].format(a=r.get("alias") or "")
    if f == "query_sql":
        base = r.get("base")
        if base not in BASES:
            return None
        q = G["_InsightQueries"](FakeConn(bool(r.get("has_index")), bool(r.get("local_surfaces"))))
        return {"all": getattr(q, base + "_ALL"), "with_source": getattr(q, base + "_WITH_SOURCE")}
    if f == "query_params":
        q = G["_InsightQueries"](FakeConn(True, bool(r.get("local_surfaces"))))
        rec = []
        conn = FakeConn(True, bool(r.get("local_surfaces")), record=rec)
        q._query(conn, "_GET_SESSIONS", r.get("cutoff"), r.get("source"))
        return list(rec[-1][1]) if rec else []
    if f == "insight_wait":
        return G["_insight_wait"](Holder(bool(r.get("read_only")), r.get("read_busy_timeout_s")))
    return None


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
