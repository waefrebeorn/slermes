#!/usr/bin/env python3
"""Differential oracle for tools/connectors/gateway/wire.py (gate w19_wire_cd861d)."""
import ast, json, sys, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
GATE = "w19_wire_cd861d"
SRC = Path("C:/Users/eman5/hermes-agent/tools/connectors/gateway/wire.py")
_ANN = __future__.annotations.compiler_flag


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "wire_live"}
    want_vars = {"EXECUTE_ERROR_CODES", "FILE_ERROR_FAMILIES"}
    want_fn = {"execute_error_family"}
    for node in tree.body:
        name = getattr(getattr(node, "target", None), "id", None) or None
        if isinstance(node, (ast.Assign, ast.AnnAssign)) and name in want_vars:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.FunctionDef) and node.name in want_fn:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    g["CONNECTOR_FILE_MAX_BYTES"] = 20 * 1024 * 1024
    return g


G = extract()


def run_case(r):
    f = r.get("func")
    if f == "execute_error_family":
        return G["execute_error_family"](r.get("code", ""), r.get("family"))
    if f == "execute_error_status":
        return G["EXECUTE_ERROR_CODES"].get(r.get("code", ""), (None, -1))[1]
    if f == "file_max_bytes":
        return G["CONNECTOR_FILE_MAX_BYTES"]
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
        if f not in {"execute_error_family", "execute_error_status", "file_max_bytes"}:
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
