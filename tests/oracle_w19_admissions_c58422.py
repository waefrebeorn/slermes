#!/usr/bin/env python3
"""Differential oracle for persistence/authority/admissions.py (gate w19_admissions_c58422).

Extracts the live enums, the GateRefusal dataclass and the pure helpers via ast.parse and execs
them into a globals dict where the contract error family is a stub that records its fields.
"""
import ast
import dataclasses
import json
import sys
from collections.abc import Mapping
from enum import StrEnum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "persistence" / "authority" / "admissions.py"
VOCAB = Q / "persistence" / "authority" / "vocabulary.py"
GATE = "w19_admissions_c58422"


class AuthorityRefusal(Exception):
    """Stub of the contract refusal: the fields the ported verbs raise with."""

    def __init__(self, reason, field=None, detail=None):
        super().__init__(reason)
        self.reason = reason
        self.field = field
        self.detail = detail


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_admissions_live", "__builtins__": __builtins__,
         "dataclass": dataclasses.dataclass, "StrEnum": StrEnum, "Mapping": Mapping,
         "AuthorityRefusal": AuthorityRefusal, "STEERING": _steering(), "Any": object}
    wanted_classes = {"AdmissionIntent", "AdmissionStatus", "GateRefusal"}
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in wanted_classes:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.FunctionDef) and node.name in ("raise_gate", "steer_replayed"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
        elif isinstance(node, ast.ClassDef) and node.name == "AdmissionRecord":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == "__post_init__":
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), str(SRC), "exec"), g)
    return g


def _steering():
    """STEERING from the live vocabulary module (no package import: ast only)."""
    vtree = ast.parse(VOCAB.read_text(encoding="utf-8"))
    for node in vtree.body:
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) == "STEERING" for t in node.targets):
            return ast.literal_eval(node.value)
    return "steering"


G = extract()


class Rec:
    def __init__(self, intent, status):
        self.intent = intent
        self.status = status


def make_gate(kind, reason, field, detail):
    if kind == 0:
        return None
    refusal = G["GateRefusal"](reason, field, detail)
    if kind == 1:
        return refusal
    if kind == 2:
        return lambda: None
    return lambda: refusal


def run_case(r):
    f = r.get("func")
    if f == "raise_gate":
        gate = make_gate(int(r.get("gate_kind") or 0), r.get("reason"), r.get("field"),
                         r.get("detail"))
        try:
            G["raise_gate"](gate)
        except AuthorityRefusal as exc:
            return {"raised": True, "reason": exc.reason, "field": exc.field, "detail": exc.detail}
        return {"raised": False, "reason": None, "field": None, "detail": None}
    if f == "refusal":
        built = G["GateRefusal"](r.get("reason"), r.get("field"), r.get("detail")).refusal()
        return {"reason": built.reason, "field": built.field, "detail": built.detail}
    if f == "steer_replayed":
        return bool(G["steer_replayed"]({"status": r.get("status"), "outcome": r.get("outcome")}))
    if f == "coerce_record":
        rec = Rec(r.get("intent"), r.get("status"))
        try:
            G["__post_init__"](rec)
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        return {"ok": True, "intent": str(rec.intent), "status": str(rec.status)}
    raise KeyError(f)


FUNCS = {"raise_gate", "refusal", "steer_replayed", "coerce_record"}


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
