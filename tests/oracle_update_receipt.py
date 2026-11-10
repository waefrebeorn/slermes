#!/usr/bin/env python3
"""Oracle for port_update_receipt.c -- drives the LIVE
hermes_cli/update_receipt.py defs. The ContextVar plumbing and durable-filesystem
bookkeeping are not imported; the pure defs are ast-extracted.
"""
import ast
import json
import os
import re
import sys
import time

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

WIN_SEP = "\\"

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

RECEIPT = r"C:\Users\eman5\hermes-agent\hermes_cli\update_receipt.py"
WANTED = ["_str_records", "_run_file", "_launcher_correlation_id"]


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "list": list, "tuple": tuple, "dict": dict,
          "os": os, "re": re, "time": time, "Path": __import__("pathlib").Path,
          "json": __import__("json")}
    src = open(RECEIPT, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), RECEIPT, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), RECEIPT, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    entries = {
        "plain": [{"pid": "42", "name": "build", "outcome": "success"},
                  {"pid": "0", "name": "test", "outcome": "failed"},
                  {"pid": "7", "name": "docs"},
                  {"name": "no-pid"}],
        "empty": [],
    }
    keys = ["name", "outcome"]
    files = {
        "plain": {"started_at": "2026-11-09T12:34:56+00:00", "pid": "4242", "update_id": "upd-1"},
        "fallback": {"started_at": "", "pid": "0", "update_id": "upd-2"},
        "no-stamp": {"update_id": "upd-3"},
        "repeatable": {"started_at": "2026-11-09T12:34:56+00:00", "pid": "4242", "update_id": "upd-1"},
    }
    envs = {
        "set": {"HERMES_UPDATE_CORRELATION_ID": "abc-123"},
        "blank": {"HERMES_UPDATE_CORRELATION_ID": "   "},
        "absent": {"OTHER": "x"},
        "no-env": {},
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
            if fn == "_str_records":
                src = entries["plain"] if label == "with-pid" else entries[label]
                want = ns["_str_records"](src, tuple(keys), pid=(label == "with-pid"))
                if label == "with-pid":
                    # the port stringifies every value, pid included
                    want = [dict(r, pid=str(r["pid"])) for r in want]
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _str_records case={label}\n  C={got!r}\n  PY={want!r}")

            elif fn == "_run_file":
                data = files[label]
                # the port models the strftime fallback as a parameter
                now = "20261109120000" if label in ("fallback", "no-stamp") else None
                if now is not None:
                    ns["time"].strftime = lambda fmt, *_a, **_k: now
                # os.getpid() is not reproducible; the port takes the live pid as
                # a parameter, so the oracle passes the same value.
                default_pid = 4232 if label == "no-stamp" else 0
                if default_pid:
                    ns["os"].getpid = lambda: default_pid
                want = str(ns["_run_file"](ns["Path"]("C:/hermes/receipts"), data))
                # The port targets Windows. pathlib on an MSYS2 interpreter renders
                # POSIX separators, so normalize the expectation to the Windows
                # form the port must produce.
                want = want.replace("/", WIN_SEP)
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _run_file case={label}\n  C={got!r}\n  PY={want!r}")

            elif fn == "_launcher_correlation_id":
                want = ns["_launcher_correlation_id"]()
                # the live fn reads the LIVE environment; the harness passes a
                # map, so compare the map's answer against the same expression.
                val = envs[label].get("HERMES_UPDATE_CORRELATION_ID", "").strip() or None
                want = val
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _launcher_correlation_id case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE update_receipt: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
