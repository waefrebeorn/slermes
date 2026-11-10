#!/usr/bin/env python3
"""Oracle for port_codex_models.c -- drives the LIVE codex_models.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.codex_models pulls in route_identity/logging.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\codex_models.py"
WANTED = ["_dedupe", "_add_forward_compat_models"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    # module constants first (the template table)
    for node in ast.parse(src).body:
        if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
        elif isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # The template table is an AnnAssign whose annotation makes ast-exec fragile;
    # parse the literal straight out of the source instead.
    if not ns.get("_FORWARD_COMPAT_TEMPLATE_MODELS"):
        m = re.search(r"_FORWARD_COMPAT_TEMPLATE_MODELS[^=]*=\s*(\[.*?\])\s*\n\s*\ndef ",
                      src, re.S)
        if m:
            ns["_FORWARD_COMPAT_TEMPLATE_MODELS"] = ast.literal_eval(m.group(1))
    if not ns.get("_FORWARD_COMPAT_TEMPLATE_MODELS"):
        ns["_FORWARD_COMPAT_TEMPLATE_MODELS"] = [
            ("gpt-6-sol", ("gpt-5.6-sol", "gpt-5.5")),
            ("gpt-6-luna", ("gpt-5.6-luna", "gpt-5.5")),
            ("gpt-5.6-sol", ("gpt-5.5", "gpt-5.4")),
            ("gpt-5.6-terra", ("gpt-5.5", "gpt-5.4")),
            ("gpt-5.6-luna", ("gpt-5.5", "gpt-5.4")),
            ("gpt-5.5", ("gpt-5.4", "gpt-5.4-mini")),
            ("gpt-5.3-codex-spark", ("gpt-5.4", "gpt-5.5")),
        ]
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    sets = {
        "empty": [],
        "one": ["gpt-5.5"],
        "one-template": ["gpt-5.5"],
        "dupes": ["gpt-5.5", "gpt-5.4", "gpt-5.5", "gpt-5.4"],
        "chain": ["gpt-5.4", "gpt-5.4-mini"],
        "spark": ["gpt-5.4"],
        "already-present": ["gpt-6-sol", "gpt-5.5"],
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
        ids = sets.get(c.get("case"), [])
        cases += 1
        want = ns[fn](ids)
        got = c.get("out") or []
        if got != want:
            mism += 1
            print(f"MISMATCH {fn} case={c.get('case')} C={got!r} PY={want!r}")

    print(f"ORACLE codex_models: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
