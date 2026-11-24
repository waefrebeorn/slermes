#!/usr/bin/env python3
"""Differential oracle for persistence/authority/subagents.py (gate w19_subagents_8f6344).

Extracts the live module-level helper and the SubagentRecord.owed property from the
upstream AST (annotations are strings thanks to `from __future__ import annotations`,
so the dataclass builds with stub-filled globals) and compares each fixture case.
"""
import ast
import json
import sys
import types
from dataclasses import dataclass
from enum import StrEnum

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/persistence/authority/subagents.py"
SESS = Q + "/persistence/conversation/sessions.py"
GATE = "w19_subagents_8f6344"
_ANN = __import__("__future__").annotations.compiler_flag


def _mk_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def _exec_node(node, g, path):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, path, "exec", flags=_ANN, dont_inherit=True), g)


# == the automatic-end vocabulary, straight out of sessions.py ================
stree = ast.parse(open(SESS, encoding="utf-8").read())
sg = {"frozenset": frozenset, "__name__": "sessions_live"}
for node in stree.body:
    if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) \
            and node.target.id in ("RECOVERABLE_END_REASONS", "AUTOMATIC_END_REASONS"):
        _exec_node(node, sg, SESS)
    elif isinstance(node, ast.Assign) and any(
            getattr(t, "id", "") in ("RECOVERABLE_END_REASONS", "AUTOMATIC_END_REASONS")
            for t in node.targets):
        _exec_node(node, sg, SESS)
    elif isinstance(node, ast.FunctionDef) and node.name == "is_automatic_end_reason":
        _exec_node(node, sg, SESS)

_mk_module("persistence")
_mk_module("persistence.authority")
_mk_module("persistence.conversation")
_mk_module("persistence.conversation.sessions",
           is_automatic_end_reason=sg["is_automatic_end_reason"],
           AUTOMATIC_END_REASONS=sg["AUTOMATIC_END_REASONS"])
_mk_module("persistence.core")
_mk_module("persistence.core.identity", DelegationId=str, JsonValue=object, SessionId=str)

# == the subagents.py helpers under test =====================================
tree = ast.parse(open(SRC, encoding="utf-8").read())
g = {
    "__name__": "subagents_live",
    "__package__": "persistence.authority",
    "dataclass": dataclass, "StrEnum": StrEnum, "Mapping": object, "Sequence": object,
    "Literal": object, "Optional": object, "Protocol": object,
    "DelegationId": str, "JsonValue": object, "SessionId": str, "__all__": [],
}
_live_mod = types.ModuleType("subagents_live")
_live_mod.__package__ = "persistence.authority"
sys.modules["subagents_live"] = _live_mod
g = _live_mod.__dict__
g.update({
    "dataclass": dataclass, "StrEnum": StrEnum, "Mapping": object, "Sequence": object,
    "Literal": object, "Optional": object, "Protocol": object,
    "DelegationId": str, "JsonValue": object, "SessionId": str, "__all__": [],
})
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name == "parent_superseded_note":
        _exec_node(node, g, SRC)
    elif isinstance(node, ast.ClassDef) and node.name in ("SubagentState", "SubagentRecord"):
        _exec_node(node, g, SRC)

ParentNote = g["parent_superseded_note"]
State = g["SubagentState"]
Record = g["SubagentRecord"]
auto_reason = sg["is_automatic_end_reason"]


def run_case(r):
    f = r.get("func")
    if f == "parent_superseded_note":
        return ParentNote(r.get("tip_end_reason"), tip_exists=bool(r.get("tip_exists")))
    if f == "owed":
        rec = Record(subagent_id="sa", state=State.RUNNING, pinned=False, stop_cause=None,
                     owner_pid=None, owner_started_at=None, document=r.get("document") or {},
                     created_at=0.0, updated_at=0.0, expires_at=None)
        return bool(rec.owed)
    if f == "is_automatic_end_reason":
        return bool(auto_reason(r.get("reason")))
    return "SKIP"


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
        if f not in {"parent_superseded_note", "owed", "is_automatic_end_reason"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if isinstance(py, bool) or isinstance(got, bool):
            same = bool(py) == bool(got) and isinstance(py, bool) == isinstance(got, bool)
        else:
            same = py == got
        if not same:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
