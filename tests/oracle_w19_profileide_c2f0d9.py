#!/usr/bin/env python3
"""Differential oracle for persistence/housekeeping/profile_identity.py (gate w19_profileide_c2f0d9)."""
import ast, dataclasses, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_profileide_c2f0d9"
SRC = Q / "persistence" / "housekeeping" / "profile_identity.py"
_ANN = __future__.annotations.compiler_flag

from persistence.core.errors import Conflict  # the contract base ProfileIdentityCollision extends


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("profile_identity_live")
    _mod.__path__ = []
    sys.modules["profile_identity_live"] = _mod
    g = _mod.__dict__
    g.update({"__name__": "profile_identity_live",
              "dataclass": dataclasses.dataclass, "fields": dataclasses.fields,
              "Optional": object, "Protocol": object, "runtime_checkable": lambda c: c,
              "Conflict": Conflict})
    wanted_fn = {"profile_namespace", "_present"}
    wanted_cls = {"ProfileIdentityCollision", "ProfileRekeyCounts", "ProfilePurgeCounts",
                  "ProfileIdentityRepository"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted_fn:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted_cls:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "profile_namespace":
        return G["profile_namespace"](r.get("profile"))
    if f == "rekey_as_dict":
        return G["ProfileRekeyCounts"](**(r.get("counts") or {})).as_dict()
    if f == "purge_as_dict":
        return G["ProfilePurgeCounts"](**(r.get("counts") or {})).as_dict()
    if f == "collision":
        exc = G["ProfileIdentityCollision"](r.get("new_name") or "", r.get("message") or "")
        return {"str": str(exc), "entity": exc.entity, "entity_id": exc.entity_id}
    return None


def main():
    cases = mism = 0
    known = {"profile_namespace", "rekey_as_dict", "purge_as_dict", "collision"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
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
