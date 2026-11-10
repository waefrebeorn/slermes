#!/usr/bin/env python3
"""Oracle for port_state_repair.c -- drives the LIVE hermes_state_repair.py defs.
The SQLite surgery / locking / ledger half is not imported; the pure defs are
ast-extracted.
"""
import ast
import json
import os
import sys
import sqlite3

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

REPAIR = r"C:\Users\eman5\hermes-agent\hermes_state_repair.py"
WANTED = ["_mask_volatile_header", "_repair_backup_headroom_bytes",
          "_repair_failure_consumes_attempt", "_sidecars"]


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "frozenset": frozenset, "list": list,
          "tuple": tuple, "dict": dict, "os": os, "sqlite3": sqlite3,
          "Path": __import__("pathlib").Path, "bytes": bytes,
          "bytearray": bytearray, "len": len}
    src = open(REPAIR, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), REPAIR, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), REPAIR, "exec"), ns)
    return ns


class FakeDBError(sqlite3.DatabaseError):
    """A sqlite3.DatabaseError carrying sqlite_errorcode."""
    def __init__(self, message, code=None):
        super().__init__(message)
        if code is not None:
            self.sqlite_errorcode = code


def main():
    ns = extract()
    cases = 0
    mism = 0

    head_cases = {"full": 100, "short": 40}
    head = bytes([0xAB]) * 100
    head_out = {}
    for label, n in head_cases.items():
        # the function RETURNS a new bytes object; it does not mutate the input
        head_out[label] = ns["_mask_volatile_header"](bytes(head[:n])).hex()

    volumes = {
        "small": 1024,
        "exact": 128 * 1024 * 1024,
        "large": 100 * 1024 * 1024 * 1024,
        "zero": 0,
    }
    failures = {
        "corrupt": (1, 11, ""),
        "notadb": (1, 16, ""),
        "extended": (1, 267, ""),          # 0x10B: 256 | SQLITE_CORRUPT
        "busy": (1, 5, ""),
        "not-db-error": (0, 11, ""),
        "msg-corrupt": (1, 0, "file is not a database"),
        "msg-malformed": (1, 0, "database disk image is malformed"),
        "msg-full": (1, 0, "disk is full"),
        "msg-readonly": (1, 0, "attempt to write a readonly database"),
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_mask_volatile_header":
                want = head_out[label]
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _mask_volatile_header case={label} C={got!r} PY={want!r}")

            elif fn == "_repair_backup_headroom_bytes":
                want = ns["_repair_backup_headroom_bytes"](volumes[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _repair_backup_headroom_bytes case={label} C={got!r} PY={want!r}")

            elif fn == "_repair_failure_consumes_attempt":
                is_db, code, msg = failures[label]
                if is_db:
                    exc = FakeDBError(msg, code if code else None)
                else:
                    exc = RuntimeError(msg)
                want = bool(ns["_repair_failure_consumes_attempt"](exc))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _repair_failure_consumes_attempt case={label} C={got!r} PY={want!r}")

            elif fn == "_sidecars":
                want = [p.name for p in ns["_sidecars"](ns["Path"]("state.db"))]
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _sidecars case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE state_repair: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
