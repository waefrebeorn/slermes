#!/usr/bin/env python3
"""Oracle for port_nous_account.c -- drives the LIVE nous_account.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.nous_account pulls in urllib/dataclasses machinery.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
# the harness emits UTF-8 (the notice carries an em dash); without this the
# pipe decodes it as cp1252 and every comparison fails on mojibake
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\nous_account.py"
WANTED = ["_normalize_tier", "_is_anonymous_tier", "nous_policy_notice"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    # module-level constants the live bodies read
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "_normalize_tier":
            want = ns["_normalize_tier"](c["in"] if c["in"] is not None else 42)
            got = c.get("out")
            # Python returns "" for a non-string; the C port returns NULL
            want = want if isinstance(want, str) else None
            if got != want:
                mism += 1
                print(f"MISMATCH _normalize_tier in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_is_anonymous_tier":
            # the live fn takes an account snapshot; drive it with a stand-in
            class _Info:
                def __init__(self, flag): self.is_anonymous_tier = flag
            info = _Info(bool(c["flag"])) if c["has"] else None
            want = bool(ns["_is_anonymous_tier"](info))
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _is_anonymous_tier has={c['has']} flag={c['flag']} C={got!r} PY={want!r}")

        elif fn == "nous_policy_notice":
            # policy_present is passed as a fact; the live fn looks it up via
            # nous_policy_present(), so patch that to the emitted value.
            ns["nous_policy_present"] = lambda: {1: True, 0: False, -1: None}[c["present"]]
            want = ns["nous_policy_notice"](removed=bool(c["removed"]))
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH nous_policy_notice removed={c['removed']} present={c['present']} C={got!r} PY={want!r}")

    print(f"ORACLE nous_account: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
