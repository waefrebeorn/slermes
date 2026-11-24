#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/authority/runtime.py (gate w19_runtime_3ded45).

The four module-level pure helpers are extracted from the live AST.  ``_derive`` is
exercised with the REAL contract-package chain dataclasses (imported from the quarry's
``persistence.authority.runtime``), and the binding derivations it calls are the live
``persistence.authority.bindings`` functions, so the compared ids are the SHA-256
identities the store really uses.  The repository class is a DB wrapper and is not
exercised.
"""
import ast, json, sys, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "authority" / "runtime.py"
GATE = "w19_runtime_3ded45"
_ANN = __future__.annotations.compiler_flag
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import persistence.authority.runtime as CONTRACT            # noqa: E402
import persistence.authority.bindings as BINDINGS           # noqa: E402


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "runtime_live", "Any": object,
         "ApiGenerations": CONTRACT.ApiGenerations,
         "RoomGenerations": CONTRACT.RoomGenerations,
         "LocalGenerations": CONTRACT.LocalGenerations,
         "api_generation_id": BINDINGS.api_generation_id,
         "room_session_id": BINDINGS.room_session_id,
         "local_session_id": BINDINGS.local_session_id,
         "generation_request_id": BINDINGS.generation_request_id}
    wanted = {"_derive", "_mutation_call", "_legacy_prepared", "_chat_params"}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def chain_for(r):
    kind = r.get("kind")
    if kind == "api":
        return CONTRACT.ApiGenerations(r.get("session_id") or "")
    if kind == "room":
        return CONTRACT.RoomGenerations(tuple(r.get("identity") or []))
    return CONTRACT.LocalGenerations(r.get("profile_id") or "", r.get("principal_id") or "",
                                     r.get("base_request_id") or "")


def run_case(r):
    f = r.get("func")
    if f == "derive":
        return str(G["_derive"](chain_for(r))(int(r.get("generation") or 0)))
    if f == "mutation_call":
        payload = r.get("payload")
        cmd = type("Cmd", (), {})()
        cmd.operation = r.get("operation")
        cmd.payload = (lambda: payload) if r.get("payload_callable") else payload
        return G["_mutation_call"](cmd)
    if f == "legacy_prepared":
        if r.get("is_none"):
            return G["_legacy_prepared"](None)
        snap = CONTRACT.LocalSnapshot(receipt=r.get("receipt"), target=r.get("target") or "",
                                      target_revision=int(r.get("target_revision") or 0))
        prepared = CONTRACT.PreparedMutation(snapshot=snap, values=r.get("values") or {})
        return G["_legacy_prepared"](prepared)
    if f == "chat_params":
        params = CONTRACT.ChatPickParams(request_id=r.get("request_id") or "",
                                         operation=r.get("operation") or "",
                                         payload=r.get("payload") or {},
                                         expected_revision=int(r.get("expected_revision") or 0),
                                         expected_generation=int(r.get("expected_generation") or 0))
        return G["_chat_params"](params)
    return "<unknown>"


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
    if f not in {"derive", "mutation_call", "legacy_prepared", "chat_params"}:
        continue
    cases += 1
    try:
        py = run_case(r)
    except BaseException as exc:
        mism += 1
        print("MISMATCH %s oracle raised %r args=%s" % (f, exc, r), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
