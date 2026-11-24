#!/usr/bin/env python3
"""Differential oracle for hermes_cli/local_runtime/processes.py (gate w19_processes_bd7796).

Extracts the live FunctionDef via ast.parse and execs it into a stub-filled globals dict,
then compares the C harness `out` against it for every fixture case.
"""
import ast
import importlib
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = Path("C:/Users/eman5/hermes-agent")
SRC = Q / "hermes_cli" / "local_runtime" / "processes.py"
GATE = "w19_processes_bd7796"
WANTED = {"server_child_env"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "w19_processes_live", "__builtins__": __builtins__}
    # Module-level literals the function reads (the marker tuple), straight from source.
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                getattr(t, "id", None) == "_CREDENTIAL_ENV_MARKERS" for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "server_child_env":
        return G["server_child_env"](r.get("base_env") or {})
    raise KeyError(f)


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
        if f not in WANTED:
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
