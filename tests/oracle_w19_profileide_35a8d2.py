#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/housekeeping/profile_identity.py (w19_profileide_35a8d2)."""
import ast, copy, json, sys, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "housekeeping" / "profile_identity.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_profileide_35a8d2"
WANTED = {"_moved_key", "_rekey_entry"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "profile_identity_live", "__builtins__": __builtins__,
         "Optional": object, "ProfileRekeyCounts": lambda *a, **k: "SKIP",
         "ProfilePurgeCounts": lambda *a, **k: "SKIP"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    # The rekey_profile guard runs as the live statements do: strip both names, refuse empty/equal.
    guard_fn = next(n for n in ast.walk(tree)
                    if isinstance(n, ast.FunctionDef) and n.name == "rekey_profile")
    guard = ast.FunctionDef(
        name="_rekey_guard",
        args=ast.arguments(posonlyargs=[], args=[ast.arg(arg="old_name"), ast.arg(arg="new_name")],
                           vararg=None, kwonlyargs=[], kw_defaults=[], kwarg=None, defaults=[]),
        body=[guard_fn.body[0], guard_fn.body[2]],
        decorator_list=[], returns=None, type_comment=None)
    exec(compile(ast.fix_missing_locations(ast.Module(body=[guard], type_ignores=[])), str(SRC), "exec",
                 flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "moved_key":
        return G["_moved_key"](r.get("key"), r.get("old"), r.get("new"))
    if f == "rekey_entry":
        entry = copy.deepcopy(r.get("entry"))
        changed = G["_rekey_entry"](entry, r.get("old"), r.get("new"))
        return {"changed": bool(changed), "entry": entry}
    if f == "rekey_allowed":
        return G["_rekey_guard"](r.get("old"), r.get("new")) is None
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
        if f not in {"moved_key", "rekey_entry", "rekey_allowed"}:
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
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or not cases) else 0)


if __name__ == "__main__":
    main()
