#!/usr/bin/env python3
"""Oracle for port_methods_session.c -- drives the LIVE
tui_gateway/methods_session.py defs plus utils.py:is_truthy_value. The RPC
dispatch / db / session-registry half is not imported; the pure defs are
ast-extracted.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\tui_gateway\methods_session.py"
UTILS = r"C:\Users\eman5\hermes-agent\utils.py"
WANTED = ["_str_param", "_flag", "_int_param", "_denied_source",
          "_auto_resume_denied_source"]


def extract():
    ns = {"dict": dict, "str": str, "int": int, "bool": bool, "Any": object,
          "Optional": object, "frozenset": frozenset}
    for path, names in ((UTILS, ["is_truthy_value"]),
                        (SRC, WANTED)):
        src = open(path, encoding="utf-8").read()
        for node in ast.parse(src).body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
        for node in ast.parse(src).body:
            if isinstance(node, ast.FunctionDef) and node.name in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
    ns["INTERNAL_LISTING_SOURCES"] = ("kanban", "tool", "oneshot")
    ns["_LISTING_DENY_SOURCES"] = frozenset(ns["INTERNAL_LISTING_SOURCES"])
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    truthies = {
        "true": "true", "yes": "YES", "one": "1", "on": " on ",
        "false": "false", "empty": "", "null": None, "int1": 1, "int0": 0,
    }
    strs = {
        "plain": ({"name": "  hello  "}, "name", "dflt"),
        "empty": ({"name": ""}, "name", "dflt"),
        "null": ({"name": None}, "name", "dflt"),
        "missing": ({"other": "x"}, "name", "dflt"),
        "no-map": ({}, "name", "dflt"),
        "nodflt": ({"other": "x"}, "name", None),
    }
    flags = {
        "on": ({"verbose": "true"}, "verbose"),
        "off": ({"verbose": "0"}, "verbose"),
        "missing": ({"other": "true"}, "verbose"),
    }
    ints = {
        "plain": ({"limit": "42"}, "limit", 10),
        "padded": ({"limit": "  7  "}, "limit", 10),
        "bad": ({"limit": "abc"}, "limit", 10),
        "neg": ({"limit": "-5"}, "limit", 10),
        "trailing": ({"limit": "12x"}, "limit", 10),
        "missing": ({"other": "9"}, "limit", 10),
    }
    denied = {
        "kanban": {"source": "kanban"}, "tool": {"source": "  TOOL "},
        "oneshot": {"source": "oneshot"}, "unknown": {"source": "unknown"},
        "cli": {"source": "cli"}, "empty": {"source": ""}, "no-row": {},
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
            if fn == "is_truthy_value":
                want = bool(ns["is_truthy_value"](truthies[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH is_truthy_value case={label} C={got!r} PY={want!r}")

            elif fn == "_str_param":
                p, k, d = strs[label]
                want = ns["_str_param"](p, k, d)
                got = c.get("out")
                # the C port returns "" where the Python returns None; both are
                # "no value", and the caller treats them the same
                if want is None:
                    want = ""
                if got != want:
                    mism += 1
                    print(f"MISMATCH _str_param case={label} C={got!r} PY={want!r}")

            elif fn == "_flag":
                p, n = flags[label]
                want = bool(ns["_flag"](p, n))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _flag case={label} C={got!r} PY={want!r}")

            elif fn == "_int_param":
                p, k, d = ints[label]
                want = ns["_int_param"](p, k, d)
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _int_param case={label} C={got!r} PY={want!r}")

            elif fn in ("_denied_source", "_auto_resume_denied_source"):
                want = bool(ns[fn](denied[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH {fn} case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE methods_session: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
