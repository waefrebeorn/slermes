#!/usr/bin/env python3
"""Differential oracle for gate w13_kanbandbg_30158b (hermes_cli/kanban_db_graph.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_kanbandbg_30158b"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/hermes_cli/kanban_db_graph.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


class FakeConn:
    def __init__(self, rows):
        self.rows = rows or {}

    def execute(self, sql, params):
        out = []
        for pid in params:
            if pid in self.rows:
                row = dict(self.rows[pid])
                row["id"] = pid
                out.append(row)
        return out


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "initial_state":
        try:
            st, ten = live.initial_task_state(
                FakeConn(r.get("rows")), tuple(r.get("parents") or []),
                r.get("initial_status"), bool(r.get("triage")), r.get("tenant"))
            return [st, ten]
        except ValueError as exc:
            return {"error": str(exc)}
    if func == "validate_graph":
        try:
            live._validate_children_graph(r.get("children") or [])
            return {"ok": True}
        except ValueError as exc:
            return {"error": str(exc)}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
