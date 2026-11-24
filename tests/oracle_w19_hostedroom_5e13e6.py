#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/gateway/hosted_room_peers.py
(gate w19_hostedroom_5e13e6).

The live module is a SQLite repository whose import pulls the whole persistence
stack, so the oracle execs its module-level pure helpers (and the receipt-key
column tuple they read) out of the source AST against stub globals.
"""
import ast, json, sqlite3, sys, types, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
GATE = "w19_hostedroom_5e13e6"
SRC = Path("C:/Users/eman5/hermes-agent/persistence/sqlite/gateway/hosted_room_peers.py")
_ANN = __future__.annotations.compiler_flag
WANT_FN = {"_reservation_superseded", "_reservation_values", "_remote_run_identity"}
WANT_VAR = {"_REMOTE_RUN_IDENTITY_COLUMNS"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "hrp_live", "sqlite3": sqlite3, "Any": object}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FN:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.Assign):
            names = {getattr(t, "id", None) for t in node.targets}
            if names & WANT_VAR:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


class Row:
    """dict-like row: attribute access for the record path, item access for the row path."""

    def __init__(self, d):
        self.__dict__.update(d)

    def __getitem__(self, key):
        return self.__dict__.get(key)

    def __getattr__(self, key):
        return None


def run_case(r):
    f = r.get("func")
    if f == "reservation_superseded":
        return bool(G["_reservation_superseded"](Row(r.get("row") or {}),
                                                 r.get("gateway_id", ""), int(r.get("epoch", 0))))
    if f == "reservation_values":
        return list(G["_reservation_values"](Row(r.get("claims") or {})))
    if f == "remote_run_identity":
        return list(G["_remote_run_identity"](Row(r.get("record") or {})))
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
        if f not in {"reservation_superseded", "reservation_values", "remote_run_identity"}:
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
