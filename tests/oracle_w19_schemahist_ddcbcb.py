#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_schema_history.py (gate w19_schemahist_ddcbcb)."""
import ast, dataclasses, itertools, json, sys
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "admin" / "_schema_history.py"
GATE = "w19_schemahist_ddcbcb"
WANTED = {"_apply", "declared_snapshots", "current_declared_columns",
          "reachable_physical_layouts"}
WANTED_CLASSES = {"_TableHistory"}
WANTED_VALUES = {"SCHEMA_HISTORY", "Edit"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "schema_history_live", "dataclass": dataclasses.dataclass}
    g.update(dict(tuple=tuple, list=list, dict=dict, int=int, str=str, bool=bool,
                  Optional=object, Callable=object, Iterator=object, Sequence=object,
                  Any=object, frozen=True))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in targets if isinstance(t, ast.Name)}
            if names & WANTED_VALUES:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             dont_inherit=True), g)
    return g


G = extract()


def norm(v):
    if isinstance(v, tuple):
        return [norm(x) for x in v]
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def make_accept(spec):
    if spec is None:
        return None
    max_len = spec.get("max_len")
    max_first = spec.get("max_first_new")
    require = spec.get("require") or []

    def accept(layout, first_new):
        if max_len is not None and len(layout) > max_len:
            return False
        if max_first is not None and first_new > max_first:
            return False
        return all(col in layout for col in require)
    return accept


def run_case(r):
    f = r.get("func")
    table = r.get("table", "")
    if f == "apply":
        columns = list(r.get("columns") or [])
        G["_apply"](columns, [tuple(e) for e in (r.get("edits") or [])])
        return columns
    if f == "snapshots":
        return [list(s) for s in G["declared_snapshots"](table)]
    if f == "current":
        return list(G["current_declared_columns"](table))
    if f == "reachable":
        limit = r.get("limit", 1000)
        it = G["reachable_physical_layouts"](table, make_accept(r.get("accept")))
        layouts = []
        truncated = False
        for layout in it:
            if len(layouts) >= limit:
                truncated = True
                break
            layouts.append(list(layout))
        return {"layouts": layouts, "count": len(layouts), "truncated": truncated}
    if f == "history":
        h = G["SCHEMA_HISTORY"][table]
        return {"base": list(h.base), "events": [[lab, [[e[0], e[1], e[2] if len(e) > 2 else None]
                                                        for e in edits]] for lab, edits in h.events]}
    if f == "tables":
        return list(G["SCHEMA_HISTORY"].keys())
    return None


FUNCS = {"apply", "snapshots", "current", "reachable", "history", "tables"}


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
            py = norm(run_case(r))
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
