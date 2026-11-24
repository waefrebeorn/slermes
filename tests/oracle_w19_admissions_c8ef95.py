#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/admissions.py (gate w19_admissions_c8ef95)."""
import ast, dataclasses, enum, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "authority" / "admissions.py"
CONTRACT_SRC = REPO.parent / "hermes-agent" / "persistence" / "authority" / "admissions.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_admissions_c8ef95"
WANTED_FN = {"_record", "_claimed"}
WANTED_CLS = {"AdmissionIntent", "AdmissionStatus", "AdmissionRecord", "ClaimedAdmission"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("admissions_live")
    mod.__dict__["__builtins__"] = __builtins__
    sys.modules["admissions_live"] = mod
    g = mod.__dict__
    g.update(dict(dataclass=dataclasses.dataclass, StrEnum=enum.StrEnum, Any=object,
                  Mapping=dict, Sequence=list, field=dataclasses.field))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_FN:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    # the contract dataclasses live in the domain module the repository imports from
    ctree = ast.parse(CONTRACT_SRC.read_text(encoding="utf-8"))
    for node in ctree.body:
        if isinstance(node, ast.ClassDef) and node.name in WANTED_CLS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(CONTRACT_SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    g["_RECORD_FIELDS"] = tuple(f.name for f in dataclasses.fields(g["AdmissionRecord"]))
    return g


G = extract()
FIELDS = G["_RECORD_FIELDS"]


def scalar(v):
    return v.value if isinstance(v, enum.Enum) else v


def dump(rec):
    return {f.name: scalar(getattr(rec, f.name)) for f in dataclasses.fields(type(rec))}


def run_case(r):
    f = r.get("func")
    row = r.get("row")
    for name, fn in (("record", G["_record"]), ("claimed", G["_claimed"])):
        if f != name:
            continue
        try:
            return dump(fn(row)) if fn(row) is not None else None
        except KeyError as e:
            return {"error": "KeyError: %r" % (e.args[0],)}
        except ValueError as e:
            return {"error": "ValueError: %s" % (e,)}
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
        if f not in {"record", "claimed"}:
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
