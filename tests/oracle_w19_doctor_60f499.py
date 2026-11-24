#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/admin/_doctor.py (gate w19_doctor_60f499).

The live AST helpers are exec'd into stub-filled globals: `_header_mode` runs against a
real temporary file holding the fixture's header bytes (the only true seam is the byte
read), `_newest_mtime_ns` gets an identity `_watcher_mtime_ns` so it is the pure max over
already-stat'ed values, and `_refusal` is pure.
"""
import ast
import json
import os
import sys
import tempfile
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/persistence/sqlite/admin/_doctor.py"
GATE = "w19_doctor_60f499"
_ANN = __import__("__future__").annotations.compiler_flag

WANTED = {"_header_mode", "_refusal", "_newest_mtime_ns"}
WANTED_CONSTS = {"_HEADER_VERSION_BYTES", "_MODE_BY_HEADER_VERSION"}


def _exec_node(node, g):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), g)


tree = ast.parse(open(SRC, encoding="utf-8").read())
g = {"__name__": "doctor_live", "__package__": "persistence.sqlite.admin"}
for node in tree.body:
    if isinstance(node, ast.Assign) and any(
            getattr(t, "id", "") in WANTED_CONSTS for t in node.targets):
        _exec_node(node, g)
    elif isinstance(node, ast.AnnAssign) and getattr(node.target, "id", "") in WANTED_CONSTS:
        _exec_node(node, g)
    elif isinstance(node, ast.FunctionDef) and node.name in WANTED:
        _exec_node(node, g)

header_mode = g["_header_mode"]
refusal = g["_refusal"]
newest_mtime_ns = g["_newest_mtime_ns"]
# Pure-max seam: the values already are st_mtime_ns results, None for a failed stat.
g["_watcher_mtime_ns"] = lambda value: value


def run_case(r, tmpdir):
    f = r.get("func")
    if f == "header_mode":
        hex_text = r.get("header_hex") or ""
        try:
            raw = bytes.fromhex(hex_text)
        except ValueError:
            raw = b""
        path = os.path.join(tmpdir, "state.db")
        with open(path, "wb") as fh:
            fh.write(raw)
        return header_mode(__import__("pathlib").Path(path))
    if f == "refusal":
        return refusal(r.get("target", "wal"), r.get("current"),
                       on_cross_vm_fs=bool(r.get("on_cross_vm_fs")))
    if f == "newest_mtime_ns":
        vals = []
        for v in r.get("values") or []:
            vals.append(None if v is None else int(v))
        out = newest_mtime_ns(vals)
        return None if out is None else str(out)
    return "SKIP"


def main():
    cases = mism = 0
    with tempfile.TemporaryDirectory() as tmpdir:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            f = r.get("func")
            if f not in {"header_mode", "refusal", "newest_mtime_ns"}:
                continue
            cases += 1
            try:
                py = run_case(r, tmpdir)
            except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
                mism += 1
                print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
                continue
            got = r.get("out")
            if py != got:
                mism += 1
                print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
