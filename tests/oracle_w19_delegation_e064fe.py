#!/usr/bin/env python3
"""Differential oracle for persistence/workers/delegations.py (gate w19_delegation_e064fe).

Extracts the live module-level function `delegation_payload` from the Python AST and runs
it against stub-filled globals (the dataclasses are types; their shape is the fixture's
`kind` tag). Compares each fixture case's `out` (produced by the C harness) with the live
result. Prints `ORACLE <gate>: N cases, M mismatches` and exits non-zero on a mismatch.
"""
import ast
import json
import sys
import __future__
from dataclasses import dataclass, fields, is_dataclass
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "persistence" / "workers" / "delegations.py"
GATE = "w19_delegation_e064fe"
_ANN = __future__.annotations.compiler_flag


# -- stand-ins for the module's declared types (names/shapes from the live source) --
@dataclass(frozen=True)
class WorkerDelegationDispatch:
    delegation_id: object
    task: object
    dispatched_at: object
    session_key: object
    parent_session_id: object
    origin_ui_session_id: object
    origin_session_id: object


@dataclass(frozen=True)
class WorkerDelegationCompletion:
    event: object
    result: object
    delivery: object = "pending"


@dataclass(frozen=True)
class WorkerDelegationChild:
    delegation_id: object
    entry: object


@dataclass(frozen=True)
class WorkerDelegationClaim:
    delegation_id: object
    claim_id: object


@dataclass(frozen=True)
class WorkerDelegationPayload:
    payload: object


WANTED = {"delegation_payload"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {
        "__name__": "deleg_live",
        "dataclass": dataclass,
        "fields": fields,
        "is_dataclass": is_dataclass,
        "getattr": getattr,
        "isinstance": isinstance,
        "WorkerDelegationPayload": WorkerDelegationPayload,
        "WorkerDelegationCompletion": WorkerDelegationCompletion,
        "WorkerDelegationDispatch": WorkerDelegationDispatch,
        "WorkerDelegationChild": WorkerDelegationChild,
        "WorkerDelegationClaim": WorkerDelegationClaim,
        "JsonValue": object,
        "Any": object,
    }
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def _arg(kind, value):
    v = value if isinstance(value, dict) else {}
    if kind == "payload":
        return WorkerDelegationPayload(payload=value)
    if kind == "completion":
        return WorkerDelegationCompletion(event=v.get("event"), result=v.get("result"),
                                          delivery=v.get("delivery", "pending"))
    if kind == "dispatch":
        return WorkerDelegationDispatch(**{f: v.get(f) for f in (
            "delegation_id", "task", "dispatched_at", "session_key",
            "parent_session_id", "origin_ui_session_id", "origin_session_id")})
    if kind == "child":
        return WorkerDelegationChild(delegation_id=v.get("delegation_id"), entry=v.get("entry"))
    if kind == "claim":
        return WorkerDelegationClaim(delegation_id=v.get("delegation_id"), claim_id=v.get("claim_id"))
    if kind == "record" and isinstance(value, dict):
        return WorkerDelegationClaim(delegation_id=value.get("delegation_id"),
                                     claim_id=value.get("claim_id"))
    return value


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
        if r.get("func") != "delegation_payload":
            continue
        cases += 1
        try:
            py = G["delegation_payload"](_arg(r.get("kind"), r.get("value")))
        except BaseException as exc:  # noqa: BLE001 - a live raise is a fixture bug, not a pass
            mism += 1
            print("MISMATCH delegation_payload args=%r py=raised %r c=%r" % (r, exc, r.get("out")),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH delegation_payload args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
