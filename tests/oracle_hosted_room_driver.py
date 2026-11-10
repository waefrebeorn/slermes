#!/usr/bin/env python3
"""Oracle for port_hosted_room_driver.c -- drives the LIVE
gateway/hosted_room_driver.py defs. The SQLite schema / transaction /
row-mapping half is not imported; the pure defs are ast-extracted.

DriverValidationError has no C equivalent, so the port reports failure through
an `ok` flag and an `err` string; the oracle compares the branch taken.
"""
import ast
import json
import math
import sqlite3
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

DRIVER = r"C:\Users\eman5\hermes-agent\gateway\hosted_room_driver.py"
WANTED = ["_task_update", "_generation_update", "_finite", "_timestamp",
          "_lease_window"]


class DriverValidationError(Exception):
    pass


class DriverStateError(Exception):
    pass


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "Callable": object, "frozenset": frozenset,
          "list": list, "tuple": tuple, "dict": dict, "math": math,
          "sqlite3": sqlite3, "json": __import__("json"),
          "DriverValidationError": DriverValidationError,
          "DriverStateError": DriverStateError}
    src = open(DRIVER, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), DRIVER, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.ClassDef) and node.name in ("DriverValidationError",
                                                            "DriverStateError"):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), DRIVER, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), DRIVER, "exec"), ns)
    return ns


def call_finite(ns, candidate, positive=False):
    """Return (ok, value_or_err) for the live _finite, with the REAL message."""
    msg = "ttl_seconds must be a finite positive number" if positive else "msg"
    try:
        return True, ns["_finite"](lambda: candidate, msg, positive=positive)
    except ns["DriverValidationError"] as e:
        return False, str(e)


def call_timestamp(ns, value, callable_=True):
    try:
        return True, ns["_timestamp"]((lambda: value) if callable_ else "nope")
    except ns["DriverValidationError"] as e:
        return False, str(e)


def call_window(ns, ttl, clock, callable_=True):
    try:
        return True, ns["_lease_window"](ttl, (lambda: clock) if callable_ else "nope")
    except ns["DriverValidationError"] as e:
        return False, str(e)


def main():
    ns = extract()
    cases = 0
    mism = 0

    updates = {"plain": ("status='queued'", "status='running'"), "empty": ("", "")}
    generations = {"running": "running", "stopping": "stopping"}
    finites = {
        "plain": ("1.5", 0, "msg"), "negative": ("-2", 0, "msg"),
        "positive": ("-2", 1, "ttl_seconds must be a finite positive number"),
        "nan": ("nan", 0, "msg"), "inf": ("inf", 0, "msg"),
        "junk": ("abc", 0, "msg"),
    }
    stamps = {
        "ok": ("1234.5", 1, None),
        "not-callable": ("1234.5", 0, "clock must be callable"),
        "not-finite": ("nan", 1, "clock must return a finite number"),
    }
    windows = {
        "ok": ("30", "1000", 1, None),
        "zero-ttl": ("0", "1000", 1, "ttl_seconds must be a finite positive number"),
        "neg-ttl": ("-1", "1000", 1, "ttl_seconds must be a finite positive number"),
        "no-clock": ("30", "1000", 0, "clock must be callable"),
        "nan-clock": ("30", "nan", 1, "clock must return a finite number"),
    }

    def cmp_ok(fn, label, out, want_ok, want_val, mism):
        got_ok = "ok" in out
        if got_ok != want_ok:
            return False, f"C_ok={got_ok!r} PY_ok={want_ok!r}"
        if want_ok:
            if isinstance(want_val, tuple):
                gn, ge = out["ok"]
                if gn != want_val[0] or ge != want_val[1]:
                    return False, f"C=({gn!r},{ge!r}) PY=({want_val[0]!r},{want_val[1]!r})"
            elif out.get("ok") != want_val:
                return False, f"C={out.get('ok')!r} PY={want_val!r}"
        else:
            if (out.get("err") or "") != (want_val or ""):
                return False, f"C_err={out.get('err')!r} PY_err={want_val!r}"
        return True, ""

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
        out = c.get("out") or {}

        try:
            if fn == "_task_update":
                sc, fence = updates[label]
                want = ns["_task_update"](sc, fence)
                if c.get("out") != want:
                    mism += 1
                    print(f"MISMATCH _task_update case={label} C={c.get('out')!r} PY={want!r}")

            elif fn == "_generation_update":
                st = generations[label]
                sc = "status=?" if label == "stopping" else "status=?, updated_at=?"
                want = ns["_generation_update"](sc, st)
                if c.get("out") != want:
                    mism += 1
                    print(f"MISMATCH _generation_update case={label} C={c.get('out')!r} PY={want!r}")

            elif fn == "_finite":
                cand, pos, msg = finites[label]
                ok, val = call_finite(ns, cand, positive=bool(pos))
                good, why = cmp_ok(fn, label, out, ok, val, mism)
                if not good:
                    mism += 1
                    print(f"MISMATCH _finite case={label} {why}")

            elif fn == "_timestamp":
                val, callable_, err = stamps[label]
                ok, res = call_timestamp(ns, val, callable_=bool(callable_))
                good, why = cmp_ok(fn, label, out, ok, res if ok else err, mism)
                if not good:
                    mism += 1
                    print(f"MISMATCH _timestamp case={label} {why}")

            elif fn == "_lease_window":
                ttl, clock, callable_, err = windows[label]
                ok, res = call_window(ns, ttl, clock, callable_=bool(callable_))
                good, why = cmp_ok(fn, label, out, ok, res if ok else err, mism)
                if not good:
                    mism += 1
                    print(f"MISMATCH _lease_window case={label} {why}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE hosted_room_driver: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
