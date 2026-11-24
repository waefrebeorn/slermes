#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/bindings.py (gate w19_bindings_fe89c1)."""
import ast
import contextlib
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "sqlite" / "authority" / "bindings.py"
GATE = "w19_bindings_fe89c1"
FUNCS = {"new_receipt", "api_binding", "binding_marks", "handoff_head",
         "legacy_adoption_scan", "lineages"}
METHODS = {"api_binding", "binding_marks", "handoff_head", "legacy_adoption_scan", "list_lineages"}


def record_class(name):
    cls = type(name, (), {})
    def init(self, **kw):
        self.__dict__.update(kw)
    cls.__init__ = init
    cls.__name__ = name
    return cls


@contextlib.contextmanager
def _refused():
    yield None


class LocalStub:
    pass


def load_live():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {
        "__name__": "bind_live",
        "refused": _refused,
        "_local": LocalStub(),
        "_binding_api": LocalStub(),
        "_binding_history": LocalStub(),
        "ApiBinding": record_class("ApiBinding"),
        "BindingMarks": record_class("BindingMarks"),
        "LocalLineage": record_class("LocalLineage"),
        "LegacyAdoptionScan": record_class("LegacyAdoptionScan"),
        "SessionId": str,
        "Optional": object, "Mapping": object,
    }
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_new_receipt":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name == "SQLiteSessionBindingRepository":
            for item in node.body:
                if isinstance(item, ast.FunctionDef) and item.name in METHODS:
                    exec(compile(ast.Module(body=[item], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = load_live()


class Store:
    def holder(self):
        return None


class Repo:
    def __init__(self):
        self._store = Store()


for _m in METHODS:
    setattr(Repo, _m, G[_m])


def to_jsonable(v):
    if isinstance(v, dict):
        return {k: to_jsonable(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [to_jsonable(x) for x in v]
    if isinstance(v, frozenset):
        return sorted(to_jsonable(x) for x in v)
    return v


class Binding:
    def __init__(self, d):
        for k in ("profile_id", "principal_id", "request_id", "session_id", "route", "entry",
                  "policy", "legacy_session_id", "lineage", "created_source"):
            setattr(self, k, d.get(k))
        self.__dict__.update(d)


def run_case(r):
    f = r.get("func")
    if f == "new_receipt":
        return to_jsonable(G["_new_receipt"](Binding(r.get("binding") or {})))
    if f == "api_binding":
        G["_binding_api"].api_binding_receipt = lambda *a, **k: r.get("receipt")
        rec = Repo().api_binding(r.get("session_id"))
        return None if rec is None else to_jsonable(vars(rec))
    if f == "binding_marks":
        G["_local"].binding_marks = lambda *a, **k: (r.get("policy"), r.get("legacy"), r.get("owner"))
        return to_jsonable(vars(Repo().binding_marks(r.get("session_id"))))
    if f == "handoff_head":
        G["_local"].handoff_head = lambda *a, **k: (r.get("owner"), r.get("head"))
        return to_jsonable(list(Repo().handoff_head(r.get("session_id"))))
    if f == "legacy_adoption_scan":
        G["_local"].legacy_adoption_scan = lambda *a, **k: (r.get("ids") or [], r.get("marked") or [])
        rec = Repo().legacy_adoption_scan(None)
        return {"session_ids": list(rec.session_ids), "marked": sorted(rec.marked)}
    if f == "lineages":
        G["_local"].local_lineages = lambda *a, **k: [tuple(p) for p in (r.get("found") or [])]
        rows = Repo().list_lineages()
        return [{"owner": x.owner, "lineage": list(x.lineage)} for x in rows]
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
