#!/usr/bin/env python3
"""Oracle for port_relaunch.c -- drives the LIVE relaunch.py defs.

`_extract_inherited_flags` is ast-extracted together with
`PRE_ARGPARSE_INHERITED_FLAGS` from `hermes_cli._parser` (the module-level
`_INHERITED_FLAGS_TABLE` is built by introspecting argparse, which cannot be
imported here; the static half is the part the C port implements).
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\relaunch.py"
PARSER_SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\_parser.py"

PRE = [("--profile", True), ("-p", True)]


def extract():
    ns = {}
    # the static pre-argparse table the C port is built on
    src = open(PARSER_SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.AnnAssign) and getattr(node.target, "id", "") == "PRE_ARGPARSE_INHERITED_FLAGS":
            try:
                ns["PRE_ARGPARSE_INHERITED_FLAGS"] = ast.literal_eval(node.value)
            except Exception:
                pass
    if "PRE_ARGPARSE_INHERITED_FLAGS" not in ns:
        ns["PRE_ARGPARSE_INHERITED_FLAGS"] = PRE

    # the two defs, with the module-level table patched to the static half
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    keep = [n for n in tree.body
            if isinstance(n, ast.FunctionDef) and n.name in
            ("_extract_inherited_flags", "build_relaunch_argv")]
    mod = ast.Module(body=keep, type_ignores=[])
    exec(compile(mod, SRC, "exec"), ns)
    ns["_INHERITED_FLAGS_TABLE"] = list(ns["PRE_ARGPARSE_INHERITED_FLAGS"])
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    extracts = {
        "basic": ["--profile", "work", "sessions", "browse"],
        "equals": ["--profile=work", "sessions"],
        "short": ["-p", "work", "--dev"],
        "trailing-no-value": ["--profile"],
        "next-starts-dash": ["--profile", "-tui"],
        "none": ["sessions", "browse"],
        "repeated": ["-p", "work", "-p", "home"],
        "mixed": ["--dev", "--profile", "x", "--profile=y"],
        "empty": [],
    }
    builds = {
        "bin+inherited": ("C:/hermes/hermes.exe", None, ["--profile", "work", "--dev"], True, ["--yes"]),
        "bin+no-inherit": ("C:/hermes/hermes.exe", None, ["--profile", "work", "--dev"], False, ["--yes"]),
        "bin-no-extra": ("C:/hermes/hermes.exe", None, ["--profile", "work", "--dev"], True, []),
        "python-fallback": (None, "C:/py/python.exe", ["--profile", "work", "--dev"], True, ["--yes"]),
        "python-no-inherit": (None, "C:/py/python.exe", ["--profile", "work", "--dev"], False, ["--yes"]),
        "bin-no-orig": ("C:/hermes/hermes.exe", None, [], True, ["--yes"]),
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

        if fn == "_extract_inherited_flags":
            want = ns["_extract_inherited_flags"](extracts.get(c.get("case"), []))
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH _extract_inherited_flags case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "build_relaunch_argv":
            binp, py, orig, preserve, extra = builds.get(c.get("case"), (None, None, [], True, []))
            ns["resolve_hermes_bin"] = lambda: binp
            ns["sys"] = type("S", (), {"executable": py or "python"})()
            want = ns["build_relaunch_argv"](extra, preserve_inherited=preserve, original_argv=orig)
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH build_relaunch_argv case={c.get('case')} C={got!r} PY={want!r}")

    print(f"ORACLE relaunch: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
