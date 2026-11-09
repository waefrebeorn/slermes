#!/usr/bin/env python3
"""Oracle for port_skill_commands.c -- drives the LIVE agent/skill_commands.py
defs. The module's scan/cache half is not imported; the pure defs are
ast-extracted together with the module constants they read.
"""
import ast
import json
import sys
import re

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\agent\skill_commands.py"
UTILS = r"C:\Users\eman5\hermes-agent\agent\skill_utils.py"
WANTED = ["slugify_skill_name", "command_snapshot", "diff_command_snapshots",
          "resolve_slash_key", "_setup_note", "format_missing_skills"]


def extract():
    ns = {"re": re, "Mapping": dict, "Any": object}
    for path in (SRC, UTILS):
        src = open(path, encoding="utf-8").read()
        tree = ast.parse(src)
        for node in tree.body:
            if isinstance(node, ast.Assign):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
            elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
    src = open(SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    slugs = ["Foo Bar", "foo_bar", "  multiple   spaces  ", "UPPER", "a--b---c",
             "-leading-trailing-", "with+plus/slash", "café", "", None,
             "dots.and,commas"]
    tables = {
        "exact": ["/work", "/status", "/model"],
        "lower": ["/work", "/status", "/model"],
        "underscore": ["/foo-bar"],
        "missing": ["/work", "/status", "/model"],
        "empty": ["/work", "/status", "/model"],
        "null": ["/work", "/status", "/model"],
    }
    resolves = {
        "exact": "work", "lower": "WORK", "underscore": "foo_bar",
        "missing": "nope", "empty": "", "null": None,
    }
    setups = {
        "skipped": (1, "hint", 1, "note"),
        "hint": (0, "gateway says", 0, None),
        "hint-empty": (0, "", 1, "setup needed"),
        "needed-note": (0, None, 1, "run setup"),
        "needed-empty": (0, None, 1, ""),
        "none": (0, None, 0, "unused"),
    }
    missing = {
        "all-unknown": ["alpha", "beta"],
        "one-ambiguous": ["Ambiguous skill name X: use one of a, b"],
        "mixed": ["Ambiguous skill name X: use one of a, b", "alpha", "beta"],
        "empty": [],
        "two-ambiguous": ["Ambiguous skill name Y: pick one",
                          "Ambiguous skill name Z: pick one", "gamma"],
    }
    diffs = {
        "mixed": ({"alpha": "A", "beta": "B", "gone": "G"},
                  {"alpha": "A2", "beta": "B", "new": "N"}),
        "identical": ({"x": "1"}, {"x": "1"}),
        "all-added": ({}, {"only": "O"}),
        "all-removed": ({"only": "O"}, {}),
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
        cases += 1

        if fn == "slugify_skill_name":
            want = ns["slugify_skill_name"](c["in"]) if c["in"] is not None else ns["slugify_skill_name"]("")
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH slugify in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "resolve_slash_key":
            label = c.get("case")
            cmd = resolves.get(label)
            table = tables.get(label, [])
            want = ns["resolve_slash_key"](cmd, table) if cmd else None
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH resolve case={label} C={got!r} PY={want!r}")

        elif fn == "_setup_note":
            label = c.get("case")
            sk, hint, nd, note = setups.get(label, (0, None, 0, None))
            payload = {}
            if sk: payload["setup_skipped"] = True
            if hint is not None: payload["gateway_setup_hint"] = hint
            if nd: payload["setup_needed"] = True
            if note is not None: payload["setup_note"] = note
            want = ns["_setup_note"](payload)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _setup_note case={label} C={got!r} PY={want!r}")

        elif fn == "format_missing_skills":
            want = ns["format_missing_skills"](missing.get(c.get("case"), []))
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH format_missing case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "diff_command_snapshots":
            before, after = diffs.get(c.get("case"), ({}, {}))
            want = ns["diff_command_snapshots"](before, after)
            got = c.get("out")
            try:
                got_obj = json.loads(got) if got else None
            except json.JSONDecodeError:
                got_obj = got
            if got_obj != want:
                mism += 1
                print(f"MISMATCH diff case={c.get('case')}\n  C={json.dumps(got_obj, sort_keys=True)[:220]!r}"
                      f"\n  PY={json.dumps(want, sort_keys=True)[:220]!r}")

    print(f"ORACLE skill_commands: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
