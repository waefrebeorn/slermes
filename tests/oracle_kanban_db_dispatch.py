#!/usr/bin/env python3
"""Oracle for port_kanban_db_dispatch.c -- drives the LIVE
hermes_cli/kanban_db_dispatch.py defs. The sqlite / process / signal half is not
imported; the pure defs are ast-extracted.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\kanban_db_dispatch.py"
WANTED = ["_wait_status_from_returncode", "_exit_code_kind",
          "_error_fingerprint", "_worker_survived_termination"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "Optional": object, "Any": object, "dict": dict,
          "bool": bool, "str": str, "int": int, "Iterable": object}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # hermes_cli/kanban_db.py: the two special worker exit codes
    ns["_kb"] = type("KB", (), {"KANBAN_RATE_LIMIT_EXIT_CODE": 75,
                                "KANBAN_TERMINAL_PROVIDER_EXIT_CODE": 78})()
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    waits = {"zero": 0, "one": 1, "127": 127, "255": 255, "256": 256,
             "neg15": -15, "neg1": -1}
    kinds = {"clean": (0, 75, 78), "rate": (75, 75, 78), "terminal": (78, 75, 78),
             "other": (7, 75, 78), "zero-is-not-rate": (0, 0, 0)}
    fps = {
        "pid": "worker pid 12345 died", "pid-long": "pid 987654321 exited",
        "pid-word": "rapid 12345 done", "ts": "failed at 1730000000 after",
        "ts-short": "failed at 1234567 after", "both": "pid 42 at 1730000000",
        "upper": "Worker PID 9 DIED",
        "long-text": "a" * 90 + "pid 7", "empty": "", "padded": "  Something Failed  ",
    }
    survived = {
        "attempted": {"host_local": True, "termination_attempted": True, "terminated": False},
        "refused": {"host_local": True, "signal_refused": True, "terminated": False},
        "terminated": {"host_local": True, "termination_attempted": True, "terminated": True},
        "no-attempt": {"host_local": True, "termination_attempted": False, "terminated": False},
        "not-local": {"host_local": False, "termination_attempted": True, "terminated": False},
        "empty": {},
    }
    kind_names = ["clean_exit", "rate_limited", "terminal_provider", "nonzero_exit"]

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
            if fn == "_wait_status_from_returncode":
                want = ns[fn](waits[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _wait_status_from_returncode case={label} C={got!r} PY={want!r}")

            elif fn == "_exit_code_kind":
                code, rl, tp = kinds[label]
                want = ns[fn](code)
                got_kind = c.get("out")
                got_code = c.get("code")
                if got_kind != kind_names.index(want[0]) or got_code != want[1]:
                    mism += 1
                    print(f"MISMATCH _exit_code_kind case={label} "
                          f"C=(kind={got_kind!r},code={got_code!r}) PY={want!r}")

            elif fn == "_error_fingerprint":
                want = ns[fn](fps[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _error_fingerprint case={label} C={got!r} PY={want!r}")

            elif fn == "_worker_survived_termination":
                want = bool(ns[fn](survived[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _worker_survived_termination case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE kanban_db_dispatch: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
