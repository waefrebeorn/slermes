#!/usr/bin/env python3
"""Oracle for port_gateway_status.c -- drives the LIVE gateway/status.py defs.
The runtime-status writer / process liveness / PID probing half is not imported;
the pure defs are ast-extracted.
"""
import ast
import hashlib
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# The port targets Windows; pin platform values rather than inheriting them from
# whichever interpreter runs the oracle (MSYS2 python reports os.sep == '/').
WIN_SEP = "\\"

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

STATUS = r"C:\Users\eman5\hermes-agent\gateway\status.py"
WANTED = ["_scope_hash", "_start_times_agree", "start_time_fingerprints_match",
          "scoped_lock_owner_label", "_canonical_hermes_home", "_same_hermes_home",
          "retained_gateway_state", "_profile_label_for_home"]


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "frozenset": frozenset, "hashlib": hashlib,
          "re": re, "Path": __import__("pathlib").Path, "list": list, "tuple": tuple, "dict": dict,
          "os": __import__("os"), "contextlib": __import__("contextlib")}
    src = open(STATUS, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), STATUS, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), STATUS, "exec"), ns)
    # _profile_label_for_home calls _get_platform_default_hermes_home (imported
    # from hermes_constants at module level) - seed it so the "root home" case
    # can resolve.
    ns["_get_platform_default_hermes_home"] = lambda: "C:/hermes"
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    hashes = {"plain": "profile:work", "empty": "", "null": None}
    agree = {
        "agree": ("1700000000.000", ["1700000000.001", "1700000000.002"]),
        "drift": ("1700000000.000", ["1700000000.001", "1700000000.500"]),
        "zero": ("1700000000.000", ["0"]),
        "cur-zero": ("0", ["1700000000.001"]),
        "none": ("1700000000.000", []),
    }
    matches = {
        "same": ("170000000000", "170000000000", 200),
        "drift": ("170000000000", "170000000100", 200),
        "far": ("170000000000", "170000001000", 200),
        "tight": ("170000000000", "170000000005", 4),
    }
    labels = {
        "profile": {"profile": "  work  "},
        "from-home": {"profile": "BAD LABEL", "hermes_home": "C:/hermes/profiles/work"},
        "home-only": {"hermes_home": "C:/hermes/profiles/work"},
        "root-home": {"hermes_home": "C:/hermes"},
        "no-record": {},
    }
    homes = {
        "same": ("C:/Hermes", "c:\\hermes"),
        "diff": ("C:/Hermes", "C:/Other"),
    }
    retained = {
        "failed": {"desired_state": "running", "gateway_state": "startup_failed"},
        "degraded": {"desired_state": "running", "gateway_state": "degraded",
                     "exit_reason": "loop_liveness_watchdog"},
        "stopped": {"desired_state": "stopped", "gateway_state": "startup_failed"},
        "degraded-other": {"desired_state": "running", "gateway_state": "degraded",
                           "exit_reason": "other"},
        "running": {"desired_state": "running", "gateway_state": "running"},
        "no-runtime": {},
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
            if fn == "_scope_hash":
                ident = hashes[label]
                # None raises AttributeError in Python; the C port hashes ""
                # (a defensive extension for a C caller).
                want = hashlib.sha256((ident or "").encode("utf-8")).hexdigest()[:16]
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _scope_hash case={label} C={got!r} PY={want!r}")

            elif fn == "_start_times_agree":
                cur, rec = agree[label]
                want = ns["_start_times_agree"](cur, *rec)
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _start_times_agree case={label} C={got!r} PY={want!r}")

            elif fn == "start_time_fingerprints_match":
                rec, cur, tol = matches[label]
                want = ns["start_time_fingerprints_match"](rec, cur, tol)
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH start_time_fingerprints_match case={label} C={got!r} PY={want!r}")

            elif fn == "scoped_lock_owner_label":
                want = ns["scoped_lock_owner_label"](labels[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH scoped_lock_owner_label case={label} C={got!r} PY={want!r}")

            elif fn == "_same_hermes_home":
                left, right = homes[label]
                # The live fn resolves through the filesystem; the port models the
                # resolve answer as a parameter, so compare the un-resolved form.
                want = ns["_same_hermes_home"](left, right)
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _same_hermes_home case={label} C={got!r} PY={want!r}")

            elif fn == "retained_gateway_state":
                want = ns["retained_gateway_state"](retained[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH retained_gateway_state case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE gateway_status: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
