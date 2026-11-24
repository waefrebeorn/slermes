#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_maintenance.py (gate w19_maintenanc_c262d4).

Execs the live AST classes into stub-filled globals: `_next_chunk_rows` and the two
progress dataclasses run as-is (the vacuum clock is a fixed monotonic seam), and the
`MaintenanceAdmin` page-stat methods run with `_page_pragmas` stubbed to the fixture's
values so the arithmetic is the live one.
"""
import ast
import dataclasses
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/persistence/sqlite/admin/_maintenance.py"
GATE = "w19_maintenanc_c262d4"
_ANN = __import__("__future__").annotations.compiler_flag


def _exec_node(node, g):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)


_live = types.ModuleType("maintenance_live")
_live.__package__ = "persistence.sqlite.admin"
sys.modules["maintenance_live"] = _live
g = _live.__dict__

NOW = {"now": 1000.0}
g.update({
    "dataclass": dataclasses.dataclass, "field": dataclasses.field,
    "time": SimpleNamespace(monotonic=lambda: NOW["now"]),
    "Any": object, "Callable": object, "Optional": object,
    "contextlib": __import__("contextlib"), "threading": __import__("threading"),
    "partial": __import__("functools").partial, "logger": __import__("logging").getLogger("oracle"),
})


class AdminFile:  # stand-in base for the admin mixin
    def __init__(self, db_path=None):
        self.db_path = db_path


g["AdminFile"] = AdminFile

WANTED = {"_FtsStorageOptimizer", "_TeardownProgress", "_VacuumProgress", "MaintenanceAdmin"}
tree = ast.parse(open(SRC, encoding="utf-8").read())
for node in tree.body:
    if isinstance(node, ast.ClassDef) and node.name in WANTED:
        _exec_node(node, g)

Optimizer = g["_FtsStorageOptimizer"]
Teardown = g["_TeardownProgress"]
Vacuum = g["_VacuumProgress"]
Admin = g["MaintenanceAdmin"]

_opt = object.__new__(Optimizer)
_opt._fts_chunk_rows = 500


def run_case(r):
    f = r.get("func")
    if f == "next_chunk_rows":
        return _opt._next_chunk_rows(r.get("rows"), r.get("cost_s"), r.get("target_s"))
    if f == "teardown_info":
        prog = Teardown(table=r.get("table") or "", table_index=r.get("table_index") or 0,
                        table_count=r.get("table_count") or 0, total=r.get("total") or 0,
                        drained=r.get("drained") or 0)
        return prog.info()
    if f == "vacuum_info":
        elapsed = r.get("elapsed") or 0
        prog = Vacuum(size_bytes=r.get("size_bytes") or 0,
                      estimated_bytes=r.get("estimated_bytes") or 0,
                      started=NOW["now"] - float(elapsed))
        return prog.info()
    if f in ("logical_size", "freelist_ratio"):
        ok = r.get("ok", True)
        if f == "logical_size":
            vals = [r.get("page_count"), r.get("page_size")]
        else:
            vals = [r.get("page_count"), r.get("freelist_count")]
        admin = object.__new__(Admin)
        admin._page_pragmas = lambda names, msg, _v=vals: None if not ok else _v
        return admin.logical_size_bytes() if f == "logical_size" else admin.freelist_ratio()
    return "SKIP"


FUNCS = {"next_chunk_rows", "teardown_info", "vacuum_info", "logical_size", "freelist_ratio"}


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
        except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if isinstance(py, dict) and isinstance(got, dict):
            same = py == got
        elif isinstance(py, (int, float)) and isinstance(got, (int, float)):
            same = abs(float(py) - float(got)) <= 1e-12 * max(1.0, abs(float(py)), abs(float(got)))
        else:
            same = py == got
        if not same:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
